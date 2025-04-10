// providers/AnthropicProvider.h
#ifndef ANTHROPIC_PROVIDER_H
#define ANTHROPIC_PROVIDER_H

#include <String.h>
#include <ObjectList.h>
#include <Messenger.h>
#include <NetEndpoint.h>
#include "LLMProvider.h"

class AnthropicProvider : public LLMProvider {
public:
    AnthropicProvider();
    virtual ~AnthropicProvider();

    virtual BObjectList<LLMModel>* GetModels();
    virtual void SendMessage(const BObjectList<ChatMessage, true>& history,
                            const BString& message,
                            BMessenger* messenger);

    virtual void CancelRequest();

private:
    void _InitModels();
    static int32 _RequestThreadFunc(void* data);

    BObjectList<LLMModel> fModels;
    thread_id fRequestThread;
    bool fCancelRequested;
};

#endif // ANTHROPIC_PROVIDER_H