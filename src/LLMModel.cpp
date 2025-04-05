// LLMModel.cpp
#include "LLMModel.h"

LLMModel::LLMModel(const BString& name, const BString& label)
    : fName(name)
    , fLabel(label)
    , fContextWindow(4096)
    , fMaxTokens(2048)
    , fSupportsVision(false)
    , fSupportsTools(false)
{
}

LLMModel::~LLMModel()
{
}
