// providers/OllamaProvider.h
#ifndef OLLAMA_PROVIDER_H
#define OLLAMA_PROVIDER_H

#include <String.h>
#include <ObjectList.h>
#include <Messenger.h>
#include <NetEndpoint.h>
#include "LLMProvider.h"

class OllamaProvider : public LLMProvider {
public:
    OllamaProvider();
    virtual ~OllamaProvider();

    virtual BObjectList<LLMModel>* GetModels();
    virtual void SendMessage(const BObjectList<ChatMessage, true>& history,
                            const BString& message,
                            BMessenger* messenger);

    virtual void CancelRequest();

    status_t FetchAvailableModels();

private:
    void _InitModels();
    static int32 _RequestThreadFunc(void* data);

    BObjectList<LLMModel> fModels;
    thread_id fRequestThread;
    bool fCancelRequested;
};

#endif // OLLAMA_PROVIDER_H