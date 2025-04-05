// MCPTool.h
#ifndef MCP_TOOL_H
#define MCP_TOOL_H

#include <String.h>
#include <Message.h>

class MCPTool {
public:
   MCPTool();
   ~MCPTool();
   
   BString Name() const { return fName; }
   void SetName(const BString& name) { fName = name; }
   
   BString Description() const { return fDescription; }
   void SetDescription(const BString& description) { fDescription = description; }
   
   BMessage* InputSchema() const { return fInputSchema; }
   void SetInputSchema(BMessage* schema) { fInputSchema = schema; }
   
   status_t Call(const BMessage& args, BMessage* response);
   
private:
   BString fName;
   BString fDescription;
   BMessage* fInputSchema;
};

#endif // MCP_TOOL_H
