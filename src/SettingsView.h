// SettingsView.h
#ifndef SETTINGS_VIEW_H
#define SETTINGS_VIEW_H

#include <View.h>
#include <TabView.h>
#include <CheckBox.h>
#include <Slider.h>
#include <Button.h>
#include <StringView.h>
#include <TextControl.h>

const uint32 MSG_SETTINGS_CHANGED = 'stch';

class SettingsView : public BView {
public:
    SettingsView();
    virtual ~SettingsView();

    virtual void AttachedToWindow();
    virtual void MessageReceived(BMessage* message);

private:
    void _BuildLayout();
    void _LoadSettings();
    void _SaveSettings();
    void _UpdateAPIStatus();
    void _UpdateUsageStats();

    BTabView* fTabView;

    // Model settings
    BSlider* fTemperatureSlider;
    BSlider* fMaxTokensSlider;
    BCheckBox* fToolsEnabledCheckbox;

    // API settings
    BButton* fAPISettingsButton;
    BStringView* fAPIStatusView;

    // Usage stats
    BStringView* fTotalUsageView;
    BButton* fResetStatsButton;
};

#endif // SETTINGS_VIEW_H