// MCPManager.cpp
#include "MCPManager.h"
#include <Path.h>
#include <FindDirectory.h>
#include <Directory.h>
#include <File.h>
#include <OS.h>
#include <stdio.h>

MCPManager* MCPManager::sInstance = NULL;

MCPManager* MCPManager::GetInstance()
{
    if (sInstance == NULL)
        sInstance = new MCPManager();

    return sInstance;
}

MCPManager::MCPManager()
    : fServers(10)  // true for owning pointers
{
}

MCPManager::~MCPManager()
{
    Shutdown();
}

status_t MCPManager::Initialize()
{
    // Load registered servers from settings
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("Otto/mcp_servers");

        BFile file(path.Path(), B_READ_ONLY);
        if (file.InitCheck() == B_OK) {
            BMessage serversMsg;
            if (serversMsg.Unflatten(&file) == B_OK) {
                BString name, command;
                for (int32 i = 0; serversMsg.FindString("server_name", i, &name) == B_OK; i++) {
                    if (serversMsg.FindString("server_command", i, &command) == B_OK) {
                        RegisterServer(name, command);
                    }
                }
            }
        }
    }

    return B_OK;
}

void MCPManager::Shutdown()
{
    // Stop all servers
    for (int32 i = 0; i < fServers.CountItems(); i++) {
        fServers.ItemAt(i)->Stop();
    }

    // Save server list
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append("Otto");

        // Create directory if it doesn't exist
        BDirectory dir(path.Path());
        if (dir.InitCheck() != B_OK)
            create_directory(path.Path(), 0755);

        path.Append("mcp_servers");

        BMessage serversMsg;
        for (int32 i = 0; i < fServers.CountItems(); i++) {
            MCPServer* server = fServers.ItemAt(i);
            serversMsg.AddString("server_name", server->Name());
            serversMsg.AddString("server_command", server->Command());
        }

        BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
        if (file.InitCheck() == B_OK) {
            serversMsg.Flatten(&file);
        }
    }
}

status_t MCPManager::RegisterServer(const BString& name, const BString& command)
{
    // Check if server already exists
    for (int32 i = 0; i < fServers.CountItems(); i++) {
        if (fServers.ItemAt(i)->Name() == name)
            return B_NAME_IN_USE;
    }

    MCPServer* server = new MCPServer(name, command);
    fServers.AddItem(server);

    return B_OK;
}

status_t MCPManager::UnregisterServer(const BString& name)
{
    for (int32 i = 0; i < fServers.CountItems(); i++) {
        if (fServers.ItemAt(i)->Name() == name) {
            fServers.ItemAt(i)->Stop();
            fServers.RemoveItemAt(i);
            return B_OK;
        }
    }

    return B_NAME_NOT_FOUND;
}

MCPServer* MCPManager::GetServer(const BString& name)
{
    for (int32 i = 0; i < fServers.CountItems(); i++) {
        if (fServers.ItemAt(i)->Name() == name)
            return fServers.ItemAt(i);
    }

    return NULL;
}

BObjectList<MCPTool>* MCPManager::GetAllTools()
{
    BObjectList<MCPTool>* allTools = new BObjectList<MCPTool, false>(20);  // false for not owning pointers

    for (int32 i = 0; i < fServers.CountItems(); i++) {
        MCPServer* server = fServers.ItemAt(i);
        if (server->IsActive()) {
            BObjectList<MCPTool>* serverTools = server->Tools();
            for (int32 j = 0; j < serverTools->CountItems(); j++) {
                allTools->AddItem(serverTools->ItemAt(j));
            }
        }
    }

    return allTools;
}

status_t MCPManager::CallTool(const BString& serverName, const BString& toolName,
                            const BMessage& args, BMessage* response)
{
    MCPServer* server = GetServer(serverName);
    if (server == NULL)
        return B_NAME_NOT_FOUND;

    if (!server->IsActive()) {
        status_t status = server->Start();
        if (status != B_OK)
            return status;
    }

    // Find the tool
    MCPTool* tool = NULL;
    BObjectList<MCPTool>* tools = server->Tools();
    for (int32 i = 0; i < tools->CountItems(); i++) {
        if (tools->ItemAt(i)->Name() == toolName) {
            tool = tools->ItemAt(i);
            break;
        }
    }

    if (tool == NULL)
        return B_NAME_NOT_FOUND;

    // Call the tool (implementation depends on MCP protocol details)
    // This is a simplified placeholder
    *response = BMessage(B_REPLY);
    response->AddString("result", "Tool execution placeholder");

    return B_OK;
}

// MCPServer implementation

MCPServer::MCPServer(const BString& name, const BString& command)
    : fName(name)
    , fCommand(command)
    , fTools(10)  // true for owning pointers
    , fServerThread(-1)
    , fIsActive(false)
{
}

MCPServer::~MCPServer()
{
    Stop();
}

status_t MCPServer::Start()
{
    if (fIsActive)
        return B_OK;

    // This is a simplified placeholder for starting the MCP server process
    // In a real implementation, you would:
    // 1. Create pipes for communication
    // 2. Fork and exec the server command
    // 3. Parse tool definitions from the server

    fIsActive = true;

    // Add placeholder tools for demonstration
    MCPTool* tool1 = new MCPTool();
    tool1->SetName("tool1");
    tool1->SetDescription("Example tool 1");
    fTools.AddItem(tool1);

    MCPTool* tool2 = new MCPTool();
    tool2->SetName("tool2");
    tool2->SetDescription("Example tool 2");
    fTools.AddItem(tool2);

    return B_OK;
}

void MCPServer::Stop()
{
    // Stop the server thread if running
    if (fServerThread >= 0) {
        // Implement actual thread termination logic
        fServerThread = -1;
    }

    // Clear active flag
    fIsActive = false;

    // Clear tools
    fTools.MakeEmpty();
}