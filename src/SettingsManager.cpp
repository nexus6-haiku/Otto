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
        path.Append("Otto");

        // Create directory if it doesn't exist
        BDirectory dir(path.Path());
        if (dir.InitCheck() != B_OK)
            create_directory(path.Path(), 0755);

        path.Append("settings");
        fSettingsPath = path.Path();
    } else {
        // Fallback to temp directory
        fSettingsPath = "/tmp/otto_settings";
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

BString SettingsManager::GetApiKey(const BString& provider)
{
    BString key;
    BString settingName = provider;
    settingName.Append("ApiKey");

    if (fSettings.FindString(settingName, &key) != B_OK)
        return "";

    return key;
}

void SettingsManager::SetApiKey(const BString& provider, const BString& apiKey)
{
    BString settingName = provider;
    settingName.Append("ApiKey");

    if (fSettings.HasString(settingName))
        fSettings.ReplaceString(settingName, apiKey);
    else
        fSettings.AddString(settingName, apiKey);
}

BString SettingsManager::GetApiBase(const BString& provider)
{
    BString base;
    BString settingName = provider;
    settingName.Append("ApiBase");

    if (fSettings.FindString(settingName, &base) != B_OK)
        return "";

    return base;
}

void SettingsManager::SetApiBase(const BString& provider, const BString& apiBase)
{
    BString settingName = provider;
    settingName.Append("ApiBase");

    if (fSettings.HasString(settingName))
        fSettings.ReplaceString(settingName, apiBase);
    else
        fSettings.AddString(settingName, apiBase);
}

BString SettingsManager::GetDefaultModel(const BString& provider)
{
    BString model;
    BString settingName = provider;
    settingName.Append("DefaultModel");

    if (fSettings.FindString(settingName, &model) != B_OK)
        return "";

    return model;
}

void SettingsManager::SetDefaultModel(const BString& provider, const BString& model)
{
    BString settingName = provider;
    settingName.Append("DefaultModel");

    if (fSettings.HasString(settingName))
        fSettings.ReplaceString(settingName, model);
    else
        fSettings.AddString(settingName, model);
}

bool SettingsManager::GetToolsEnabled()
{
    bool enabled;

    if (fSettings.FindBool("ToolsEnabled", &enabled) != B_OK)
        return false;

    return enabled;
}

void SettingsManager::SetToolsEnabled(bool enabled)
{
    if (fSettings.HasBool("ToolsEnabled"))
        fSettings.ReplaceBool("ToolsEnabled", enabled);
    else
        fSettings.AddBool("ToolsEnabled", enabled);
}

