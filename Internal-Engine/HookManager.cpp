#include "HookManager.hpp"
#include "MemoryEngine.hpp"
#include <algorithm>

namespace InternalEngine {

// Global instance
HookManager* g_HookManager = nullptr;

HookManager::HookManager() : initialized(false) {
    Initialize();
}

HookManager::~HookManager() {
    RemoveAllHooks();
    Cleanup();
}

void HookManager::Initialize() {
    if (!initialized) {
        InitializeCriticalSection(&criticalSection);
        initialized = true;
    }
}

void HookManager::Cleanup() {
    if (initialized) {
        DeleteCriticalSection(&criticalSection);
        initialized = false;
    }
}

bool HookManager::InstallHook(const std::string& name, uintptr_t targetAddress, uintptr_t detourAddress, 
                              DetoursLite::HookType type) {
    EnterCriticalSection(&criticalSection);
    
    // Check if hook with this name already exists
    if (hooks.find(name) != hooks.end()) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    // Check if address is already hooked
    if (addressToName.find(targetAddress) != addressToName.end()) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    // Create hook info
    auto hookInfo = std::make_unique<HookInfo>();
    hookInfo->name = name;
    hookInfo->targetAddress = targetAddress;
    hookInfo->detourAddress = detourAddress;
    hookInfo->type = type;
    hookInfo->isActive = true;
    
    // Install the hook
    uintptr_t originalFunction = 0;
    bool success = DetoursLite::InstallHook(targetAddress, detourAddress, &originalFunction, type);
    
    if (success) {
        hookInfo->originalAddress = originalFunction;
        addressToName[targetAddress] = name;
        hooks[name] = std::move(hookInfo);
    }
    
    LeaveCriticalSection(&criticalSection);
    return success;
}

bool HookManager::InstallHook(const std::string& name, void* targetFunction, void* detourFunction,
                              DetoursLite::HookType type) {
    return InstallHook(name, reinterpret_cast<uintptr_t>(targetFunction), 
                      reinterpret_cast<uintptr_t>(detourFunction), type);
}

bool HookManager::RemoveHook(const std::string& name) {
    EnterCriticalSection(&criticalSection);
    
    auto it = hooks.find(name);
    if (it == hooks.end()) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    bool success = DetoursLite::RemoveHook(it->second->targetAddress);
    
    if (success) {
        addressToName.erase(it->second->targetAddress);
        hooks.erase(it);
    }
    
    LeaveCriticalSection(&criticalSection);
    return success;
}

bool HookManager::RemoveAllHooks() {
    EnterCriticalSection(&criticalSection);
    
    bool allSuccess = true;
    std::vector<std::string> hookNames;
    
    // Collect all hook names
    for (const auto& pair : hooks) {
        hookNames.push_back(pair.first);
    }
    
    LeaveCriticalSection(&criticalSection);
    
    // Remove hooks one by one
    for (const auto& name : hookNames) {
        if (!RemoveHook(name)) {
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

bool HookManager::EnableHook(const std::string& name) {
    EnterCriticalSection(&criticalSection);
    
    auto it = hooks.find(name);
    if (it == hooks.end()) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    bool success = DetoursLite::EnableHook(it->second->targetAddress);
    if (success) {
        it->second->isActive = true;
    }
    
    LeaveCriticalSection(&criticalSection);
    return success;
}

bool HookManager::DisableHook(const std::string& name) {
    EnterCriticalSection(&criticalSection);
    
    auto it = hooks.find(name);
    if (it == hooks.end()) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    bool success = DetoursLite::DisableHook(it->second->targetAddress);
    if (success) {
        it->second->isActive = false;
    }
    
    LeaveCriticalSection(&criticalSection);
    return success;
}

bool HookManager::ToggleHook(const std::string& name) {
    EnterCriticalSection(&criticalSection);
    
    auto it = hooks.find(name);
    if (it == hooks.end()) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    bool isActive = it->second->isActive;
    LeaveCriticalSection(&criticalSection);
    
    return isActive ? DisableHook(name) : EnableHook(name);
}

bool HookManager::IsHooked(const std::string& name) const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    
    bool found = hooks.find(name) != hooks.end();
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    return found;
}

bool HookManager::IsHooked(uintptr_t address) const {
    return DetoursLite::IsHooked(address);
}

uintptr_t HookManager::GetOriginalFunction(const std::string& name) const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    
    auto it = hooks.find(name);
    uintptr_t original = 0;
    
    if (it != hooks.end()) {
        original = it->second->originalAddress;
    }
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    return original;
}

HookManager::HookInfo HookManager::GetHookInfo(const std::string& name) const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    
    HookInfo info = {};
    auto it = hooks.find(name);
    
    if (it != hooks.end()) {
        info = *it->second;
    }
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    return info;
}

std::vector<HookManager::HookInfo> HookManager::GetAllHooks() const {
    EnterCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    
    std::vector<HookInfo> allHooks;
    for (const auto& pair : hooks) {
        allHooks.push_back(*pair.second);
    }
    
    LeaveCriticalSection(const_cast<CRITICAL_SECTION*>(&criticalSection));
    return allHooks;
}

uintptr_t HookManager::GetFunctionAddress(const std::string& moduleName, const std::string& functionName) {
    HMODULE hModule = GetModuleHandleA(moduleName.c_str());
    if (!hModule) {
        hModule = LoadLibraryA(moduleName.c_str());
    }
    
    if (!hModule) {
        return 0;
    }
    
    return reinterpret_cast<uintptr_t>(GetProcAddress(hModule, functionName.c_str()));
}

uintptr_t HookManager::GetVTableFunction(uintptr_t objectPtr, size_t index) {
    if (!objectPtr) {
        return 0;
    }
    
    // Get vtable pointer
    uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(objectPtr);
    if (!vtable) {
        return 0;
    }
    
    // Get function pointer from vtable
    return vtable[index];
}

bool HookManager::InstallHookByPattern(const std::string& name, const std::string& pattern, 
                                       const std::string& mask, uintptr_t detourAddress,
                                       DetoursLite::HookType type) {
    // Find pattern
    auto address = MemoryEngine::PatternScanFirst(pattern, mask);
    if (!address.has_value()) {
        return false;
    }
    
    return InstallHook(name, address.value(), detourAddress, type);
}

} // namespace InternalEngine