// ModelSelector.cpp
#include "ModelSelector.h"
#include <LayoutBuilder.h>
#include <Catalog.h>
#include <MenuItem.h>
#include <stdio.h>  // Add this include for printf
#include "ModelManager.h"
#include "SettingsManager.h"
#include "SettingsWindow.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ModelSelector"

ModelSelector::ModelSelector()
    : BView("modelSelector", B_WILL_DRAW)
    , fSelectedProvider(NULL)
    , fSelectedModel(NULL)
{
    _BuildLayout();
    _LoadProviders();
}

ModelSelector::~ModelSelector()
{
}

void ModelSelector::_BuildLayout()
{
    // Create menu fields
    BPopUpMenu* providerPopUp = new BPopUpMenu("Provider");
    fProviderMenu = new BMenuField("providerMenu", B_TRANSLATE("Provider:"), providerPopUp);

    BPopUpMenu* modelPopUp = new BPopUpMenu("Model");
    fModelMenu = new BMenuField("modelMenu", B_TRANSLATE("Model:"), modelPopUp);

    // Create settings button
    fSettingsButton = new BButton("settingsButton", B_TRANSLATE("Settings"),
        new BMessage(MSG_SETTINGS_CLICKED));

    // Create status view
    fStatusView = new BStringView("statusView", "");

    // Set up layout
    SetLayout(new BGroupLayout(B_VERTICAL));

    BLayoutBuilder::Group<>(this, B_VERTICAL)
        .Add(fProviderMenu)
        .Add(fModelMenu)
        .Add(fSettingsButton)
        .Add(fStatusView)
        .SetInsets(B_USE_DEFAULT_SPACING)
        .End();
}

void ModelSelector::AttachedToWindow()
{
    BView::AttachedToWindow();

    // Set targets
    fProviderMenu->Menu()->SetTargetForItems(this);
    fModelMenu->Menu()->SetTargetForItems(this);
    fSettingsButton->SetTarget(this);

    // Select default provider
    if (fProviderMenu->Menu()->CountItems() > 0) {
        fProviderMenu->Menu()->ItemAt(0)->SetMarked(true);

        // Trigger selection
        BMessage msg(MSG_PROVIDER_SELECTED);
        msg.AddString("provider", fProviderMenu->Menu()->ItemAt(0)->Label());
        MessageReceived(&msg);
    }
}

void ModelSelector::MessageReceived(BMessage* message)
{
    switch (message->what) {
        case MSG_PROVIDER_SELECTED: {
            BString providerName;
            if (message->FindString("provider", &providerName) == B_OK) {
                // Find the provider
                fSelectedProvider = ModelManager::GetInstance()->GetProvider(providerName);

                // Update model menu
                _UpdateModelMenu();

                // [NEW] Directly notify the parent window
                BMessage notifyMsg(B_OBSERVER_NOTICE_CHANGE);
                notifyMsg.AddString("provider_selected", providerName);
                Window()->PostMessage(&notifyMsg);

                // API key check (existing code)
                SettingsManager* settings = SettingsManager::GetInstance();
                BString apiKey = settings->GetApiKey(providerName);

                if (apiKey.IsEmpty()) {
                    fStatusView->SetText(B_TRANSLATE("Please set API key in settings"));
                    fStatusView->SetHighColor(255, 0, 0);
                } else {
                    fStatusView->SetText("");
                }
            }
            break;
        }

		case MSG_MODEL_SELECTED: {
			BString modelName;
			if (message->FindString("model", &modelName) == B_OK && fSelectedProvider != NULL) {
				// Find the model
				BObjectList<LLMModel>* models = fSelectedProvider->GetModels();

				for (int32 i = 0; i < models->CountItems(); i++) {
					if (models->ItemAt(i)->Name() == modelName) {
						fSelectedModel = models->ItemAt(i);

						// [NEW] Notify the parent window about model selection
						BMessage notifyMsg(B_OBSERVER_NOTICE_CHANGE);
						notifyMsg.AddString("model_selected", modelName);
						Window()->PostMessage(&notifyMsg);
						break;
					}
				}

				// Update settings
				if (fSelectedModel != NULL) {
					SettingsManager* settings = SettingsManager::GetInstance();
					settings->SetDefaultModel(fSelectedProvider->Name(), fSelectedModel->Name());
				}
			}
			break;
		}

        case MSG_SETTINGS_CLICKED: {
            // Show settings window
            SettingsWindow* window = new SettingsWindow();
            window->Show();
            break;
        }

        default:
            BView::MessageReceived(message);
            break;
    }
}

void ModelSelector::_LoadProviders()
{
    // Get providers from the model manager
    BMenu* menu = fProviderMenu->Menu(); // Changed BPopUpMenu* to BMenu*
    menu->RemoveItems(0, menu->CountItems());

    BObjectList<LLMProvider>* providers = ModelManager::GetInstance()->GetProviders();

    for (int32 i = 0; i < providers->CountItems(); i++) {
        LLMProvider* provider = providers->ItemAt(i);

        BMessage* message = new BMessage(MSG_PROVIDER_SELECTED);
        message->AddString("provider", provider->Name());

        BMenuItem* item = new BMenuItem(provider->Name(), message);
        menu->AddItem(item);
    }
}

void ModelSelector::_UpdateModelMenu()
{
    // Clear existing items
    BMenu* menu = fModelMenu->Menu(); // Changed BPopUpMenu* to BMenu*
    menu->RemoveItems(0, menu->CountItems());

    // No provider selected
    if (fSelectedProvider == NULL)
        return;

    // Get models from the provider
    BObjectList<LLMModel>* models = fSelectedProvider->GetModels();

    // Get default model from settings
    SettingsManager* settings = SettingsManager::GetInstance();
    BString defaultModel = settings->GetDefaultModel(fSelectedProvider->Name());

    // Add models to menu
    for (int32 i = 0; i < models->CountItems(); i++) {
        LLMModel* model = models->ItemAt(i);

        BMessage* message = new BMessage(MSG_MODEL_SELECTED);
        message->AddString("model", model->Name());

        BMenuItem* item = new BMenuItem(model->Label(), message);
        menu->AddItem(item);

        // Mark default model
        if (model->Name() == defaultModel) {
            item->SetMarked(true);
            fSelectedModel = model;
        }
    }

    // If no default model was marked, mark the first one
    if (fSelectedModel == NULL && menu->CountItems() > 0) {
        menu->ItemAt(0)->SetMarked(true);
        BString modelName;
        menu->ItemAt(0)->Message()->FindString("model", &modelName);

        // Find the model
        for (int32 i = 0; i < models->CountItems(); i++) {
            if (models->ItemAt(i)->Name() == modelName) {
                fSelectedModel = models->ItemAt(i);
                break;
            }
        }
    }
}