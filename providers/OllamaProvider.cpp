// providers/OllamaProvider.cpp
#include "OllamaProvider.h"
#include <NetEndpoint.h>
#include <NetBuffer.h>
#include <App.h>
#include <json.hpp>
#include <stdlib.h>
#include <stdio.h>
#include "SettingsManager.h"
#include "ChatMessage.h"

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
    , fModels(10, true)
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

    // Build the request body as JSON
    using json = nlohmann::json;

    json requestBody;
    requestBody["model"] = model.String();
    requestBody["prompt"] = threadData->message.String();
    requestBody["stream"] = false;

    // Build messages array from history for chat completion
    json messages = json::array();

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
        messages.push_back(messageObj);
    }

    // Add current message
    json userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = threadData->message.String();
    messages.push_back(userMsg);

    requestBody["messages"] = messages;

    // Convert to string
    BString requestBodyStr = requestBody.dump().c_str();

    // Parse API base to extract host, port, and path
    BString host, path;
    int port = 80;  // Default HTTP port for Ollama

    if (apiBase.StartsWith("https://")) {
        host = apiBase.String() + 8;  // Skip "https://"
        port = 443;  // HTTPS port
    } else if (apiBase.StartsWith("http://")) {
        host = apiBase.String() + 7;  // Skip "http://"
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

    completionPath << "/api/chat";

    // Create socket connection
    BNetEndpoint socket;
    status_t status = socket.Connect(host, port);

    if (status != B_OK) {
        // Connection failed
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        BString errorStr = "Error connecting to Ollama: ";
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
    httpRequest << "Content-Length: " << requestBodyStr.Length() << "\r\n";
    httpRequest << "Connection: close\r\n";
    httpRequest << "\r\n";
    httpRequest << requestBodyStr;

    // Send request
    socket.Send(httpRequest.String(), httpRequest.Length());

    // Receive response
    BNetBuffer buffer;
    status = socket.Receive(buffer);

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
        errorMsg.AddString("content", "Error: Invalid response format from Ollama");
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
        BString errorStr = "Ollama Error (";
        errorStr << statusCode << "): " << body;
        errorMsg.AddString("content", errorStr);
        messenger->SendMessage(&errorMsg);

        socket.Close();
        delete threadData;
        return -1;
    }

    // Parse JSON response
    try {
        json responseJson = json::parse(body.String());

        // Extract completion text
        BString completionText;

        if (responseJson.contains("message") &&
            responseJson["message"].contains("content")) {

            completionText = responseJson["message"]["content"].get<std::string>().c_str();

            // Get token usage if available
            int32 inputTokens = 0, outputTokens = 0;

            if (responseJson.contains("prompt_eval_count")) {
                inputTokens = responseJson["prompt_eval_count"].get<int>();
            }

            if (responseJson.contains("eval_count")) {
                outputTokens = responseJson["eval_count"].get<int>();
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
            errorMsg.AddString("content", "Error: Invalid or incomplete response from Ollama");
            messenger->SendMessage(&errorMsg);
        }
    } catch (const std::exception& e) {
        // JSON parsing error
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        BString errorStr = "Error parsing Ollama response: ";
        errorStr << e.what();
        errorMsg.AddString("content", errorStr);
        messenger->SendMessage(&errorMsg);
    }

    socket.Close();
    delete threadData;
    return 0;
}