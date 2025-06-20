#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>

namespace InternalEngine {

// Command handler function type
using CommandHandler = std::function<std::string(const std::string& params)>;

class CommandRouter {
public:
    CommandRouter();
    ~CommandRouter();

    // Command registration
    void RegisterCommand(const std::string& command, CommandHandler handler);
    void UnregisterCommand(const std::string& command);
    
    // Command execution
    std::string ExecuteCommand(const std::string& jsonRequest);
    
    // Built-in commands
    void RegisterBuiltinCommands();

private:
    std::unordered_map<std::string, CommandHandler> commands;
    
    // JSON parsing helpers
    std::string ParseCommand(const std::string& json);
    std::string ParseParams(const std::string& json);
    std::string CreateResponse(bool success, const std::string& data, const std::string& error = "", const std::string& id = "");
    
    // Built-in command handlers
    std::string HandleMemoryRead(const std::string& params);
    std::string HandleMemoryWrite(const std::string& params);
    std::string HandleMemoryScan(const std::string& params);
    std::string HandleMemoryRegions(const std::string& params);
    std::string HandleMemoryValidate(const std::string& params);
    std::string HandleMemoryPatch(const std::string& params);
    std::string HandleMemoryNop(const std::string& params);
    std::string HandlePatternScan(const std::string& params);
    std::string HandlePatternScanAll(const std::string& params);
    std::string HandleModuleList(const std::string& params);
    std::string HandleModuleInfo(const std::string& params);
    std::string HandleProcessInfo(const std::string& params);
    std::string HandleHookInstall(const std::string& params);
    std::string HandleHookRemove(const std::string& params);
    std::string HandleHookList(const std::string& params);
    std::string HandleHookToggle(const std::string& params);
    std::string HandleAllocateMemory(const std::string& params);
    std::string HandleFreeMemory(const std::string& params);
    std::string HandlePointerChain(const std::string& params);
    std::string HandlePointerFind(const std::string& params);
    
    // New enhanced commands
    std::string HandleMemoryReadValue(const std::string& params);
    std::string HandleMemoryReadBulk(const std::string& params);
    std::string HandleMemoryDisassemble(const std::string& params);
    std::string HandleModuleFromAddress(const std::string& params);
};

// Global command router instance
extern CommandRouter* g_CommandRouter;

} // namespace InternalEngine 