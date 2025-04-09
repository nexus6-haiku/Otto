// providers/AnthropicProvider.cpp
#include "LLMProvider.h"
#include "AnthropicProvider.h"

#include <Application.h>
#include <HttpSession.h>
#include <HttpRequest.h>
#include <HttpResult.h>
#include <HttpFields.h>
#include <Url.h>
#include <Path.h>
#include <File.h>
#include <FindDirectory.h>
#include <OS.h>
#include <String.h>
#include <ObjectList.h>
#include <Messenger.h>
#include <Entry.h>
#include <DataIO.h>

#include "ChatView.h"
#include "external/json.hpp"
#include "SettingsManager.h"
#include "ChatMessage.h"

using namespace BPrivate::Network;
using json = nlohmann::json;

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
    fApiBase = "https://api.anthropic.com/v1";
    _InitModels();
}

AnthropicProvider::~AnthropicProvider()
{
    CancelRequest();
}

void AnthropicProvider::_InitModels()
{
    // Add Claude 3 Opus
    LLMModel* claude3opus = new LLMModel("claude-3-opus-20240229", "Claude 3 Opus");
    claude3opus->SetContextWindow(200000);
    claude3opus->SetMaxTokens(4096);
    claude3opus->SetSupportsVision(true);
    claude3opus->SetSupportsTools(true);
    fModels.AddItem(claude3opus);

    // Add Claude 3 Sonnet
    LLMModel* claude3sonnet = new LLMModel("claude-3-sonnet-20240229", "Claude 3 Sonnet");
    claude3sonnet->SetContextWindow(200000);
    claude3sonnet->SetMaxTokens(4096);
    claude3sonnet->SetSupportsVision(true);
    claude3sonnet->SetSupportsTools(true);
    fModels.AddItem(claude3sonnet);

    // Add Claude 3 Haiku
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

    // Validate API key
    if (apiKey.IsEmpty()) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Please set an Anthropic API key in settings.");
        messenger->SendMessage(&errorMsg);
        return;
    }

    // Use default API base if not set
    if (apiBase.IsEmpty()) {
        apiBase = fApiBase;
    }

    // Use default model if not set
    if (model.IsEmpty()) {
        model = "claude-3-7-sonnet-latest";
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
        status_t result;
        wait_for_thread(fRequestThread, &result);
        fRequestThread = -1;
    }
}

int32 AnthropicProvider::_RequestThreadFunc(void* data)
{
    RequestThreadData* threadData = static_cast<RequestThreadData*>(data);
    BMessenger* messenger = threadData->messenger;
    bool* cancelFlag = threadData->cancelFlag;
    
    // Create temporary file for the response
    BPath tempPath;
    find_directory(B_SYSTEM_TEMP_DIRECTORY, &tempPath);
    tempPath.Append("otto_response.json");
    
    // Build the JSON request body
    json requestBody;
    requestBody["model"] = threadData->model.String();
    requestBody["messages"] = json::array();
    
    // Variable to store system message if found
    BString systemMessage;
    
    // Process history messages
    for (int32 i = 0; i < threadData->history.CountItems(); i++) {
        ChatMessage* msg = threadData->history.ItemAt(i);
        
        // Extract system message separately
        if (msg->Role() == MESSAGE_ROLE_SYSTEM) {
            systemMessage = msg->Content();
            continue;
        }
        
        json messageObj;
        switch (msg->Role()) {
            case MESSAGE_ROLE_USER:
                messageObj["role"] = "user";
                break;
            case MESSAGE_ROLE_ASSISTANT:
                messageObj["role"] = "assistant";
                break;
        }
        
        messageObj["content"] = msg->Content().String();
        requestBody["messages"].push_back(messageObj);
    }
    
    // Add the new user message
    json userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = threadData->message.String();
    requestBody["messages"].push_back(userMsg);
    
    // Add system message if found
    if (!systemMessage.IsEmpty()) {
        requestBody["system"] = systemMessage.String();
    }
    
    // Set additional parameters
    requestBody["temperature"] = 0.7;
    requestBody["max_tokens"] = 1000;
    
    // Convert JSON to string
    std::string requestBodyStr = requestBody.dump();
    
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
    curlCmd << "-H \"x-api-key: " << threadData->apiKey << "\" ";
    curlCmd << "-H \"anthropic-version: 2023-06-01\" ";
    curlCmd << "-d '" << sanitizedRequest.c_str() << "' ";
    curlCmd << "\"" << threadData->apiBase << "/messages\" ";
    curlCmd << "> " << tempPath.Path();
    
    // Execute the command
    int result = system(curlCmd.String());
    if (result != 0) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", BString("Error: Failed to execute API request. Check that curl is installed."));
        messenger->SendMessage(&errorMsg);
        return B_ERROR;
    }
    
    // Read the response from the temp file
    BFile responseFile(tempPath.Path(), B_READ_ONLY);
    if (responseFile.InitCheck() != B_OK) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Failed to read API response");
        messenger->SendMessage(&errorMsg);
        return B_ERROR;
    }
    
    // Get file size
    off_t fileSize;
    responseFile.GetSize(&fileSize);
    if (fileSize == 0) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", "Error: Empty response from API");
        messenger->SendMessage(&errorMsg);
        return B_ERROR;
    }
    
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
        
        // In a successful response:
        // - "content" is an array of objects
        // - We need the "text" field from the first object with "type":"text"
        if (responseJson.contains("content") &&
            responseJson["content"].is_array() &&
            !responseJson["content"].empty()) {
            
            // Extract text from the first content item of type "text"
            for (const auto& contentItem : responseJson["content"]) {
                if (contentItem.contains("type") &&
                    contentItem["type"] == "text" &&
                    contentItem.contains("text")) {
                    
                    completionText = contentItem["text"].get<std::string>().c_str();
                    break;
                }
            }
            
            // Extract token usage information
            if (responseJson.contains("usage")) {
                if (responseJson["usage"].contains("input_tokens")) {
                    inputTokens = responseJson["usage"]["input_tokens"].get<int>();
                }
                
                if (responseJson["usage"].contains("output_tokens")) {
                    outputTokens = responseJson["usage"]["output_tokens"].get<int>();
                }
            }
            
            // If we found text, send the response
            if (completionText.Length() > 0) {
                BMessage responseMsg(MSG_MESSAGE_RECEIVED);
                responseMsg.AddString("content", completionText);
                responseMsg.AddInt32("input_tokens", inputTokens);
                responseMsg.AddInt32("output_tokens", outputTokens);
                
                if (messenger->IsValid()) {
                    messenger->SendMessage(&responseMsg);
                }
            } else {
                BMessage errorMsg(MSG_MESSAGE_RECEIVED);
                errorMsg.AddString("content", "Error: No text content found in the API response.");
                messenger->SendMessage(&errorMsg);
            }
        }
        else if (responseJson.contains("error")) {
            // Handle API error response
            BString errorMessage = "API Error: ";
            if (responseJson["error"].contains("message")) {
                errorMessage << responseJson["error"]["message"].get<std::string>().c_str();
            } else {
                errorMessage << "Unknown error occurred";
            }
            
            BMessage errorMsg(MSG_MESSAGE_RECEIVED);
            errorMsg.AddString("content", errorMessage);
            messenger->SendMessage(&errorMsg);
        }
        else {
            // Unexpected response format
            BMessage errorMsg(MSG_MESSAGE_RECEIVED);
            errorMsg.AddString("content", "Error: Unexpected API response format.");
            messenger->SendMessage(&errorMsg);
        }
    } catch (const std::exception& e) {
        BMessage errorMsg(MSG_MESSAGE_RECEIVED);
        errorMsg.AddString("content", BString("JSON parsing error: ") << e.what());
        messenger->SendMessage(&errorMsg);
    }
    
    // Delete buffer and remove temp file
    delete[] buffer;
    BEntry tempEntry(tempPath.Path());
    tempEntry.Remove();
    
    return B_OK;
}