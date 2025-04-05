// ChatMessage.h
#ifndef CHAT_MESSAGE_H
#define CHAT_MESSAGE_H

#include <String.h>
#include <ObjectList.h>
#include <time.h>

typedef enum {
    MESSAGE_ROLE_USER,
    MESSAGE_ROLE_ASSISTANT,
    MESSAGE_ROLE_SYSTEM
} MessageRole;

class ChatMessage {
public:
    ChatMessage(const BString& content, MessageRole role);
    ~ChatMessage();

    BString Content() const { return fContent; }
    MessageRole Role() const { return fRole; }
    time_t Timestamp() const { return fTimestamp; }
    void SetTimestamp(time_t timestamp) { fTimestamp = timestamp; }

    int32 InputTokens() const { return fInputTokens; }
    void SetInputTokens(int32 tokens) { fInputTokens = tokens; }

    int32 OutputTokens() const { return fOutputTokens; }
    void SetOutputTokens(int32 tokens) { fOutputTokens = tokens; }

private:
    BString fContent;
    MessageRole fRole;
    time_t fTimestamp;
    int32 fInputTokens;
    int32 fOutputTokens;
};

class Chat {
public:
    Chat(const BString& title);
    ~Chat();

    BString Title() const { return fTitle; }
    void SetTitle(const BString& title) { fTitle = title; }

    time_t CreatedAt() const { return fCreatedAt; }
    void SetCreatedAt(time_t time) { fCreatedAt = time; }

    time_t UpdatedAt() const { return fUpdatedAt; }
    void SetUpdatedAt(time_t time) { fUpdatedAt = time; }

    void AddMessage(ChatMessage* message) { fMessages.AddItem(message); }
    BObjectList<ChatMessage, true>* Messages() { return &fMessages; }

private:
    BString fTitle;
    BObjectList<ChatMessage, true> fMessages;  // true for owning pointers
    time_t fCreatedAt;
    time_t fUpdatedAt;
};

#endif // CHAT_MESSAGE_H