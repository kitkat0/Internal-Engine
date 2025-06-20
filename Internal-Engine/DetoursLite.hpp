#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>

namespace InternalEngine {

// x86/x64 compatible Detours implementation
class DetoursLite {
public:
    // Trampoline structure for storing original bytes and jump back
    struct Trampoline {
        uintptr_t originalFunction;
        uintptr_t trampolineAddress;
        size_t originalSize;
        std::vector<uint8_t> originalBytes;
        bool isActive;
    };

    // Hook types
    enum class HookType {
        JMP_RELATIVE,    // E9 XX XX XX XX (5 bytes on x86, needs relay on x64)
        JMP_ABSOLUTE,    // FF 25 XX XX XX XX (6 bytes on x86, 14 bytes on x64)
        PUSH_RET,        // 68 XX XX XX XX C3 (6 bytes on x86)
        AUTO             // Automatically choose best method
    };

    // Install a hook
    static bool InstallHook(uintptr_t targetFunction, uintptr_t detourFunction, uintptr_t* originalFunction, HookType type = HookType::AUTO);
    
    // Remove a hook
    static bool RemoveHook(uintptr_t targetFunction);
    
    // Enable/Disable hook temporarily
    static bool EnableHook(uintptr_t targetFunction);
    static bool DisableHook(uintptr_t targetFunction);
    
    // Check if address is hooked
    static bool IsHooked(uintptr_t address);
    
    // Get all active hooks
    static std::vector<uintptr_t> GetActiveHooks();

private:
    // Disassembler helpers
    static size_t GetInstructionLength(uintptr_t address);
    static size_t CalculateHookSize(HookType type);
    static bool IsRelativeJump(uint8_t* instruction);
    static bool IsAbsoluteJump(uint8_t* instruction);
    
    // Hook installation helpers
    static bool WriteJumpRelative(uintptr_t from, uintptr_t to);
    static bool WriteJumpAbsolute(uintptr_t from, uintptr_t to);
    static bool WritePushRet(uintptr_t from, uintptr_t to);
    
    // Trampoline creation
    static uintptr_t CreateTrampoline(uintptr_t originalFunction, size_t hookSize);
    static void BuildTrampoline(uint8_t* trampolineBuffer, const uint8_t* originalBytes, size_t originalSize, uintptr_t originalAddress, uintptr_t trampolineAddress);
    
    // Relocation helpers
    static bool RelocateInstruction(uint8_t* instruction, uintptr_t oldAddress, uintptr_t newAddress);
    static int32_t CalculateRelativeOffset(uintptr_t from, uintptr_t to);
    
    // Memory protection
    static bool SetMemoryProtection(uintptr_t address, size_t size, DWORD protection, DWORD* oldProtection = nullptr);
    
    // Hook storage
    static std::vector<Trampoline> trampolines;
    static CRITICAL_SECTION criticalSection;
    static bool initialized;
    
    // Initialize critical section
    static void Initialize();
    static void Cleanup();
    
    // Architecture detection
#ifdef _WIN64
    static constexpr bool is64Bit = true;
#else
    static constexpr bool is64Bit = false;
#endif
};

// Simple length disassembler for x86/x64
class LengthDisassembler {
public:
    static size_t GetLength(const uint8_t* data);
    
private:
    // Instruction prefixes
    static bool IsPrefix(uint8_t byte);
    static bool IsRexPrefix(uint8_t byte);
    
    // ModRM byte parsing
    static size_t GetModRMSize(uint8_t modRM, bool hasRex = false);
    static bool HasSIB(uint8_t modRM);
    static size_t GetSIBSize(uint8_t sib, uint8_t mod);
    
    // Opcode tables
    static size_t GetOpcodeSize(const uint8_t* data, bool hasRex = false);
};

} // namespace InternalEngine 