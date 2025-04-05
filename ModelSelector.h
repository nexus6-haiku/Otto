// ModelSelector.h
#ifndef MODEL_SELECTOR_H
#define MODEL_SELECTOR_H

#include <View.h>
#include <MenuField.h>
#include <PopUpMenu.h>
#include <Button.h>
#include <StringView.h>
#include "LLMProvider.h"
#include "LLMModel.h"

const uint32 MSG_PROVIDER_SELECTED = 'prvs';
const uint32 MSG_MODEL_SELECTED = 'mdls';
const uint32 MSG_SETTINGS_CLICKED = 'stng';

class ModelSelector : public BView {
public:
    ModelSelector();
    virtual ~ModelSelector();
    
    virtual void AttachedToWindow();
    virtual void MessageReceived(BMessage* message);
    
    LLMProvider* SelectedProvider() const { return fSelectedProvider; }
    LLMModel* SelectedModel() const { return fSelectedModel; }
    
private:
    void _BuildLayout();
    void _LoadProviders();
    void _UpdateModelMenu();
    
    BMenuField* fProviderMenu;
    BMenuField* fModelMenu;
    BButton* fSettingsButton;
    BStringView* fStatusView;
    
    LLMProvider* fSelectedProvider;
    LLMModel* fSelectedModel;
};

#endif // MODEL_SELECTOR_H
