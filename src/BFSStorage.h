// BFSStorage.h
#ifndef BFS_STORAGE_H
#define BFS_STORAGE_H

#include <String.h>
#include <Path.h>
#include <Directory.h>
#include <ObjectList.h>
#include <Query.h>
#include <FindDirectory.h>
#include <fs_index.h>
#include <Volume.h>
#include <NodeInfo.h>
#include "ChatMessage.h"

// Data storage types
#define ATTR_CHAT_TITLE "Otto:Title"
#define ATTR_CHAT_CREATED "Otto:CreatedAt"
#define ATTR_CHAT_UPDATED "Otto:UpdatedAt"
#define ATTR_MESSAGE_ROLE "Otto:Role"
#define ATTR_MESSAGE_TIMESTAMP "Otto:Timestamp"
#define ATTR_MESSAGE_TOKENS_IN "Otto:TokensIn"
#define ATTR_MESSAGE_TOKENS_OUT "Otto:TokensOut"
#define ATTR_CHAT_ID "Otto:ChatID"
#define ATTR_MESSAGE_ORDER "Otto:Order"

// Usage stats types
#define ATTR_USAGE_PROVIDER "Otto:Provider"
#define ATTR_USAGE_MODEL "Otto:Model"
#define ATTR_USAGE_TOKENS_IN "Otto:TokensIn"
#define ATTR_USAGE_TOKENS_OUT "Otto:TokensOut"
#define ATTR_USAGE_TIMESTAMP "Otto:Timestamp"

class BFSStorage {
public:
    static BFSStorage* GetInstance();

    bool Initialize();

    // Chat operations
    status_t SaveChat(Chat* chat);
    status_t LoadChat(const entry_ref& ref, Chat* chat);
    status_t DeleteChat(const entry_ref& ref);
    BObjectList<Chat, true>* LoadAllChats();

    // Usage statistics
    status_t SaveUsageStats(const BString& provider, const BString& model,
                            int32 inputTokens, int32 outputTokens);
    status_t GetTotalUsage(const BString& provider, const BString& model,
                           time_t startTime, time_t endTime,
                           int32* inputTokens, int32* outputTokens);

    // Helper methods
    BPath GetChatsDirectory() const { return fChatsDir; }
    BPath GetStatsDirectory() const { return fStatsDir; }

private:
    BFSStorage();
    ~BFSStorage();

    status_t _EnsureDirectoryExists(BPath& path);
    status_t _CreateIndices(dev_t device);
    status_t _SaveChatMessages(const entry_ref& chatRef, const BObjectList<ChatMessage, true>* messages);
    status_t _LoadChatMessages(const entry_ref& chatRef, BObjectList<ChatMessage, true>* messages);
    BString _GenerateUniqueID() const;

    static BFSStorage* sInstance;
    BPath fBasePath;
    BPath fChatsDir;
    BPath fStatsDir;
};

#endif // BFS_STORAGE_H