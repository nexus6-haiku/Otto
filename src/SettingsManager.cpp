// SettingsManager.cpp
#include "SettingsManager.h"
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Directory.h>

SettingsManager* SettingsManager::sInstance = NULL;

SettingsManager* SettingsManager::GetInstance()
{
    if (sInstance == NULL)
        sInstance = new SettingsManager();
    
    return sInstance;
}

SettingsManager::SettingsManager()
{
    // Set up settings file path in user settings folder
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("HaikuLLM");
        
        // Create directory if it doesn't exist
        BDirectory dir(path.Path());
        if (dir.InitCheck() != B_OK)
            create_directory(path.Path(), 0755);
        
        path.Append("settings");
        fSettingsPath = path.Path();
    } else {
        // Fallback to temp directory
        fSettingsPath = "/tmp/haikullm_settings";
    }
    
    LoadSettings();
}

SettingsManager::~SettingsManager()
{
    SaveSettings();
}

void SettingsManager::LoadSettings()
{
    BFile file(fSettingsPath.String(), B_READ_ONLY);
    if (file.InitCheck() == B_OK) {
        fSettings.Unflatten(&file);
    }
}

void SettingsManager::SaveSettings()
{
    BFile file(fSettingsPath.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() == B_OK) {
        fSettings.Flatten(&file);
    }
}
