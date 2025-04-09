// MainWindow.cpp
#include "MainWindow.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Catalog.h>

#include "BFSStorage.h"
#include "SettingsWindow.h"
#include "ModelManager.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

MainWindow::MainWindow()
    : BWindow(BRect(100, 100, 700, 900), B_TRANSLATE("Otto"), B_TITLED_WINDOW,
        B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
{
    _BuildMenu();
    _InitLayout();

    // Create and set a default chat
    Chat* defaultChat = new Chat("New Chat");
    ChatMessage* sysMsg = new ChatMessage("You are chatting with an AI assistant.", MESSAGE_ROLE_SYSTEM);
    defaultChat->AddMessage(sysMsg);
    fChatView->SetActiveChat(defaultChat);

    // Save the new chat
    BFSStorage::GetInstance()->SaveChat(defaultChat);

    CenterOnScreen();
}

MainWindow::~MainWindow()
{
}

bool MainWindow::QuitRequested()
{
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void MainWindow::_BuildMenu()
{
    fMenuBar = new BMenuBar("menubar");

    // File menu
    BMenu* fileMenu = new BMenu(B_TRANSLATE("File"));
    fileMenu->AddItem(new BMenuItem(B_TRANSLATE("New Chat"), new BMessage(MSG_NEW_CHAT), 'N'));
    fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Save Chat"), new BMessage(MSG_SAVE_CHAT), 'S'));
    fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Export Chat"), new BMessage(MSG_EXPORT_CHAT), 'E'));
    fileMenu->AddSeparatorItem();
    fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Quit"), new BMessage(B_QUIT_REQUESTED), 'Q'));
    fMenuBar->AddItem(fileMenu);

    // Edit menu
    BMenu* editMenu = new BMenu(B_TRANSLATE("Edit"));
    editMenu->AddItem(new BMenuItem(B_TRANSLATE("Copy"), new BMessage(B_COPY), 'C'));
    editMenu->AddItem(new BMenuItem(B_TRANSLATE("Paste"), new BMessage(B_PASTE), 'V'));
    fMenuBar->AddItem(editMenu);

    // View menu
    BMenu* viewMenu = new BMenu(B_TRANSLATE("View"));
    viewMenu->AddItem(new BMenuItem(B_TRANSLATE("Settings"), new BMessage(MSG_SHOW_SETTINGS)));
    viewMenu->AddItem(new BMenuItem(B_TRANSLATE("Usage Statistics"), new BMessage(MSG_SHOW_STATS)));
    fMenuBar->AddItem(viewMenu);

    // Help menu
    BMenu* helpMenu = new BMenu(B_TRANSLATE("Help"));
    helpMenu->AddItem(new BMenuItem(B_TRANSLATE("About Otto"), new BMessage(B_ABOUT_REQUESTED)));
    fMenuBar->AddItem(helpMenu);
}

void MainWindow::_InitLayout()
{
    fModelSelector = new ModelSelector();
    fChatView = new ChatView();
    fSettingsView = new SettingsView();

    // Create a split view with model selector on left and chat view on right
    fMainSplitView = new BSplitView(B_HORIZONTAL);
    fMainSplitView->SetCollapsible(true);

    BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
        .Add(fMenuBar)
        .AddSplit(fMainSplitView)
            .AddGroup(B_VERTICAL)
                .Add(fModelSelector)
                .SetInsets(B_USE_DEFAULT_SPACING)
                .End()
            .Add(fChatView)
            .SetInsets(B_USE_DEFAULT_SPACING)
            .End()
		.AddGlue()
	.End();
}

void MainWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_NEW_CHAT:
            // Handle new chat
            {
                // Create a new chat with a default title
                Chat* newChat = new Chat("New Chat");
                // Add system message for context (optional)
                ChatMessage* sysMsg = new ChatMessage("You are chatting with an AI assistant.", MESSAGE_ROLE_SYSTEM);
                newChat->AddMessage(sysMsg);
                // Set as active chat
                fChatView->SetActiveChat(newChat);
                // Save the new chat
                BFSStorage::GetInstance()->SaveChat(newChat);
            }
            break;

        case MSG_SAVE_CHAT:
            // Trigger chat save (already handled in ChatView when messages are sent)
            break;

        case MSG_EXPORT_CHAT:
            // Handle chat export
            break;

        case MSG_SHOW_SETTINGS:
            {
                SettingsWindow* window = new SettingsWindow();
                window->Show();
            }
            break;

        case MSG_SHOW_STATS:
            // Show statistics window
            break;

        case B_OBSERVER_NOTICE_CHANGE:
            {
                // Handle provider selection change
                BString providerName;
                if (message->FindString("provider_selected", &providerName) == B_OK) {
                    LLMProvider* provider = ModelManager::GetInstance()->GetProvider(providerName);
                    if (provider != NULL) {
                        fChatView->SetProvider(provider);

                        // Auto-select default model if available
                        BObjectList<LLMModel>* models = provider->GetModels();
                        if (models != NULL && models->CountItems() > 0) {
                            fChatView->SetModel(models->ItemAt(0));
                        }
                    }
                }

                // Handle model selection change
                BString modelName;
                if (message->FindString("model_selected", &modelName) == B_OK) {
                    if (fModelSelector->SelectedProvider() != NULL) {
                        BObjectList<LLMModel>* models = fModelSelector->SelectedProvider()->GetModels();
                        for (int32 i = 0; i < models->CountItems(); i++) {
                            if (models->ItemAt(i)->Name() == modelName) {
                                fChatView->SetModel(models->ItemAt(i));
                                break;
                            }
                        }
                    }
                }
            }
            break;

        default:
            BWindow::MessageReceived(message);
            break;
    }
}
