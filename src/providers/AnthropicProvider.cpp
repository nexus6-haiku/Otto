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
#include <Path.h>
#include <File.h>
#include <FindDirectory.h>
#include <OS.h>
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

    // Strip trailing slash and path if present in the base URL
    if (apiBase.FindLast("/v1") != B_ERROR) {
        int32 pathPos = apiBase.FindLast("/v1");
        apiBase.Truncate(pathPos + 3); // Keep up to "/v1"
    }
    printf("Using API base: %s\n", apiBase.String());

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

    // Create a temporary file for the response
    BPath tempPath;
    find_directory(B_SYSTEM_TEMP_DIRECTORY, &tempPath);
    tempPath.Append("otto_response.json");

    // Sanitize the request body for command line usage
    std::string sanitizedRequest = requestBodyStr;
    // Replace single quotes with escaped single quotes for shell command
    size_t pos = 0;
    while ((pos = sanitizedRequest.find("'", pos)) != std::string::npos) {
        sanitizedRequest.replace(pos, 1, "'\\''");
        pos += 4; // Move past the replacement
    }

    // Build the curl command
    BString curlCmd;
    curlCmd << "curl -s -X POST ";
    curlCmd << "-H \"Content-Type: application/json\" ";
    curlCmd << "-H \"x-api-key: " << apiKey << "\" ";
    curlCmd << "-H \"anthropic-version: 2023-06-01\" ";
    curlCmd << "-d '" << sanitizedRequest.c_str() << "' ";
    curlCmd << "\"" << apiBase << "/messages\" ";
    curlCmd << "> " << tempPath.Path();

    // Print debug info
    printf("Executing curl command:\n%s\n", curlCmd.String());
    printf("API Base: %s, Model: %s\n", apiBase.String(), model.String());

    // Execute the command
    int result = system(curlCmd.String());
    if (result != 0) {
        printf("Error executing curl command: %d\n", result);
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", BString("Error: Failed to execute API request. Check that curl is installed."));
        messenger->SendMessage(&errorMsg);
        return -1;
    }

    // Read the response from the temp file
    BFile responseFile(tempPath.Path(), B_READ_ONLY);
    if (responseFile.InitCheck() != B_OK) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Failed to read API response");
        messenger->SendMessage(&errorMsg);
        return -1;
    }

    // Get file size
    off_t fileSize;
    responseFile.GetSize(&fileSize);
    if (fileSize == 0) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Empty response from API");
        messenger->SendMessage(&errorMsg);
        return -1;
    }
    printf("Response file size: %lld bytes\n", fileSize);

    // Read content
    char* buffer = new char[fileSize + 1];
    responseFile.Read(buffer, fileSize);
    buffer[fileSize] = '\0';

    // Parse response
    try {
        std::string responseStr(buffer);
        json responseJson = json::parse(responseStr);

        BString completionText;
        int32 inputTokens = 0, outputTokens = 0;

        printf("Successfully parsed JSON response\n");

        // Extract completion text and token usage
        if (responseJson.contains("content") &&
            !responseJson["content"].empty() &&
            responseJson["content"][0].contains("text")) {
            printf("Found content in response\n");

            completionText = responseJson["content"][0]["text"].get<std::string>().c_str();
            printf("Completion text length: %ld\n", completionText.Length());

            if (responseJson.contains("usage")) {
                inputTokens = responseJson["usage"]["input_tokens"].get<int>();
                outputTokens = responseJson["usage"]["output_tokens"].get<int>();
            }
            printf("Tokens - Input: %d, Output: %d\n", inputTokens, outputTokens);

            // Send response
            BMessage responseMsg(MSG_MESSAGE_RECEIVED);
            responseMsg.AddString("content", completionText);
            responseMsg.AddInt32("input_tokens", inputTokens);
            responseMsg.AddInt32("output_tokens", outputTokens);
            messenger->SendMessage(&responseMsg);
            printf("Response message sent via messenger\n");
        }
        else {
            printf("Content structure not found in response\n");
            printf("Raw response: %s\n", buffer);

            // Send an error message to the UI
            BMessage errorMsg(MSG_MESSAGE_RECEIVED);
            errorMsg.AddString("content", "Error: Unexpected API response format. Check console for details.");
            messenger->SendMessage(&errorMsg);
        }
    } catch (const std::exception& e) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);

        // Print the raw response for debugging
        printf("JSON parsing error: %s\n", e.what());
        printf("Raw response: %s\n", buffer);
        messenger->SendMessage(&errorMsg);
    }

    // Delete buffer
    delete[] buffer;

    // Optionally remove the temporary file
    BEntry tempEntry(tempPath.Path());
    tempEntry.Remove();

    return 0;
}