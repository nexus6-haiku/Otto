// ModelManager.cpp
#include "ModelManager.h"
#include "providers/OpenAIProvider.h"
#include "providers/AnthropicProvider.h"
#include "providers/OllamaProvider.h"

ModelManager* ModelManager::sInstance = NULL;

ModelManager* ModelManager::GetInstance()
{
    if (sInstance == NULL)
        sInstance = new ModelManager();

    return sInstance;
}

ModelManager::ModelManager()
    : fProviders(10)  // true for owning pointers
{
    // Add default providers
    fProviders.AddItem(new OpenAIProvider());
    fProviders.AddItem(new AnthropicProvider());
    fProviders.AddItem(new OllamaProvider());
}

ModelManager::~ModelManager()
{
    // BObjectList will handle deleting the providers
}

void ModelManager::AddProvider(LLMProvider* provider)
{
    fProviders.AddItem(provider);
}

void ModelManager::RemoveProvider(const BString& providerName)
{
    for (int i = 0; i < fProviders.CountItems(); i++) {
        if (fProviders.ItemAt(i)->Name() == providerName) {
            fProviders.RemoveItemAt(i);
            break;
        }
    }
}

LLMProvider* ModelManager::GetProvider(const BString& providerName)
{
    for (int i = 0; i < fProviders.CountItems(); i++) {
        if (fProviders.ItemAt(i)->Name() == providerName)
            return fProviders.ItemAt(i);
    }

    return NULL;
}
