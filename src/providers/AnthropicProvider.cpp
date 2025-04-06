// providers/AnthropicProvider.cpp
#include "LLMProvider.h"
#include "AnthropicProvider.h"
#include <NetEndpoint.h>
#include <NetBuffer.h>
#include <Application.h>
#include "external/json.hpp"
#include <stdlib.h>
#include "SettingsManager.h"
#include "ChatMessage.h"
#include "ChatView.h"

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

    // Build the request body as JSON
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

    // Set other parameters
    requestBody["temperature"] = 0.7;
    requestBody["max_tokens"] = 1000;
    requestBody["stream"] = false;  // Not using streaming for simplicity

    // Convert to string
    BString requestBodyStr = requestBody.dump().c_str();

    // Parse API base to extract host, port, and path
    BString host, path;
    int port = 443;  // Default HTTPS port

    if (apiBase.StartsWith("https://")) {
        host = apiBase.String() + 8;  // Skip "https://"
    } else if (apiBase.StartsWith("http://")) {
        host = apiBase.String() + 7;  // Skip "http://"
        port = 80;  // HTTP port
    } else {
        host = apiBase;
    }

    // Extract host and path
    int32 slashPos = host.FindFirst('/');
    if (slashPos >= 0) {
        path = host.String() + slashPos;
        host.Truncate(slashPos);
    } else {
        path = "/";
    }

    // Extract port if specified
    int32 colonPos = host.FindFirst(':');
    if (colonPos >= 0) {
        BString portStr = host.String() + colonPos + 1;
        host.Truncate(colonPos);
        port = atoi(portStr.String());
    }

    // Prepare endpoint path for completion
    BString completionPath = path;
    if (completionPath.EndsWith("/"))
        completionPath.RemoveLast("/");

    completionPath << "/messages";

    // Create socket connection
    BNetEndpoint socket;
    status_t status = socket.Connect(host, port);

    if (status != B_OK) {
        // Connection failed
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        BString errorStr = "Error connecting to API: ";
        errorStr << strerror(status);
        errorMsg.AddString("content", errorStr);
        messenger->SendMessage(&errorMsg);

        delete threadData;
        return -1;
    }

    // Build HTTP request
    BString httpRequest;
    httpRequest << "POST " << completionPath << " HTTP/1.1\r\n";
    httpRequest << "Host: " << host << "\r\n";
    httpRequest << "Content-Type: application/json\r\n";
    httpRequest << "x-api-key: " << apiKey << "\r\n";
    httpRequest << "anthropic-version: 2023-06-01\r\n";  // Required header for Anthropic
    httpRequest << "Content-Length: " << requestBodyStr.Length() << "\r\n";
    httpRequest << "Connection: close\r\n";
    httpRequest << "\r\n";
    httpRequest << requestBodyStr;

    // Send request
    socket.Send(httpRequest.String(), httpRequest.Length());

    // Receive response
    BNetBuffer buffer;
    status = socket.Receive(buffer, buffer.Size());

    if (status != B_OK || *cancelFlag) {
        // Connection error or canceled
        if (!*cancelFlag) {
            BMessage errorMsg(MSG_MESSAGE_RECEIVED);
            BString errorStr = "Error receiving response: ";
            errorStr << strerror(status);
            errorMsg.AddString("content", errorStr);
            messenger->SendMessage(&errorMsg);
        }

        socket.Close();
        delete threadData;
        return -1;
    }

    // Parse HTTP response
    BString response((const char*)buffer.Data(), buffer.Size());

    // Extract status code
    int statusCode = 0;
    if (response.StartsWith("HTTP/1.1 ")) {
        BString statusStr = response.String() + 9;
        statusCode = atoi(statusStr.String());
    }

    // Find the response body (after double newline)
    int32 bodyPos = response.FindFirst("\r\n\r\n");
    if (bodyPos < 0) {
        // Invalid response format
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Invalid response format from API");
        messenger->SendMessage(&errorMsg);

        socket.Close();
        delete threadData;
        return -1;
    }

    // Extract the body
    BString body = response.String() + bodyPos + 4;

    if (statusCode != 200) {
        // API error
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        BString errorStr = "API Error (";
        errorStr << statusCode << "): " << body;
        errorMsg.AddString("content", errorStr);
        messenger->SendMessage(&errorMsg);

        socket.Close();
        delete threadData;
        return -1;
    }

    // Parse JSON response - Anthropic format is different from OpenAI
    try {
        json responseJson = json::parse(body.String());

        // Extract completion text
        BString completionText;

        if (responseJson.contains("content") &&
            responseJson["content"].size() > 0 &&
            responseJson["content"][0].contains("text")) {

            completionText = responseJson["content"][0]["text"].get<std::string>().c_str();

            // Get token usage if available
            int32 inputTokens = 0, outputTokens = 0;

            if (responseJson.contains("usage")) {
                if (responseJson["usage"].contains("input_tokens")) {
                    inputTokens = responseJson["usage"]["input_tokens"].get<int>();
                }

                if (responseJson["usage"].contains("output_tokens")) {
                    outputTokens = responseJson["usage"]["output_tokens"].get<int>();
                }
            }

            // Send the response message
            BMessage responseMsg(MSG_MESSAGE_RECEIVED);
            responseMsg.AddString("content", completionText);
            responseMsg.AddInt32("input_tokens", inputTokens);
            responseMsg.AddInt32("output_tokens", outputTokens);
            messenger->SendMessage(&responseMsg);
        } else {
            // Incomplete or invalid response
            BMessage errorMsg(MSG_MESSAGE_RECEIVED);
            errorMsg.AddString("content", "Error: Invalid or incomplete response from API");
            messenger->SendMessage(&errorMsg);
        }
    } catch (const std::exception& e) {
        // JSON parsing error
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        BString errorStr = "Error parsing response: ";
        errorStr << e.what();
        errorMsg.AddString("content", errorStr);
        messenger->SendMessage(&errorMsg);
    }

    socket.Close();
    delete threadData;
    return 0;
}