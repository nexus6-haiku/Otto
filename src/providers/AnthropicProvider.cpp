// providers/AnthropicProvider.cpp
#include "LLMProvider.h"
#include "AnthropicProvider.h"

#include <Application.h>
#include <HttpSession.h>
#include <HttpRequest.h>
#include <HttpResult.h>
#include <HttpFields.h>
#include <Url.h>

#include <memory>
#include <stdlib.h>

#include "ChatView.h"
#include "external/json.hpp"
#include "SettingsManager.h"
#include "ChatMessage.h"
#include "ChatView.h"

using namespace BPrivate::Network;

// Request thread struct
struct RequestThreadData {
    BObjectList<ChatMessage, true> history;
    BString message;
    BString apiKey;
    BString apiBase;
    BString model;
    BMessenger* messenger;
    bool* cancelFlag;
};

AnthropicProvider::AnthropicProvider()
    : LLMProvider("Anthropic")
    , fModels(10)
    , fRequestThread(-1)
    , fCancelRequested(false)
{
    // Set default API base
    fApiBase = "https://api.anthropic.com/v1";

    // Init models
    _InitModels();
}

AnthropicProvider::~AnthropicProvider()
{
    CancelRequest();
}

void AnthropicProvider::_InitModels()
{
    // Add supported models
    LLMModel* claude3opus = new LLMModel("claude-3-opus-20240229", "Claude 3 Opus");
    claude3opus->SetContextWindow(200000);
    claude3opus->SetMaxTokens(4096);
    claude3opus->SetSupportsVision(true);
    claude3opus->SetSupportsTools(true);
    fModels.AddItem(claude3opus);

    LLMModel* claude3sonnet = new LLMModel("claude-3-sonnet-20240229", "Claude 3 Sonnet");
    claude3sonnet->SetContextWindow(200000);
    claude3sonnet->SetMaxTokens(4096);
    claude3sonnet->SetSupportsVision(true);
    claude3sonnet->SetSupportsTools(true);
    fModels.AddItem(claude3sonnet);

    LLMModel* claude3haiku = new LLMModel("claude-3-haiku-20240307", "Claude 3 Haiku");
    claude3haiku->SetContextWindow(200000);
    claude3haiku->SetMaxTokens(4096);
    claude3haiku->SetSupportsVision(true);
    claude3haiku->SetSupportsTools(true);
    fModels.AddItem(claude3haiku);
}

BObjectList<LLMModel>* AnthropicProvider::GetModels()
{
    return &fModels;
}

void AnthropicProvider::SendMessage(const BObjectList<ChatMessage, true>& history,
                               const BString& message,
                               BMessenger* messenger)
{
    // Cancel any existing request
    CancelRequest();

    // Reset cancel flag
    fCancelRequested = false;

    // Get API key and model from settings
    SettingsManager* settings = SettingsManager::GetInstance();
    BString apiKey = settings->GetApiKey("Anthropic");
    BString apiBase = settings->GetApiBase("Anthropic");
    BString model = settings->GetDefaultModel("Anthropic");

    if (apiKey.IsEmpty()) {
        // Notify about missing API key
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Please set an Anthropic API key in settings.");
        messenger->SendMessage(&errorMsg);
        return;
    }

    if (apiBase.IsEmpty()) {
        apiBase = fApiBase;
    }

    if (model.IsEmpty()) {
        model = "claude-3-haiku-20240307";  // Default model
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
    fRequestThread = spawn_thread(_RequestThreadFunc, "Anthropic Request",
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

void AnthropicProvider::CancelRequest()
{
    if (fRequestThread >= 0) {
        fCancelRequested = true;

        // Wait for thread to terminate
        status_t result;
        wait_for_thread(fRequestThread, &result);

        fRequestThread = -1;
    }
}

int32 AnthropicProvider::_RequestThreadFunc(void* data)
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
    url.SetPath("/messages");

    // Prepare HTTP request
    BHttpRequest request(url);
    request.SetMethod(BHttpMethod::Post);

    // Set headers
    BPrivate::Network::BHttpFields fields;
    fields.AddField(std::string_view("Content-Type"), std::string_view("application/json"));
    fields.AddField(std::string_view("x-api-key"), std::string_view(apiKey.String()));
    fields.AddField(std::string_view("anthropic-version"), std::string_view("2023-06-01"));
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
        if (responseJson.contains("content") &&
            !responseJson["content"].empty() &&
            responseJson["content"][0].contains("text")) {

            completionText = responseJson["content"][0]["text"].get<std::string>().c_str();

            if (responseJson.contains("usage")) {
                inputTokens = responseJson["usage"]["input_tokens"].get<int>();
                outputTokens = responseJson["usage"]["output_tokens"].get<int>();
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