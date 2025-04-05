// ChatMessage.cpp
#include "ChatMessage.h"

ChatMessage::ChatMessage(const BString& content, MessageRole role)
    : fContent(content)
    , fRole(role)
    , fInputTokens(0)
    , fOutputTokens(0)
{
    time(&fTimestamp);
}

ChatMessage::~ChatMessage()
{
}

Chat::Chat(const BString& title)
    : fTitle(title)
    , fMessages(10)  // true for owning pointers
{
    time(&fCreatedAt);
    fUpdatedAt = fCreatedAt;
}

Chat::~Chat()
{
    // BObjectList will handle deleting the messages
}
