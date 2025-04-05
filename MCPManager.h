// MCPManager.h
#ifndef MCP_MANAGER_H
#define MCP_MANAGER_H

#include <String.h>
#include <ObjectList.h>
#include <Messenger.h>
#include "MCPTool.h"

class MCPServer {
public:
    MCPServer(const BString& name, const BString& command);
    ~MCPServer();
    
    BString Name() const { return fName; }
    BString Command() const { return fCommand; }
    BObjectList<MCPTool>* Tools() { return &fTools; }
    
    bool IsActive() const { return fIsActive; }
    status_t Start();
    void Stop();
    
private:
    BString fName;
    BString fCommand;
    BObjectList<MCPTool> fTools;
    thread_id fServerThread;
    bool fIsActive;
};

class MCPManager {
public:
    static MCPManager* GetInstance();
    
    status_t Initialize();
    void Shutdown();
    
    status_t RegisterServer(const BString& name, const BString& command);
    status_t UnregisterServer(const BString& name);
    MCPServer* GetServer(const BString& name);
    
    BObjectList<MCPServer>* GetServers() { return &fServers; }
    BObjectList<MCPTool>* GetAllTools();
    
    status_t CallTool(const BString& serverName, const BString& toolName, 
                     const BMessage& args, BMessage* response);
    
private:
    MCPManager();
    ~MCPManager();
    
    static MCPManager* sInstance;
    BObjectList<MCPServer> fServers;
};

#endif // MCP_MANAGER_H
