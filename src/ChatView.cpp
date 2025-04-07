// ChatView.cpp
#include "ChatView.h"
#include <LayoutBuilder.h>
#include <GroupLayout.h>
#include <Catalog.h>
#include <Alert.h>
#include <Roster.h>
#include <StringView.h>
#include <SpaceLayoutItem.h>
#include <cstdio>
#include "SettingsManager.h"
#include "BFSStorage.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChatView"

ChatView::ChatView()
    : BView("chatView", B_WILL_DRAW)
    , fActiveChat(NULL)
    , fActiveProvider(NULL)
    , fActiveModel(NULL)
    , fIsBusy(false)
    , fMessenger(this)
{
    _BuildLayout();
}

ChatView::~ChatView()
{
}

void ChatView::_BuildLayout()
{
    // Create chat display area (read-only text view with scroll bars)
    fChatDisplay = new BTextView("chatDisplay");
    fChatDisplay->SetWordWrap(true);
    fChatDisplay->MakeEditable(false);
    fChatDisplay->MakeSelectable(true);

    fChatScrollView = new BScrollView("chatScrollView", fChatDisplay,
        B_WILL_DRAW | B_FRAME_EVENTS, true, true);

    // Create input field (text view with scroll bars)
    fInputField = new BTextView("inputField");
    fInputField->SetWordWrap(true);
    BScrollView* inputScrollView = new BScrollView("inputScrollView",
        fInputField, B_WILL_DRAW | B_FRAME_EVENTS, true, true);

    // Create buttons
    fSendButton = new BButton("sendButton", B_TRANSLATE("Send"),
        new BMessage(MSG_SEND_MESSAGE));

    fCancelButton = new BButton("cancelButton", B_TRANSLATE("Cancel"),
        new BMessage(MSG_CANCEL_REQUEST));
    fCancelButton->SetEnabled(false);

    // Set up layout
    SetLayout(new BGroupLayout(B_VERTICAL));

    BLayoutBuilder::Group<>(this, B_VERTICAL)
        .Add(fChatScrollView, 10.0)
        .AddGroup(B_HORIZONTAL)
            .Add(inputScrollView, 8.0)
            .AddGroup(B_VERTICAL, 0)
                .Add(fSendButton)
                .Add(fCancelButton)
                .AddGlue()
                .End()
            .End()
        .SetInsets(B_USE_DEFAULT_SPACING)
        .End();
}

void ChatView::AttachedToWindow()
{
   BView::AttachedToWindow();

   fSendButton->SetTarget(this);
   fCancelButton->SetTarget(this);

   // Focus the input field
   fInputField->MakeFocus(true);
}

void ChatView::MessageReceived(BMessage* message)
{
   switch (message->what) {
       case MSG_SEND_MESSAGE:
           _SendMessage();
           break;

       case MSG_CANCEL_REQUEST:
           if (fActiveProvider != NULL && fIsBusy) {
               fActiveProvider->CancelRequest();
               fIsBusy = false;
               fCancelButton->SetEnabled(false);
               fSendButton->SetEnabled(true);
           }
           break;

	case MSG_MESSAGE_RECEIVED: {
		// Handle response from the LLM
		BString content;
		printf("Received message from LLM provider\n");
		if (message->FindString("content", &content) == B_OK) {
			printf("Message content: %s\n", content.String());
			// Create a new assistant message
			ChatMessage* reply = new ChatMessage(content, MESSAGE_ROLE_ASSISTANT);

			// Get token counts if available
			int32 inputTokens = 0, outputTokens = 0;
			message->FindInt32("input_tokens", &inputTokens);
			message->FindInt32("output_tokens", &outputTokens);
			printf("Tokens - Input: %d, Output: %d\n", inputTokens, outputTokens);
			reply->SetInputTokens(inputTokens);
			reply->SetOutputTokens(outputTokens);

			// Add to chat and display
			if (fActiveChat != NULL) {
				printf("Adding message to active chat\n");
				fActiveChat->AddMessage(reply);
				fActiveChat->SetUpdatedAt(time(NULL));

				// Save the chat
				BFSStorage::GetInstance()->SaveChat(fActiveChat);

				// Save usage statistics
				if (fActiveProvider != NULL && fActiveModel != NULL) {
					BFSStorage::GetInstance()->SaveUsageStats(
						fActiveProvider->Name(),
						fActiveModel->Name(),
						inputTokens,
						outputTokens
					);
				}
			}

			printf("Appending message to display\n");
			_AppendMessageToDisplay(reply);
		}

		// Update UI state
		fIsBusy = false;
		printf("Setting UI state back to ready\n");
		fCancelButton->SetEnabled(false);
		fSendButton->SetEnabled(true);

		// Focus the input field again
		fInputField->MakeFocus(true);
		break;
	}

       default:
           BView::MessageReceived(message);
           break;
   }
}

void ChatView::SetActiveChat(Chat* chat)
{
   fActiveChat = chat;
   _DisplayChat();
}

void ChatView::SetProvider(LLMProvider* provider)
{
   fActiveProvider = provider;
}

void ChatView::SetModel(LLMModel* model)
{
   fActiveModel = model;
}

void ChatView::_DisplayChat()
{
   // Clear the display
   fChatDisplay->SetText("");

   if (fActiveChat == NULL)
       return;

   // Display all messages in the chat
   BObjectList<ChatMessage, true>* messages = fActiveChat->Messages();
   for (int32 i = 0; i < messages->CountItems(); i++) {
       _AppendMessageToDisplay(messages->ItemAt(i));
   }

   // Scroll to the bottom
   fChatDisplay->ScrollTo(0, fChatDisplay->TextHeight(0, fChatDisplay->CountLines()));
}

void ChatView::_AppendMessageToDisplay(ChatMessage* message)
{
   if (message == NULL)
       return;

   // Get text insertion point at the end
   int32 textLength = fChatDisplay->TextLength();

   // Format based on role
   BString formattedText;

   switch (message->Role()) {
       case MESSAGE_ROLE_USER:
           formattedText = "\n🧑 ";  // User emoji
           break;

       case MESSAGE_ROLE_ASSISTANT:
           formattedText = "\n🤖 ";  // AI emoji
           break;

       case MESSAGE_ROLE_SYSTEM:
           formattedText = "\n⚙️ ";  // System emoji
           break;
   }

   // Add content
   formattedText << message->Content();
   formattedText << "\n";

   // Insert the text
   fChatDisplay->Insert(textLength, formattedText.String(), formattedText.Length());

   // Apply styling based on role
   BFont font;
   fChatDisplay->GetFont(&font);

   rgb_color color;

   switch (message->Role()) {
       case MESSAGE_ROLE_USER:
           color = {0, 0, 200};  // Blue for user
           break;

       case MESSAGE_ROLE_ASSISTANT:
           color = {0, 130, 0};  // Green for assistant
           break;

       case MESSAGE_ROLE_SYSTEM:
           color = {130, 60, 0};  // Brown for system
           break;

       default:
           color = {0, 0, 0};  // Black default
   }

   // Apply text color
   fChatDisplay->SetFontAndColor(textLength, textLength + formattedText.Length(),
                                &font, B_FONT_ALL, &color);

   // Scroll to the bottom
   fChatDisplay->ScrollTo(0, fChatDisplay->TextHeight(0, fChatDisplay->CountLines()));
}

void ChatView::_SendMessage()
{
	if (fActiveChat == NULL || fActiveProvider == NULL || fActiveModel == NULL) {
       // Add debug output
       printf("Error: Missing component to send message:\n");
       printf("- Chat: %s\n", fActiveChat ? "Set" : "NULL");
       printf("- Provider: %s\n", fActiveProvider ? fActiveProvider->Name().String() : "NULL");
       printf("- Model: %s\n", fActiveModel ? fActiveModel->Name().String() : "NULL");

       // Show error alert
       BAlert* alert = new BAlert(B_TRANSLATE("Error"),
           B_TRANSLATE("Please select a chat, provider, and model first."),
           B_TRANSLATE("OK"), NULL, NULL,
           B_WIDTH_AS_USUAL, B_WARNING_ALERT);
       alert->Go();
       return;
   }



   // Get message text from input field
   BString messageText = fInputField->Text();
   if (messageText.IsEmpty())
       return;

   // Create a new message
   ChatMessage* message = new ChatMessage(messageText, MESSAGE_ROLE_USER);

   // Add to chat and display
   fActiveChat->AddMessage(message);
   fActiveChat->SetUpdatedAt(time(NULL));
   _AppendMessageToDisplay(message);

   // Save the chat (to preserve the user's message even if app crashes)
   BFSStorage::GetInstance()->SaveChat(fActiveChat);

   // Clear input field
   fInputField->SetText("");

   // Send message to the LLM provider
   fIsBusy = true;
   fSendButton->SetEnabled(false);
   fCancelButton->SetEnabled(true);

   // Prepare history
   BObjectList<ChatMessage, true>* history = fActiveChat->Messages();

   // Send request
   fActiveProvider->SendMessage(*history, messageText, &fMessenger);
}
