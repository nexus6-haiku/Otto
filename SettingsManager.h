// SettingsManager.h
#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <String.h>
#include <Message.h>

class SettingsManager {
public:
    static SettingsManager* GetInstance();
    
    void LoadSettings();
    void SaveSettings();
    
    BString GetApiKey(const BString& provider);
    void SetApiKey(const BString& provider, const BString& apiKey);
    
    BString GetApiBase(const BString& provider);
    void SetApiBase(const BString& provider, const BString& apiBase);
    
    BString GetDefaultModel(const BString& provider);
    void SetDefaultModel(const BString& provider, const BString& model);
    
    bool GetToolsEnabled();
    void SetToolsEnabled(bool enabled);
    
private:
    SettingsManager();
    ~SettingsManager();
    
    static SettingsManager* sInstance;
    BMessage fSettings;
    BString fSettingsPath;
};

#endif // SETTINGS_MANAGER_H
