// MainWindow.cpp
#include "MainWindow.h"
#include <Application.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuItem.h>
#include <Catalog.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

MainWindow::MainWindow()
    : BWindow(BRect(100, 100, 900, 700), B_TRANSLATE("HaikuLLM"), B_TITLED_WINDOW,
        B_AUTO_UPDATE_SIZE_LIMITS)
{
    _BuildMenu();
    _InitLayout();
    
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
    helpMenu->AddItem(new BMenuItem(B_TRANSLATE("About HaikuLLM"), new BMessage(B_ABOUT_REQUESTED)));
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
        .End();
}
