// providers/OpenAIProvider.h
#ifndef OPENAI_PROVIDER_H
#define OPENAI_PROVIDER_H

#include <String.h>
#include <ObjectList.h>
#include <Messenger.h>
#include <NetEndpoint.h>
#include "LLMProvider.h"

class OpenAIProvider : public LLMProvider {
public:
    OpenAIProvider();
    virtual ~OpenAIProvider();
    
    virtual BObjectList<LLMModel>* GetModels();
    virtual void SendMessage(const BObjectList<ChatMessage>& history, 
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

#endif // OPENAI_PROVIDER_H
