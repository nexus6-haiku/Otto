// LLMProvider.h
#ifndef LLM_PROVIDER_H
#define LLM_PROVIDER_H

#include <String.h>
#include <ObjectList.h>
#include <Messenger.h>
#include "LLMModel.h"
#include "ChatMessage.h"

class LLMProvider {
public:
    LLMProvider(const BString& name);
    virtual ~LLMProvider();

    BString Name() const { return fName; }
    BString ApiBase() const { return fApiBase; }
    void SetApiBase(const BString& apiBase) { fApiBase = apiBase; }

    BString ApiKey() const { return fApiKey; }
    void SetApiKey(const BString& apiKey) { fApiKey = apiKey; }

    // Non-pure virtual methods with default implementations
    virtual BObjectList<LLMModel>* GetModels() { return nullptr; }
    virtual void SendMessage(const BObjectList<ChatMessage, true>& history,
                            const BString& message,
                            BMessenger* messenger) {}

    virtual void CancelRequest() {}

protected:
    BString fName;
    BString fApiBase;
    BString fApiKey;
};

#endif // LLM_PROVIDER_H