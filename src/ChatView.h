// ChatView.h
#ifndef CHAT_VIEW_H
#define CHAT_VIEW_H

#include <View.h>
#include <TextView.h>
#include <ScrollView.h>
#include <Button.h>
#include <ObjectList.h>
#include "ChatMessage.h"
#include "ModelSelector.h"
#include "LLMProvider.h"

const uint32 MSG_SEND_MESSAGE = 'send';
const uint32 MSG_MESSAGE_RECEIVED = 'rcvd';
const uint32 MSG_CANCEL_REQUEST = 'cncl';

class ChatView : public BView {
public:
    ChatView();
    virtual ~ChatView();
    
    virtual void AttachedToWindow();
    virtual void MessageReceived(BMessage* message);
    
    void SetActiveChat(Chat* chat);
    void SetProvider(LLMProvider* provider);
    void SetModel(LLMModel* model);
    
    bool IsBusy() const { return fIsBusy; }
    
private:
    void _BuildLayout();
    void _DisplayChat();
    void _SendMessage();
    void _AppendMessageToDisplay(ChatMessage* message);
    
    BTextView* fChatDisplay;
    BScrollView* fChatScrollView;
    BTextView* fInputField;
    BButton* fSendButton;
    BButton* fCancelButton;
    
    Chat* fActiveChat;
    LLMProvider* fActiveProvider;
    LLMModel* fActiveModel;
    bool fIsBusy;
    BMessenger fMessenger;
};

#endif // CHAT_VIEW_H
