// SettingsView.cpp
#include "SettingsView.h"
#include <LayoutBuilder.h>
#include <GroupLayout.h>
#include <Catalog.h>
#include <Alert.h>
#include "SettingsManager.h"
#include "BFSStorage.h"
#include "SettingsWindow.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SettingsView"

SettingsView::SettingsView()
    : BView("settingsView", B_WILL_DRAW)
{
    _BuildLayout();
    _LoadSettings();
}

SettingsView::~SettingsView()
{
}

void SettingsView::_BuildLayout()
{
    // Create tab view for settings categories
    fTabView = new BTabView("settingsTabView", B_WIDTH_FROM_LABEL);

    // Model settings tab
    BView* modelTab = new BGroupView(B_VERTICAL);

    // Add temperature slider
    fTemperatureSlider = new BSlider("temperatureSlider",
        B_TRANSLATE("Temperature:"),
        new BMessage(MSG_SETTINGS_CHANGED),
        0, 100, B_HORIZONTAL);
    fTemperatureSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
    fTemperatureSlider->SetHashMarkCount(11);
    fTemperatureSlider->SetLimitLabels(B_TRANSLATE("Precise"), B_TRANSLATE("Creative"));

    // Add max tokens slider
    fMaxTokensSlider = new BSlider("maxTokensSlider",
        B_TRANSLATE("Max Output Tokens:"),
        new BMessage(MSG_SETTINGS_CHANGED),
        256, 8192, B_HORIZONTAL);
    fMaxTokensSlider->SetHashMarks(B_HASH_MARKS_BOTTOM);
    fMaxTokensSlider->SetHashMarkCount(8);
    fMaxTokensSlider->SetLimitLabels(B_TRANSLATE("Short"), B_TRANSLATE("Long"));

    // Enable tools checkbox
    fToolsEnabledCheckbox = new BCheckBox("toolsEnabled",
        B_TRANSLATE("Enable MCP Tools Integration"),
        new BMessage(MSG_SETTINGS_CHANGED));

    // Layout model tab
    BLayoutBuilder::Group<>(modelTab, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .Add(fTemperatureSlider)
        .Add(fMaxTokensSlider)
        .AddStrut(B_USE_DEFAULT_SPACING)
        .Add(fToolsEnabledCheckbox)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING);

    // API settings tab
    BView* apiTab = new BGroupView(B_VERTICAL);

    // API settings button
    fAPISettingsButton = new BButton("apiSettingsButton",
        B_TRANSLATE("Configure API Keys..."),
        new BMessage(MSG_SETTINGS_CHANGED));

    // API status view
    fAPIStatusView = new BStringView("apiStatusView", "");

    // Layout API tab
    BLayoutBuilder::Group<>(apiTab, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .Add(fAPISettingsButton)
        .Add(fAPIStatusView)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING);

    // Usage stats tab
    BView* statsTab = new BGroupView(B_VERTICAL);

    // Usage stats view
    fTotalUsageView = new BStringView("totalUsageView", "");

    // Reset stats button
    fResetStatsButton = new BButton("resetStatsButton",
        B_TRANSLATE("Reset Statistics"),
        new BMessage(MSG_SETTINGS_CHANGED));

    // Layout stats tab
    BLayoutBuilder::Group<>(statsTab, B_VERTICAL, B_USE_DEFAULT_SPACING)
        .Add(fTotalUsageView)
        .AddStrut(B_USE_DEFAULT_SPACING)
        .Add(fResetStatsButton)
        .AddGlue()
        .SetInsets(B_USE_DEFAULT_SPACING);

    // Add tabs to tab view
    fTabView->AddTab(modelTab, new BTab());
    fTabView->TabAt(0)->SetLabel(B_TRANSLATE("Model"));

    fTabView->AddTab(apiTab, new BTab());
    fTabView->TabAt(1)->SetLabel(B_TRANSLATE("API"));

    fTabView->AddTab(statsTab, new BTab());
    fTabView->TabAt(2)->SetLabel(B_TRANSLATE("Usage"));

    // Set up main layout
    SetLayout(new BGroupLayout(B_VERTICAL));

    BLayoutBuilder::Group<>(this, B_VERTICAL)
        .Add(fTabView)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .End();
}

void SettingsView::AttachedToWindow()
{
    BView::AttachedToWindow();

    // Set targets
    fTemperatureSlider->SetTarget(this);
    fMaxTokensSlider->SetTarget(this);
    fToolsEnabledCheckbox->SetTarget(this);
    fAPISettingsButton->SetTarget(this);
    fResetStatsButton->SetTarget(this);

    // Update usage stats
    _UpdateUsageStats();
}

void SettingsView::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_SETTINGS_CHANGED: {
            const char* name = NULL;
            if (message->FindString("name", &name) == B_OK) {
                if (strcmp(name, "temperatureSlider") == 0) {
                    // Temperature slider changed
                    float value = fTemperatureSlider->Value() / 100.0;
                    SettingsManager::GetInstance()->SetTemperature(value);
                }
                else if (strcmp(name, "maxTokensSlider") == 0) {
                    // Max tokens slider changed
                    int32 value = fMaxTokensSlider->Value();
                    SettingsManager::GetInstance()->SetMaxTokens(value);
                }
                else if (strcmp(name, "toolsEnabled") == 0) {
                    // Tools checkbox changed
                    bool checked = fToolsEnabledCheckbox->Value() == B_CONTROL_ON;
                    SettingsManager::GetInstance()->SetToolsEnabled(checked);
                }
                else if (strcmp(name, "apiSettingsButton") == 0) {
                    // Show API settings window
                    SettingsWindow* window = new SettingsWindow();
                    window->Show();
                }
                else if (strcmp(name, "resetStatsButton") == 0) {
                    // Reset usage stats - for a real app, you'd implement this
                    BAlert* alert = new BAlert(
                        B_TRANSLATE("Confirm Reset"),
                        B_TRANSLATE("Are you sure you want to reset all usage statistics?"),
                        B_TRANSLATE("Cancel"),
                        B_TRANSLATE("Reset"),
                        NULL,
                        B_WIDTH_AS_USUAL,
                        B_WARNING_ALERT);
                    alert->SetShortcut(0, B_ESCAPE);
                    int32 button = alert->Go();

                    if (button == 1) {
                        // Reset stats - in a real app, implement this
                        // For now, we'll just update the view
                        _UpdateUsageStats();
                    }
                }
            }

            // Save settings
            _SaveSettings();
            break;
        }

        default:
            BView::MessageReceived(message);
            break;
    }
}

void SettingsView::_LoadSettings()
{
    SettingsManager* settings = SettingsManager::GetInstance();

    // Set temperature slider
    float temperature = settings->GetTemperature();
    fTemperatureSlider->SetValue(temperature * 100);

    // Set max tokens slider
    int32 maxTokens = settings->GetMaxTokens();
    fMaxTokensSlider->SetValue(maxTokens);

    // Set tools checkbox
    bool toolsEnabled = settings->GetToolsEnabled();
    fToolsEnabledCheckbox->SetValue(toolsEnabled ? B_CONTROL_ON : B_CONTROL_OFF);

    // Update API status
    _UpdateAPIStatus();
}

void SettingsView::_SaveSettings()
{
    SettingsManager* settings = SettingsManager::GetInstance();

    // Save temperature
    float temperature = fTemperatureSlider->Value() / 100.0;
    settings->SetTemperature(temperature);

    // Save max tokens
    int32 maxTokens = fMaxTokensSlider->Value();
    settings->SetMaxTokens(maxTokens);

    // Save tools enabled
    bool toolsEnabled = fToolsEnabledCheckbox->Value() == B_CONTROL_ON;
    settings->SetToolsEnabled(toolsEnabled);

    // Save all settings
    settings->SaveSettings();
}

void SettingsView::_UpdateAPIStatus()
{
    SettingsManager* settings = SettingsManager::GetInstance();

    // Check for API keys
    bool hasOpenAIKey = !settings->GetApiKey("OpenAI").IsEmpty();
    bool hasAnthropicKey = !settings->GetApiKey("Anthropic").IsEmpty();

    // Update status view
    BString status;
    if (!hasOpenAIKey && !hasAnthropicKey) {
        status = B_TRANSLATE("No API keys configured. Click 'Configure API Keys' to set up.");
        fAPIStatusView->SetHighColor(255, 0, 0);
    } else {
        status = B_TRANSLATE("API keys configured:");
        status << "\n";
        if (hasOpenAIKey) status << "✓ OpenAI\n";
        if (hasAnthropicKey) status << "✓ Anthropic\n";
        fAPIStatusView->SetHighColor(0, 128, 0);
    }

    fAPIStatusView->SetText(status);
}

void SettingsView::_UpdateUsageStats()
{
    // Get current month's usage
    time_t now = time(NULL);
    time_t monthStart = now; // In a real app, set to start of month

    int32 totalInputTokens = 0;
    int32 totalOutputTokens = 0;

    // Example of how to get usage stats
    BFSStorage::GetInstance()->GetTotalUsage("", "",
                                            monthStart, now,
                                            &totalInputTokens, &totalOutputTokens);

    // Format and display usage
    BString usageText = B_TRANSLATE("Total tokens used this month:");
    usageText << "\n\n";
    usageText << B_TRANSLATE("Input tokens: ") << totalInputTokens;
    usageText << "\n";
    usageText << B_TRANSLATE("Output tokens: ") << totalOutputTokens;
    usageText << "\n";
    usageText << B_TRANSLATE("Total tokens: ") << (totalInputTokens + totalOutputTokens);

    // Calculate approximate cost (example rates)
    float inputCost = totalInputTokens * 0.0001 / 1000; // $0.0001 per 1K tokens
    float outputCost = totalOutputTokens * 0.0002 / 1000; // $0.0002 per 1K tokens
    float totalCost = inputCost + outputCost;

    usageText << "\n\n";
    usageText << B_TRANSLATE("Estimated cost: $") << totalCost;

    fTotalUsageView->SetText(usageText);
}