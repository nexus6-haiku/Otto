// LLMModel.h
#ifndef LLM_MODEL_H
#define LLM_MODEL_H

#include <String.h>

class LLMModel {
public:
    LLMModel(const BString& name, const BString& label);
    virtual ~LLMModel();
    
    BString Name() const { return fName; }
    BString Label() const { return fLabel; }
    
    int32 ContextWindow() const { return fContextWindow; }
    void SetContextWindow(int32 window) { fContextWindow = window; }
    
    int32 MaxTokens() const { return fMaxTokens; }
    void SetMaxTokens(int32 tokens) { fMaxTokens = tokens; }
    
    bool SupportsVision() const { return fSupportsVision; }
    void SetSupportsVision(bool supports) { fSupportsVision = supports; }
    
    bool SupportsTools() const { return fSupportsTools; }
    void SetSupportsTools(bool supports) { fSupportsTools = supports; }
    
private:
    BString fName;
    BString fLabel;
    int32 fContextWindow;
    int32 fMaxTokens;
    bool fSupportsVision;
    bool fSupportsTools;
};

#endif // LLM_MODEL_H
