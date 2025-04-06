// providers/OllamaProvider.cpp
#include "OllamaProvider.h"

#include <HttpSession.h>
#include <HttpRequest.h>
#include <HttpResult.h>
#include <HttpFields.h>
#include <Url.h>

#include <memory>
#include <stdlib.h>

#include "ChatView.h"
#include "SettingsManager.h"
#include "external/json.hpp"
#include "SettingsManager.h"
#include "ChatMessage.h"
#include "ChatView.h"

using namespace BPrivate::Network;

// Request thread struct
struct RequestThreadData {
    BObjectList<ChatMessage, true> history;
    BString message;
    BString apiBase;
    BString model;
    BMessenger* messenger;
    bool* cancelFlag;
};

OllamaProvider::OllamaProvider()
    : LLMProvider("Ollama")
    , fModels(10)
    , fRequestThread(-1)
    , fCancelRequested(false)
{
    // Set default API base
    fApiBase = "http://localhost:11434";

    // Init models
    _InitModels();
}

OllamaProvider::~OllamaProvider()
{
    CancelRequest();
}

void OllamaProvider::_InitModels()
{
    // Add some default models (user should fetch actual models)
    LLMModel* llama3 = new LLMModel("llama3", "Llama 3");
    llama3->SetContextWindow(4096);
    llama3->SetMaxTokens(2048);
    fModels.AddItem(llama3);

    LLMModel* mistral = new LLMModel("mistral", "Mistral");
    mistral->SetContextWindow(4096);
    mistral->SetMaxTokens(2048);
    fModels.AddItem(mistral);

    // In a real app, we would fetch available models from Ollama
    FetchAvailableModels();
}

status_t OllamaProvider::FetchAvailableModels()
{
    // In a real app, this would make a request to Ollama to get available models
    // For now, we'll just return success
    return B_OK;
}

BObjectList<LLMModel>* OllamaProvider::GetModels()
{
    return &fModels;
}

void OllamaProvider::SendMessage(const BObjectList<ChatMessage, true>& history,
                             const BString& message,
                             BMessenger* messenger)
{
    // Cancel any existing request
    CancelRequest();

    // Reset cancel flag
    fCancelRequested = false;

    // Get API base and model from settings
    SettingsManager* settings = SettingsManager::GetInstance();
    BString apiBase = settings->GetApiBase("Ollama");
    BString model = settings->GetDefaultModel("Ollama");

    if (apiBase.IsEmpty()) {
        apiBase = fApiBase;
    }

    if (model.IsEmpty()) {
        model = "llama3";  // Default model
    }

    // Prepare thread data
    RequestThreadData* threadData = new RequestThreadData();

    // Copy history
    for (int32 i = 0; i < history.CountItems(); i++) {
        ChatMessage* msg = new ChatMessage(
            history.ItemAt(i)->Content(),
            history.ItemAt(i)->Role()
        );
        threadData->history.AddItem(msg);
    }

    threadData->message = message;
    threadData->apiBase = apiBase;
    threadData->model = model;
    threadData->messenger = messenger;
    threadData->cancelFlag = &fCancelRequested;

    // Start request thread
    fRequestThread = spawn_thread(_RequestThreadFunc, "Ollama Request",
                                B_NORMAL_PRIORITY, threadData);

    if (fRequestThread >= 0) {
        resume_thread(fRequestThread);
    } else {
        // Thread creation failed
        delete threadData;

        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Failed to create request thread.");
        messenger->SendMessage(&errorMsg);
    }
}

void OllamaProvider::CancelRequest()
{
    if (fRequestThread >= 0) {
        fCancelRequested = true;

        // Wait for thread to terminate
        status_t result;
        wait_for_thread(fRequestThread, &result);

        fRequestThread = -1;
    }
}

int32 OllamaProvider::_RequestThreadFunc(void* data)
{
    RequestThreadData* threadData = static_cast<RequestThreadData*>(data);

    // Convenience aliases
    const BString& apiBase = threadData->apiBase;
    const BString& model = threadData->model;
    BMessenger* messenger = threadData->messenger;
    bool* cancelFlag = threadData->cancelFlag;

    // Prepare request body
    using json = nlohmann::json;
    json requestBody;
    requestBody["model"] = model.String();
    requestBody["messages"] = json::array();

    // Add history messages
    for (int32 i = 0; i < threadData->history.CountItems(); i++) {
        ChatMessage* msg = threadData->history.ItemAt(i);

        json messageObj;

        switch (msg->Role()) {
            case MESSAGE_ROLE_USER:
                messageObj["role"] = "user";
                break;
            case MESSAGE_ROLE_ASSISTANT:
                messageObj["role"] = "assistant";
                break;
            case MESSAGE_ROLE_SYSTEM:
                messageObj["role"] = "system";
                break;
        }

        messageObj["content"] = msg->Content().String();
        requestBody["messages"].push_back(messageObj);
    }

    // Add current message
    json userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = threadData->message.String();
    requestBody["messages"].push_back(userMsg);

    // Convert JSON to string
    std::string requestBodyStr = requestBody.dump();

    // Create URL
    BUrl url(apiBase.String());
    url.SetPath("/api/chat");

    // Prepare HTTP request
    BHttpRequest request(url);
    request.SetMethod(BHttpMethod::Post);

    // Set headers
    BPrivate::Network::BHttpFields fields;
    fields.AddField(std::string_view("Content-Type"), std::string_view("application/json"));
    request.SetFields(fields);

    // Set request body
    auto bodyInput = std::make_unique<BMallocIO>();
    bodyInput->Write(requestBodyStr.c_str(), requestBodyStr.length());
    bodyInput->Seek(0, SEEK_SET);
    request.SetRequestBody(std::move(bodyInput), "application/json", requestBodyStr.length());

    // Execute request
    BHttpSession session;
    BHttpResult result = session.Execute(std::move(request));

    // Check result
    const BHttpStatus& status = result.Status();
    if (status.code != 200) {
        // Handle error
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", BString("HTTP Error: ") << status.code);
        messenger->SendMessage(&errorMsg);
        return -1;
    }

    // Parse response
    try {
        std::string responseStr = result.Body().text.value().String();
        json responseJson = json::parse(responseStr);

        BString completionText;
        int32 inputTokens = 0, outputTokens = 0;

        // Extract completion text and token usage
        if (responseJson.contains("message") &&
            responseJson["message"].contains("content")) {

            completionText = responseJson["message"]["content"].get<std::string>().c_str();

            // Ollama provides different token counting
            if (responseJson.contains("prompt_eval_count")) {
                inputTokens = responseJson["prompt_eval_count"].get<int>();
            }

            if (responseJson.contains("eval_count")) {
                outputTokens = responseJson["eval_count"].get<int>();
            }

            // Send response
            BMessage responseMsg(MSG_MESSAGE_RECEIVED);
            responseMsg.AddString("content", completionText);
            responseMsg.AddInt32("input_tokens", inputTokens);
            responseMsg.AddInt32("output_tokens", outputTokens);
            messenger->SendMessage(&responseMsg);
        }
    } catch (const std::exception& e) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", BString("JSON parsing error: ") << e.what());
        messenger->SendMessage(&errorMsg);
    }

    return 0;
}