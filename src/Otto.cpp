// HaikuLLM.cpp
#include <Application.h>
#include <Window.h>
#include "MainWindow.h"

class HaikuLLMApp : public BApplication {
public:
    HaikuLLMApp();
    virtual ~HaikuLLMApp();
};

HaikuLLMApp::HaikuLLMApp()
    : BApplication("application/x-vnd.HaikuLLM")
{
    MainWindow* mainWindow = new MainWindow();
    mainWindow->Show();
}

HaikuLLMApp::~HaikuLLMApp()
{
}

int main()
{
    HaikuLLMApp app;
    app.Run();
    return 0;
}
