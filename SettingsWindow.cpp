// SettingsWindow.cpp
#include "SettingsWindow.h"
#include <LayoutBuilder.h>
#include <Catalog.h>
#include <GroupView.h>
#include <CheckBox.h>
#include "SettingsManager.h"
#include "ModelManager.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SettingsWindow"

SettingsWindow::SettingsWindow()
    : BWindow(BRect(100, 100, 500, 400), B_TRANSLATE("Settings"), B_TITLED_WINDOW,
             B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
{
    _BuildLayout();
    _LoadSettings();
    
    CenterOnScreen();
}

SettingsWindow::~SettingsWindow()
{
}

void SettingsWindow::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_SAVE_SETTINGS:
            _SaveSettings();
            PostMessage(B_QUIT_REQUESTED);
            break;
            
        default:
            BWindow::MessageReceived(message);
            break;
    }
}

bool SettingsWindow::QuitRequested()
{
    return true;
}

void SettingsWindow::_BuildLayout()
{
    // Create tab view
    fTabView = new BTabView("tabView", B_WIDTH_FROM_LABEL);
    
    // OpenAI tab
    BView* openaiTab = new BGroupView(B_VERTICAL);
    fOpenAIKeyControl = new BTextControl("openaiKey", B_TRANSLATE("API Key:"), "", NULL);
    fOpenAIBaseControl = new BTextControl("openaiBase", B_TRANSLATE("API Base URL:"), "", NULL);
    
    BLayoutBuilder::Group<>(openaiTab, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .Add(fOpenAIKeyControl)
        .Add(fOpenAIBaseControl)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING);
    
    // Anthropic tab
    BView* anthropicTab = new BGroupView(B_VERTICAL);
    fAnthropicKeyControl = new BTextControl("anthropicKey", B_TRANSLATE("API Key:"), "", NULL);
    fAnthropicBaseControl = new BTextControl("anthropicBase", B_TRANSLATE("API Base URL:"), "", NULL);
    
    BLayoutBuilder::Group<>(anthropicTab, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .Add(fAnthropicKeyControl)
        .Add(fAnthropicBaseControl)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING);
    
    // Ollama tab
    BView* ollamaTab = new BGroupView(B_VERTICAL);
    fOllamaBaseControl = new BTextControl("ollamaBase", B_TRANSLATE("API Base URL:"), "", NULL);
    
    BLayoutBuilder::Group<>(ollamaTab, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .Add(fOllamaBaseControl)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING);
    
    // Add tabs
    fTabView->AddTab(openaiTab, new BTab());
    fTabView->TabAt(0)->SetLabel(B_TRANSLATE("OpenAI"));
    
    fTabView->AddTab(anthropicTab, new BTab());
    fTabView->TabAt(1)->SetLabel(B_TRANSLATE("Anthropic"));
    
    fTabView->AddTab(ollamaTab, new BTab());
    fTabView->TabAt(2)->SetLabel(B_TRANSLATE("Ollama"));
    
    // Create save button
    fSaveButton = new BButton("saveButton", B_TRANSLATE("Save"), new BMessage(MSG_SAVE_SETTINGS));
    
    // Set up layout
    BLayoutBuilder::Group<>(this, B_VERTICAL)
        .Add(fTabView)
        .AddGroup(B_HORIZONTAL)
            .AddGlue()
            .Add(fSaveButton)
            .End()
        .SetInsets(B_USE_DEFAULT_SPACING)
        .End();
}

void SettingsWindow::_LoadSettings()
{
    SettingsManager* settings = SettingsManager::GetInstance();
    
    // Load OpenAI settings
    fOpenAIKeyControl->SetText(settings->GetApiKey("OpenAI"));
    fOpenAIBaseControl->SetText(settings->GetApiBase("OpenAI"));
    
    // Load Anthropic settings
    fAnthropicKeyControl->SetText(settings->GetApiKey("Anthropic"));
    fAnthropicBaseControl->SetText(settings->GetApiBase("Anthropic"));
    
    // Load Ollama settings
    fOllamaBaseControl->SetText(settings->GetApiBase("Ollama"));
}

void SettingsWindow::_SaveSettings()
{
    SettingsManager* settings = SettingsManager::GetInstance();
    
    // Save OpenAI settings
    settings->SetApiKey("OpenAI", fOpenAIKeyControl->Text());
    settings->SetApiBase("OpenAI", fOpenAIBaseControl->Text());
    
    // Save Anthropic settings
    settings->SetApiKey("Anthropic", fAnthropicKeyControl->Text());
    settings->SetApiBase("Anthropic", fAnthropicBaseControl->Text());
    
    // Save Ollama settings
    settings->SetApiBase("Ollama", fOllamaBaseControl->Text());
    
    // Save all settings
    settings->SaveSettings();
}
