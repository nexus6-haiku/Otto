// BFSStorage.cpp
#include "BFSStorage.h"
#include <Entry.h>
#include <Node.h>
#include <File.h>
#include <fs_attr.h>
#include <Directory.h>
#include <NodeInfo.h>
#include <Volume.h>
#include <VolumeRoster.h>
#include <stdlib.h>  // For random number generation
#include <time.h>
#include <stdio.h>
#include <Query.h>
#include <fs_index.h>

BFSStorage* BFSStorage::sInstance = NULL;

BFSStorage* BFSStorage::GetInstance()
{
    if (sInstance == NULL)
        sInstance = new BFSStorage();

    return sInstance;
}

BFSStorage::BFSStorage()
{
    // Initialize random number generator
    srand(time(NULL));

    // Set up base directory in user settings
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &fBasePath) == B_OK) {
        fBasePath.Append("Otto");

        // Set subdirectories
        fChatsDir = fBasePath;
        fChatsDir.Append("Chats");

        fStatsDir = fBasePath;
        fStatsDir.Append("Stats");
    }
}

BFSStorage::~BFSStorage()
{
}

bool BFSStorage::Initialize()
{
    // Create necessary directories
    status_t status;

    status = _EnsureDirectoryExists(fBasePath);
    if (status != B_OK)
        return false;

    status = _EnsureDirectoryExists(fChatsDir);
    if (status != B_OK)
        return false;

    status = _EnsureDirectoryExists(fStatsDir);
    if (status != B_OK)
        return false;

    // Find the volume that contains our storage directory
    BEntry entry(fBasePath.Path());
    BVolume volume;
    if (entry.GetVolume(&volume) != B_OK)
        return false;

    // Create indices
    status = _CreateIndices(volume.Device());
    if (status != B_OK)
        return false;

    return true;
}

status_t BFSStorage::_CreateIndices(dev_t device)
{
    // Create indices for fast queries
    // The fs_create_index only needs to be run once per volume

    // Create chat indices
    fs_create_index(device, ATTR_CHAT_TITLE, B_STRING_TYPE, 0);
    fs_create_index(device, ATTR_CHAT_CREATED, B_TIME_TYPE, 0);
    fs_create_index(device, ATTR_CHAT_UPDATED, B_TIME_TYPE, 0);
    fs_create_index(device, ATTR_CHAT_ID, B_STRING_TYPE, 0);

    // Create message indices
    fs_create_index(device, ATTR_MESSAGE_ROLE, B_INT32_TYPE, 0);
    fs_create_index(device, ATTR_MESSAGE_TIMESTAMP, B_TIME_TYPE, 0);
    fs_create_index(device, ATTR_MESSAGE_ORDER, B_INT32_TYPE, 0);

    // Create usage indices
    fs_create_index(device, ATTR_USAGE_PROVIDER, B_STRING_TYPE, 0);
    fs_create_index(device, ATTR_USAGE_MODEL, B_STRING_TYPE, 0);
    fs_create_index(device, ATTR_USAGE_TIMESTAMP, B_TIME_TYPE, 0);

    return B_OK;
}

status_t BFSStorage::_EnsureDirectoryExists(BPath& path)
{
    BEntry entry(path.Path());
    if (!entry.Exists()) {
        BDirectory dir;
        status_t status = dir.CreateDirectory(path.Path(), &dir);
        if (status != B_OK)
            return status;
    }
    return B_OK;
}

BString BFSStorage::_GenerateUniqueID() const
{
    // Simple unique ID generator - timestamp + random
    char buffer[32];
    time_t now = time(NULL);
    int random = rand() % 10000;  // Random number between 0-9999

    snprintf(buffer, sizeof(buffer), "%ld_%04d", now, random);
    return BString(buffer);
}

status_t BFSStorage::SaveChat(Chat* chat)
{
    if (chat == NULL)
        return B_BAD_VALUE;

    time_t now = time(NULL);

    // Create a filename from the chat title
    BString filename = chat->Title();
    // Replace invalid filename characters
    filename.ReplaceAll("/", "_");
    filename.ReplaceAll(":", "_");

    // Add a unique ID to the filename to avoid conflicts
    BString uniqueID = _GenerateUniqueID();
    filename << "_" << uniqueID;

    BPath chatPath(fChatsDir);
    chatPath.Append(filename);

    // Create empty file to represent the chat
    BFile chatFile(chatPath.Path(), B_CREATE_FILE | B_WRITE_ONLY);
    if (chatFile.InitCheck() != B_OK)
        return chatFile.InitCheck();

    // Set file MIME type
    BNodeInfo nodeInfo(&chatFile);
    nodeInfo.SetType("application/x-vnd.Otto-chat");

    // Add attributes to the file
    chatFile.WriteAttr(ATTR_CHAT_TITLE, B_STRING_TYPE, 0, chat->Title().String(), chat->Title().Length() + 1);

    time_t createdTime = chat->CreatedAt();
    chatFile.WriteAttr(ATTR_CHAT_CREATED, B_TIME_TYPE, 0, &createdTime, sizeof(time_t));

    chatFile.WriteAttr(ATTR_CHAT_UPDATED, B_TIME_TYPE, 0, &now, sizeof(time_t));
    chatFile.WriteAttr(ATTR_CHAT_ID, B_STRING_TYPE, 0, uniqueID.String(), uniqueID.Length() + 1);

    // Close the file
    chatFile.Unset();

    // Get entry_ref for the chat file
    entry_ref chatRef;
    BEntry chatEntry(chatPath.Path());
    chatEntry.GetRef(&chatRef);

    // Save messages
    return _SaveChatMessages(chatRef, chat->Messages());
}

status_t BFSStorage::_SaveChatMessages(const entry_ref& chatRef, const BObjectList<ChatMessage, true>* messages)
{
    if (messages == NULL)
        return B_BAD_VALUE;

    // First, remove any existing message files for this chat
    BString chatDirName(chatRef.name);
    chatDirName << "_messages";

    BPath messagesPath(fChatsDir);
    messagesPath.Append(chatDirName);

    // Check if directory exists
    BEntry messagesEntry(messagesPath.Path());
    if (messagesEntry.Exists()) {
        // Delete existing files
        BDirectory messagesDir(messagesPath.Path());
        messagesDir.Rewind();
        BEntry entry;
        while (messagesDir.GetNextEntry(&entry) == B_OK) {
            entry.Remove();
        }
    } else {
        // Create directory
        BDirectory baseDir(fChatsDir.Path());
        baseDir.CreateDirectory(chatDirName, NULL);
    }

    // Save each message as a separate file
    BString chatID;
    BNode chatNode(&chatRef);
    char buffer[B_PATH_NAME_LENGTH];
    chatNode.ReadAttr(ATTR_CHAT_ID, B_STRING_TYPE, 0, buffer, sizeof(buffer));
    chatID = buffer;

    for (int32 i = 0; i < messages->CountItems(); i++) {
        ChatMessage* message = messages->ItemAt(i);

        // Create message filename
        BString msgFilename;
        msgFilename.SetToFormat("msg_%08" B_PRId32 "_%s", i, _GenerateUniqueID().String());

        BPath msgPath(messagesPath);
        msgPath.Append(msgFilename);

        // Create message file
        BFile msgFile(msgPath.Path(), B_CREATE_FILE | B_WRITE_ONLY);
        if (msgFile.InitCheck() != B_OK)
            continue;

        // Write message content to file
        msgFile.Write(message->Content().String(), message->Content().Length());

        // Set MIME type
        BNodeInfo nodeInfo(&msgFile);
        nodeInfo.SetType("text/plain");

        // Set attributes
        int32 role = message->Role();
        msgFile.WriteAttr(ATTR_MESSAGE_ROLE, B_INT32_TYPE, 0, &role, sizeof(int32));

        time_t timestamp = message->Timestamp();
        msgFile.WriteAttr(ATTR_MESSAGE_TIMESTAMP, B_TIME_TYPE, 0, &timestamp, sizeof(time_t));

        msgFile.WriteAttr(ATTR_CHAT_ID, B_STRING_TYPE, 0, chatID.String(), chatID.Length() + 1);

        int32 inputTokens = message->InputTokens();
        int32 outputTokens = message->OutputTokens();
        msgFile.WriteAttr(ATTR_MESSAGE_TOKENS_IN, B_INT32_TYPE, 0, &inputTokens, sizeof(int32));
        msgFile.WriteAttr(ATTR_MESSAGE_TOKENS_OUT, B_INT32_TYPE, 0, &outputTokens, sizeof(int32));

        // Set message order
        msgFile.WriteAttr(ATTR_MESSAGE_ORDER, B_INT32_TYPE, 0, &i, sizeof(int32));
    }

    return B_OK;
}

status_t BFSStorage::LoadChat(const entry_ref& ref, Chat* chat)
{
    if (chat == NULL)
        return B_BAD_VALUE;

    // Open the chat file node
    BNode node(&ref);
    if (node.InitCheck() != B_OK)
        return node.InitCheck();

    // Read attributes
    char titleBuffer[B_PATH_NAME_LENGTH];
    time_t createdTime, updatedTime;

    node.ReadAttr(ATTR_CHAT_TITLE, B_STRING_TYPE, 0, titleBuffer, sizeof(titleBuffer));
    node.ReadAttr(ATTR_CHAT_CREATED, B_TIME_TYPE, 0, &createdTime, sizeof(time_t));
    node.ReadAttr(ATTR_CHAT_UPDATED, B_TIME_TYPE, 0, &updatedTime, sizeof(time_t));

    // Set chat properties
    chat->SetTitle(titleBuffer);
    chat->SetCreatedAt(createdTime);
    chat->SetUpdatedAt(updatedTime);

    // Load messages
    return _LoadChatMessages(ref, chat->Messages());
}

status_t BFSStorage::_LoadChatMessages(const entry_ref& chatRef, BObjectList<ChatMessage, true>* messages)
{
    // Get chat ID
    BNode chatNode(&chatRef);
    char chatIDBuffer[B_PATH_NAME_LENGTH];
    chatNode.ReadAttr(ATTR_CHAT_ID, B_STRING_TYPE, 0, chatIDBuffer, sizeof(chatIDBuffer));
    BString chatID = chatIDBuffer;

    // Build messages directory path
    BString chatDirName(chatRef.name);
    chatDirName << "_messages";

    BPath messagesPath(fChatsDir);
    messagesPath.Append(chatDirName);

    // Check if directory exists
    BEntry messagesEntry(messagesPath.Path());
    if (!messagesEntry.Exists())
        return B_ENTRY_NOT_FOUND;

    // Use fs_query to find all messages for this chat
    BVolume vol(chatRef.device);
    BQuery query;
    query.SetVolume(&vol);

    // Set up query expression
    // We want: (BEOS:MIME == "text/plain") && (ChatID == ourChatID)
    BString expression;
    expression << "((" ATTR_CHAT_ID " == \"" << chatID.String() << "\")&&(BEOS:MIME==\"text/plain\"))";
    query.SetPredicate(expression.String());

    status_t status = query.Fetch();
    if (status != B_OK)
        return status;

    // Process each message file
    BEntry entry;
    int32 messageCount = 0;
    BMessage orderedMessages;

    while (query.GetNextEntry(&entry) == B_OK) {
        // Get entry ref
        entry_ref msgRef;
        entry.GetRef(&msgRef);

        // Open file
        BFile msgFile(&msgRef, B_READ_ONLY);
        if (msgFile.InitCheck() != B_OK)
            continue;

        // Read attributes
        int32 role = 0;
        time_t timestamp = 0;
        int32 inputTokens = 0;
        int32 outputTokens = 0;
        int32 order = 0;

        msgFile.ReadAttr(ATTR_MESSAGE_ROLE, B_INT32_TYPE, 0, &role, sizeof(int32));
        msgFile.ReadAttr(ATTR_MESSAGE_TIMESTAMP, B_TIME_TYPE, 0, &timestamp, sizeof(time_t));
        msgFile.ReadAttr(ATTR_MESSAGE_TOKENS_IN, B_INT32_TYPE, 0, &inputTokens, sizeof(int32));
        msgFile.ReadAttr(ATTR_MESSAGE_TOKENS_OUT, B_INT32_TYPE, 0, &outputTokens, sizeof(int32));
        msgFile.ReadAttr(ATTR_MESSAGE_ORDER, B_INT32_TYPE, 0, &order, sizeof(int32));

        // Read message content
        off_t fileSize;
        msgFile.GetSize(&fileSize);
        char* contentBuffer = new char[fileSize + 1];
        msgFile.Read(contentBuffer, fileSize);
        contentBuffer[fileSize] = '\0';

        // Create message
        ChatMessage* message = new ChatMessage(contentBuffer, (MessageRole)role);
        message->SetTimestamp(timestamp);
        message->SetInputTokens(inputTokens);
        message->SetOutputTokens(outputTokens);

        // We'll sort by order later
        orderedMessages.AddPointer(BString().SetToFormat("%d", order).String(), message);
        messageCount++;

        delete[] contentBuffer;
    }

    // Add messages in order
    for (int32 i = 0; i < messageCount; i++) {
        void* ptr;
        if (orderedMessages.FindPointer(BString().SetToFormat("%d", i).String(), &ptr) == B_OK) {
            ChatMessage* message = static_cast<ChatMessage*>(ptr);
            messages->AddItem(message);
        }
    }

    return B_OK;
}

BObjectList<Chat, true>* BFSStorage::LoadAllChats()
{
    BObjectList<Chat, true>* chats = new BObjectList<Chat, true>(10);

    // Create a query to find all chat files
    BQuery query;

    // Get volume containing the chats directory
    BEntry chatsEntry(fChatsDir.Path());
    BVolume volume;
    chatsEntry.GetVolume(&volume);

    query.SetVolume(&volume);

    // Find files with MIME type "application/x-vnd.Otto-chat"
    query.SetPredicate("BEOS:TYPE == \"application/x-vnd.Otto-chat\"");

    status_t status = query.Fetch();
    if (status != B_OK)
        return chats;

    // Process results
    BEntry entry;
    while (query.GetNextEntry(&entry) == B_OK) {
        entry_ref ref;
        entry.GetRef(&ref);

        Chat* chat = new Chat("");
        if (LoadChat(ref, chat) == B_OK) {
            chats->AddItem(chat);
        } else {
            delete chat;
        }
    }

    return chats;
}

status_t BFSStorage::DeleteChat(const entry_ref& ref)
{
    // First delete the messages directory
    BString chatDirName(ref.name);
    chatDirName << "_messages";

    BPath messagesPath(fChatsDir);
    messagesPath.Append(chatDirName);

    BEntry messagesEntry(messagesPath.Path());
    if (messagesEntry.Exists()) {
        BDirectory messagesDir(messagesPath.Path());
        messagesDir.Rewind();
        BEntry entry;
        while (messagesDir.GetNextEntry(&entry) == B_OK) {
            entry.Remove();
        }
        messagesEntry.Remove();
    }

    // Then delete the chat file
    BEntry chatEntry(&ref);
    return chatEntry.Remove();
}

status_t BFSStorage::SaveUsageStats(const BString& provider, const BString& model,
                                   int32 inputTokens, int32 outputTokens)
{
    time_t now = time(NULL);

    // Create a filename from timestamp and a unique ID
    BString filename;
    filename.SetToFormat("usage_%ld_%s", now, _GenerateUniqueID().String());

    BPath usagePath(fStatsDir);
    usagePath.Append(filename);

    // Create empty file to represent the usage record
    BFile usageFile(usagePath.Path(), B_CREATE_FILE | B_WRITE_ONLY);
    if (usageFile.InitCheck() != B_OK)
        return usageFile.InitCheck();

    // Set file MIME type
    BNodeInfo nodeInfo(&usageFile);
    nodeInfo.SetType("application/x-vnd.Otto-usage");

    // Add attributes to the file
    usageFile.WriteAttr(ATTR_USAGE_PROVIDER, B_STRING_TYPE, 0, provider.String(), provider.Length() + 1);
    usageFile.WriteAttr(ATTR_USAGE_MODEL, B_STRING_TYPE, 0, model.String(), model.Length() + 1);
    usageFile.WriteAttr(ATTR_USAGE_TIMESTAMP, B_TIME_TYPE, 0, &now, sizeof(time_t));
    usageFile.WriteAttr(ATTR_USAGE_TOKENS_IN, B_INT32_TYPE, 0, &inputTokens, sizeof(int32));
    usageFile.WriteAttr(ATTR_USAGE_TOKENS_OUT, B_INT32_TYPE, 0, &outputTokens, sizeof(int32));

    return B_OK;
}

status_t BFSStorage::GetTotalUsage(const BString& provider, const BString& model,
                                  time_t startTime, time_t endTime,
                                  int32* inputTokens, int32* outputTokens)
{
    if (inputTokens == NULL || outputTokens == NULL)
        return B_BAD_VALUE;

    // Reset totals
    *inputTokens = 0;
    *outputTokens = 0;

    // Create a query to find usage records matching the criteria
    BQuery query;

    // Get volume containing the stats directory
    BEntry statsEntry(fStatsDir.Path());
    BVolume volume;
    statsEntry.GetVolume(&volume);

    query.SetVolume(&volume);

    // Build query expression
    BString expression = "BEOS:TYPE == \"application/x-vnd.Otto-usage\"";

    // Add provider criteria if specified
    if (provider.Length() > 0) {
        expression << " && " << ATTR_USAGE_PROVIDER << " == \"" << provider.String() << "\"";
    }

    // Add model criteria if specified
    if (model.Length() > 0) {
        expression << " && " << ATTR_USAGE_MODEL << " == \"" << model.String() << "\"";
    }

    // Add time range
    expression << " && " << ATTR_USAGE_TIMESTAMP << " >= " << startTime;
    expression << " && " << ATTR_USAGE_TIMESTAMP << " <= " << endTime;

    query.SetPredicate(expression.String());

    status_t status = query.Fetch();
    if (status != B_OK)
        return status;

    // Process results
    BEntry entry;
    while (query.GetNextEntry(&entry) == B_OK) {
        entry_ref ref;
        entry.GetRef(&ref);

        BNode node(&ref);
        if (node.InitCheck() != B_OK)
            continue;

        int32 in = 0, out = 0;
        node.ReadAttr(ATTR_USAGE_TOKENS_IN, B_INT32_TYPE, 0, &in, sizeof(int32));
        node.ReadAttr(ATTR_USAGE_TOKENS_OUT, B_INT32_TYPE, 0, &out, sizeof(int32));

        *inputTokens += in;
        *outputTokens += out;
    }

    return B_OK;
}