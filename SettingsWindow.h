// SettingsWindow.h
#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include <Window.h>
#include <TabView.h>
#include <TextControl.h>
#include <Button.h>
#include "LLMProvider.h"

const uint32 MSG_SAVE_SETTINGS = 'svst';

class SettingsWindow : public BWindow {
public:
    SettingsWindow();
    virtual ~SettingsWindow();
    
    virtual void MessageReceived(BMessage* message);
    virtual bool QuitRequested();
    
private:
    void _BuildLayout();
    void _LoadSettings();
    void _SaveSettings();
    
    BTabView* fTabView;
    BTextControl* fOpenAIKeyControl;
    BTextControl* fOpenAIBaseControl;
    BTextControl* fAnthropicKeyControl;
    BTextControl* fAnthropicBaseControl;
    BTextControl* fOllamaBaseControl;
    BButton* fSaveButton;
};

#endif // SETTINGS_WINDOW_H
