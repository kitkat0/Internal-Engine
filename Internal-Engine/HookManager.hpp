#pragma once
#include <Windows.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "DetoursLite.hpp"

namespace InternalEngine {

class HookManager {
public:
    struct HookInfo {
        std::string name;
        uintptr_t targetAddress;
        uintptr_t detourAddress;
        uintptr_t originalAddress;
        bool isActive;
        DetoursLite::HookType type;
    };

    HookManager();
    ~HookManager();

    // Hook installation
    bool InstallHook(const std::string& name, uintptr_t targetAddress, uintptr_t detourAddress, 
                     DetoursLite::HookType type = DetoursLite::HookType::AUTO);
    
    bool InstallHook(const std::string& name, void* targetFunction, void* detourFunction,
                     DetoursLite::HookType type = DetoursLite::HookType::AUTO);
    
    // Template version for function pointers
    template<typename T>
    bool InstallHook(const std::string& name, T targetFunction, T detourFunction, T* originalFunction = nullptr) {
        uintptr_t original = 0;
        bool result = InstallHook(name, reinterpret_cast<void*>(targetFunction), 
                                 reinterpret_cast<void*>(detourFunction));
        if (result && originalFunction) {
            *originalFunction = reinterpret_cast<T>(GetOriginalFunction(name));
        }
        return result;
    }

    // Hook removal
    bool RemoveHook(const std::string& name);
    bool RemoveAllHooks();

    // Hook control
    bool EnableHook(const std::string& name);
    bool DisableHook(const std::string& name);
    bool ToggleHook(const std::string& name);

    // Hook information
    bool IsHooked(const std::string& name) const;
    bool IsHooked(uintptr_t address) const;
    uintptr_t GetOriginalFunction(const std::string& name) const;
    HookInfo GetHookInfo(const std::string& name) const;
    std::vector<HookInfo> GetAllHooks() const;

    // Utility functions
    static uintptr_t GetFunctionAddress(const std::string& moduleName, const std::string& functionName);
    static uintptr_t GetVTableFunction(uintptr_t objectPtr, size_t index);
    
    // Pattern-based hooking
    bool InstallHookByPattern(const std::string& name, const std::string& pattern, 
                             const std::string& mask, uintptr_t detourAddress,
                             DetoursLite::HookType type = DetoursLite::HookType::AUTO);

private:
    std::unordered_map<std::string, std::unique_ptr<HookInfo>> hooks;
    std::unordered_map<uintptr_t, std::string> addressToName;
    CRITICAL_SECTION criticalSection;
    bool initialized;

    void Initialize();
    void Cleanup();
};

// Global hook manager instance
extern HookManager* g_HookManager;

// Macro helpers for easier hook creation
#define HOOK_FUNCTION(name, target, detour) \
    g_HookManager->InstallHook(name, target, detour)

#define HOOK_VTABLE(name, object, index, detour) \
    g_HookManager->InstallHook(name, \
        reinterpret_cast<void*>(HookManager::GetVTableFunction(reinterpret_cast<uintptr_t>(object), index)), \
        reinterpret_cast<void*>(detour))

} // namespace InternalEngine 