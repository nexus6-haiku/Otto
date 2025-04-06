// providers/OpenAIProvider.cpp
#include "OpenAIProvider.h"
#include <Application.h>
#include <HttpFields.h>
#include <HttpSession.h>
#include <HttpRequest.h>
#include <HttpResult.h>
#include <Url.h>

#include <memory>
#include <stdlib.h>

#include "external/json.hpp"
#include "SettingsManager.h"
#include "ChatMessage.h"
#include "ChatView.h"

using namespace BPrivate::Network;

// Request thread struct
struct RequestThreadData {
    BObjectList<ChatMessage> history;
    BString message;
    BString apiKey;
    BString apiBase;
    BString model;
    BMessenger* messenger;
    bool* cancelFlag;
};

OpenAIProvider::OpenAIProvider()
    : LLMProvider("OpenAI")
    , fModels(10)
    , fRequestThread(-1)
    , fCancelRequested(false)
{
    // Set default API base
    fApiBase = "https://api.openai.com/v1";

    // Init models
    _InitModels();
}

OpenAIProvider::~OpenAIProvider()
{
    CancelRequest();
}

void OpenAIProvider::_InitModels()
{
    // Add supported models
    LLMModel* gpt4o = new LLMModel("gpt-4o", "GPT-4o");
    gpt4o->SetContextWindow(128000);
    gpt4o->SetMaxTokens(4096);
    gpt4o->SetSupportsVision(true);
    gpt4o->SetSupportsTools(true);
    fModels.AddItem(gpt4o);

    LLMModel* gpt4 = new LLMModel("gpt-4", "GPT-4");
    gpt4->SetContextWindow(8192);
    gpt4->SetMaxTokens(4096);
    gpt4->SetSupportsTools(true);
    fModels.AddItem(gpt4);

    LLMModel* gpt35 = new LLMModel("gpt-3.5-turbo", "GPT-3.5 Turbo");
    gpt35->SetContextWindow(16385);
    gpt35->SetMaxTokens(4096);
    gpt35->SetSupportsTools(true);
    fModels.AddItem(gpt35);
}

BObjectList<LLMModel>* OpenAIProvider::GetModels()
{
    return &fModels;
}

void OpenAIProvider::SendMessage(const BObjectList<ChatMessage>& history,
                               const BString& message,
                               BMessenger* messenger)
{
    // Cancel any existing request
    CancelRequest();

    // Reset cancel flag
    fCancelRequested = false;

    // Get API key and model from settings
    SettingsManager* settings = SettingsManager::GetInstance();
    BString apiKey = settings->GetApiKey("OpenAI");
    BString apiBase = settings->GetApiBase("OpenAI");
    BString model = settings->GetDefaultModel("OpenAI");

    if (apiKey.IsEmpty()) {
        // Notify about missing API key
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Please set an OpenAI API key in settings.");
        messenger->SendMessage(&errorMsg);
        return;
    }

    if (apiBase.IsEmpty()) {
        apiBase = fApiBase;
    }

    if (model.IsEmpty()) {
        model = "gpt-3.5-turbo";  // Default model
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
    threadData->apiKey = apiKey;
    threadData->apiBase = apiBase;
    threadData->model = model;
    threadData->messenger = messenger;
    threadData->cancelFlag = &fCancelRequested;

    // Start request thread
    fRequestThread = spawn_thread(_RequestThreadFunc, "OpenAI Request",
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

void OpenAIProvider::CancelRequest()
{
    if (fRequestThread >= 0) {
        fCancelRequested = true;

        // Wait for thread to terminate
        status_t result;
        wait_for_thread(fRequestThread, &result);

        fRequestThread = -1;
    }
}

int32 OpenAIProvider::_RequestThreadFunc(void* data)
{
    RequestThreadData* threadData = static_cast<RequestThreadData*>(data);

    // Convenience aliases
    const BString& apiKey = threadData->apiKey;
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

    // Set additional parameters
    requestBody["temperature"] = 0.7;
    requestBody["max_tokens"] = 1000;

    // Convert JSON to string
    std::string requestBodyStr = requestBody.dump();

    // Create URL
    BUrl url(apiBase.String());
    url.SetPath("/chat/completions");

    // Prepare HTTP request
    BHttpRequest request(url);
    request.SetMethod(BHttpMethod::Post);

    // Set headers
    BHttpFields fields;
    fields.AddField("Content-Type", "application/json");
    fields.AddField(std::string_view("Authorization"), std::string_view("Bearer " + std::string(apiKey.String())));
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
        if (responseJson.contains("choices") &&
            !responseJson["choices"].empty() &&
            responseJson["choices"][0].contains("message") &&
            responseJson["choices"][0]["message"].contains("content")) {

            completionText = responseJson["choices"][0]["message"]["content"].get<std::string>().c_str();

            if (responseJson.contains("usage")) {
                inputTokens = responseJson["usage"]["prompt_tokens"].get<int>();
                outputTokens = responseJson["usage"]["completion_tokens"].get<int>();
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