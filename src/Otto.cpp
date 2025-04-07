#include <Application.h>
#include <Window.h>
#include "MainWindow.h"

class OttoApp : public BApplication {
public:
    OttoApp();
    virtual ~OttoApp();
};

OttoApp::OttoApp()
    : BApplication("application/x-vnd.Otto")
{
    MainWindow* mainWindow = new MainWindow();
    mainWindow->Show();
}

OttoApp::~OttoApp()
{
}

int main()
{
    OttoApp app;
    app.Run();
    return 0;
}
