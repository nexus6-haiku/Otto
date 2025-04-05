// ModelManager.h
#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include <String.h>
#include <ObjectList.h>
#include "LLMProvider.h"

class ModelManager {
public:
    static ModelManager* GetInstance();
    
    void AddProvider(LLMProvider* provider);
    void RemoveProvider(const BString& providerName);
    LLMProvider* GetProvider(const BString& providerName);
    
    BObjectList<LLMProvider>* GetProviders() { return &fProviders; }
    
private:
    ModelManager();
    ~ModelManager();
    
    static ModelManager* sInstance;
    BObjectList<LLMProvider> fProviders;
};

#endif // MODEL_MANAGER_H
