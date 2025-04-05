// MainWindow.h
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <Window.h>
#include <MenuBar.h>
#include <Layout.h>
#include <SplitView.h>
#include "ChatView.h"
#include "ModelSelector.h"
#include "SettingsView.h"

// Message constants
const uint32 MSG_NEW_CHAT = 'newc';
const uint32 MSG_SAVE_CHAT = 'savc';
const uint32 MSG_EXPORT_CHAT = 'expc';
const uint32 MSG_SHOW_SETTINGS = 'shst';
const uint32 MSG_SHOW_STATS = 'shss';

class MainWindow : public BWindow {
public:
    MainWindow();
    virtual ~MainWindow();
    virtual bool QuitRequested();

private:
    void _BuildMenu();
    void _InitLayout();

    BMenuBar* fMenuBar;
    BSplitView* fMainSplitView;
    ModelSelector* fModelSelector;
    ChatView* fChatView;
    SettingsView* fSettingsView;
};

#endif // MAIN_WINDOW_H
