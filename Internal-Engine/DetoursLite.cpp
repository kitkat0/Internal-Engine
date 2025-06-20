#include "DetoursLite.hpp"
#include <algorithm>
#include <cstring>

namespace InternalEngine {

// Static member definitions
std::vector<DetoursLite::Trampoline> DetoursLite::trampolines;
CRITICAL_SECTION DetoursLite::criticalSection;
bool DetoursLite::initialized = false;

void DetoursLite::Initialize() {
    if (!initialized) {
        InitializeCriticalSection(&criticalSection);
        initialized = true;
    }
}

void DetoursLite::Cleanup() {
    if (initialized) {
        DeleteCriticalSection(&criticalSection);
        initialized = false;
    }
}

bool DetoursLite::InstallHook(uintptr_t targetFunction, uintptr_t detourFunction, uintptr_t* originalFunction, HookType type) {
    Initialize();
    EnterCriticalSection(&criticalSection);
    
    // Check if already hooked
    if (IsHooked(targetFunction)) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    // Auto-select hook type
    if (type == HookType::AUTO) {
        if (is64Bit) {
            type = HookType::JMP_ABSOLUTE;
        } else {
            type = HookType::JMP_RELATIVE;
        }
    }
    
    size_t hookSize = CalculateHookSize(type);
    
    // Calculate minimum bytes to copy
    size_t bytesToCopy = 0;
    while (bytesToCopy < hookSize) {
        size_t instructionLength = GetInstructionLength(targetFunction + bytesToCopy);
        if (instructionLength == 0) {
            LeaveCriticalSection(&criticalSection);
            return false;
        }
        bytesToCopy += instructionLength;
    }
    
    // Create trampoline
    uintptr_t trampolineAddr = CreateTrampoline(targetFunction, bytesToCopy);
    if (trampolineAddr == 0) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    // Save original bytes
    std::vector<uint8_t> originalBytes(bytesToCopy);
    memcpy(originalBytes.data(), reinterpret_cast<void*>(targetFunction), bytesToCopy);
    
    // Build trampoline
    BuildTrampoline(reinterpret_cast<uint8_t*>(trampolineAddr), originalBytes.data(), 
                    bytesToCopy, targetFunction, trampolineAddr);
    
    // Install hook
    DWORD oldProtect;
    if (!SetMemoryProtection(targetFunction, hookSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    bool success = false;
    switch (type) {
        case HookType::JMP_RELATIVE:
            success = WriteJumpRelative(targetFunction, detourFunction);
            break;
        case HookType::JMP_ABSOLUTE:
            success = WriteJumpAbsolute(targetFunction, detourFunction);
            break;
        case HookType::PUSH_RET:
            success = WritePushRet(targetFunction, detourFunction);
            break;
    }
    
    // Fill remaining bytes with NOPs
    if (success && bytesToCopy > hookSize) {
        memset(reinterpret_cast<void*>(targetFunction + hookSize), 0x90, bytesToCopy - hookSize);
    }
    
    SetMemoryProtection(targetFunction, hookSize, oldProtect);
    
    if (success) {
        // Store trampoline info
        Trampoline tramp;
        tramp.originalFunction = targetFunction;
        tramp.trampolineAddress = trampolineAddr;
        tramp.originalSize = bytesToCopy;
        tramp.originalBytes = originalBytes;
        tramp.isActive = true;
        trampolines.push_back(tramp);
        
        if (originalFunction) {
            *originalFunction = trampolineAddr;
        }
    } else {
        VirtualFree(reinterpret_cast<void*>(trampolineAddr), 0, MEM_RELEASE);
    }
    
    LeaveCriticalSection(&criticalSection);
    return success;
}

bool DetoursLite::RemoveHook(uintptr_t targetFunction) {
    Initialize();
    EnterCriticalSection(&criticalSection);
    
    auto it = std::find_if(trampolines.begin(), trampolines.end(),
        [targetFunction](const Trampoline& t) { return t.originalFunction == targetFunction; });
    
    if (it == trampolines.end()) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    // Restore original bytes
    DWORD oldProtect;
    if (!SetMemoryProtection(targetFunction, it->originalSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    memcpy(reinterpret_cast<void*>(targetFunction), it->originalBytes.data(), it->originalSize);
    SetMemoryProtection(targetFunction, it->originalSize, oldProtect);
    
    // Free trampoline memory
    VirtualFree(reinterpret_cast<void*>(it->trampolineAddress), 0, MEM_RELEASE);
    
    // Remove from list
    trampolines.erase(it);
    
    LeaveCriticalSection(&criticalSection);
    return true;
}

bool DetoursLite::EnableHook(uintptr_t targetFunction) {
    Initialize();
    EnterCriticalSection(&criticalSection);
    
    auto it = std::find_if(trampolines.begin(), trampolines.end(),
        [targetFunction](const Trampoline& t) { return t.originalFunction == targetFunction; });
    
    if (it == trampolines.end() || it->isActive) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    it->isActive = true;
    // Re-install hook logic here
    
    LeaveCriticalSection(&criticalSection);
    return true;
}

bool DetoursLite::DisableHook(uintptr_t targetFunction) {
    Initialize();
    EnterCriticalSection(&criticalSection);
    
    auto it = std::find_if(trampolines.begin(), trampolines.end(),
        [targetFunction](const Trampoline& t) { return t.originalFunction == targetFunction; });
    
    if (it == trampolines.end() || !it->isActive) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    // Restore original bytes temporarily
    DWORD oldProtect;
    if (!SetMemoryProtection(targetFunction, it->originalSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LeaveCriticalSection(&criticalSection);
        return false;
    }
    
    memcpy(reinterpret_cast<void*>(targetFunction), it->originalBytes.data(), it->originalSize);
    SetMemoryProtection(targetFunction, it->originalSize, oldProtect);
    
    it->isActive = false;
    
    LeaveCriticalSection(&criticalSection);
    return true;
}

bool DetoursLite::IsHooked(uintptr_t address) {
    EnterCriticalSection(&criticalSection);
    
    bool found = std::any_of(trampolines.begin(), trampolines.end(),
        [address](const Trampoline& t) { return t.originalFunction == address; });
    
    LeaveCriticalSection(&criticalSection);
    return found;
}

std::vector<uintptr_t> DetoursLite::GetActiveHooks() {
    std::vector<uintptr_t> activeHooks;
    
    EnterCriticalSection(&criticalSection);
    
    for (const auto& tramp : trampolines) {
        if (tramp.isActive) {
            activeHooks.push_back(tramp.originalFunction);
        }
    }
    
    LeaveCriticalSection(&criticalSection);
    return activeHooks;
}

size_t DetoursLite::GetInstructionLength(uintptr_t address) {
    return LengthDisassembler::GetLength(reinterpret_cast<const uint8_t*>(address));
}

size_t DetoursLite::CalculateHookSize(HookType type) {
    switch (type) {
        case HookType::JMP_RELATIVE:
            return 5; // E9 XX XX XX XX
        case HookType::JMP_ABSOLUTE:
            return is64Bit ? 14 : 6; // x64: FF 25 00 00 00 00 XX XX XX XX XX XX XX XX
        case HookType::PUSH_RET:
            return is64Bit ? 14 : 6; // x86: 68 XX XX XX XX C3
        default:
            return 5;
    }
}

bool DetoursLite::WriteJumpRelative(uintptr_t from, uintptr_t to) {
    uint8_t jump[5];
    jump[0] = 0xE9; // JMP rel32
    
    int32_t offset = CalculateRelativeOffset(from + 5, to);
    memcpy(&jump[1], &offset, sizeof(int32_t));
    
    memcpy(reinterpret_cast<void*>(from), jump, sizeof(jump));
    return true;
}

bool DetoursLite::WriteJumpAbsolute(uintptr_t from, uintptr_t to) {
    if (is64Bit) {
        // x64: FF 25 00 00 00 00 [8 bytes absolute address]
        uint8_t jump[14];
        jump[0] = 0xFF;
        jump[1] = 0x25;
        memset(&jump[2], 0, 4);
        memcpy(&jump[6], &to, sizeof(uintptr_t));
        
        memcpy(reinterpret_cast<void*>(from), jump, sizeof(jump));
    } else {
        // x86: FF 25 [4 bytes absolute address]
        uint8_t jump[6];
        jump[0] = 0xFF;
        jump[1] = 0x25;
        
        uintptr_t* addressPtr = reinterpret_cast<uintptr_t*>(&jump[2]);
        *addressPtr = reinterpret_cast<uintptr_t>(&to);
        
        memcpy(reinterpret_cast<void*>(from), jump, sizeof(jump));
    }
    return true;
}

bool DetoursLite::WritePushRet(uintptr_t from, uintptr_t to) {
    if (is64Bit) {
        // x64 doesn't support direct PUSH imm64, use MOV + PUSH
        return false;
    } else {
        uint8_t jump[6];
        jump[0] = 0x68; // PUSH imm32
        memcpy(&jump[1], &to, sizeof(uint32_t));
        jump[5] = 0xC3; // RET
        
        memcpy(reinterpret_cast<void*>(from), jump, sizeof(jump));
        return true;
    }
}

uintptr_t DetoursLite::CreateTrampoline(uintptr_t originalFunction, size_t hookSize) {
    // Allocate memory for trampoline (original bytes + jump back)
    size_t trampolineSize = hookSize + (is64Bit ? 14 : 5);
    
    void* trampolineMemory = VirtualAlloc(nullptr, trampolineSize, 
                                         MEM_COMMIT | MEM_RESERVE, 
                                         PAGE_EXECUTE_READWRITE);
    
    if (!trampolineMemory) {
        return 0;
    }
    
    return reinterpret_cast<uintptr_t>(trampolineMemory);
}

void DetoursLite::BuildTrampoline(uint8_t* trampolineBuffer, const uint8_t* originalBytes, 
                                  size_t originalSize, uintptr_t originalAddress, 
                                  uintptr_t trampolineAddress) {
    // Copy original bytes
    memcpy(trampolineBuffer, originalBytes, originalSize);
    
    // Relocate instructions if necessary
    for (size_t i = 0; i < originalSize; ) {
        size_t instructionLength = LengthDisassembler::GetLength(&trampolineBuffer[i]);
        RelocateInstruction(&trampolineBuffer[i], originalAddress + i, trampolineAddress + i);
        i += instructionLength;
    }
    
    // Add jump back to original function + hookSize
    uintptr_t jumpBackAddress = originalAddress + originalSize;
    WriteJumpRelative(trampolineAddress + originalSize, jumpBackAddress);
}

bool DetoursLite::RelocateInstruction(uint8_t* instruction, uintptr_t oldAddress, uintptr_t newAddress) {
    // Check for relative jumps/calls
    if (instruction[0] == 0xE8 || instruction[0] == 0xE9) { // CALL/JMP rel32
        int32_t* offset = reinterpret_cast<int32_t*>(&instruction[1]);
        uintptr_t targetAddress = oldAddress + 5 + *offset;
        *offset = CalculateRelativeOffset(newAddress + 5, targetAddress);
        return true;
    }
    
    // Check for conditional jumps (0x0F 0x8X)
    if (instruction[0] == 0x0F && (instruction[1] & 0xF0) == 0x80) {
        int32_t* offset = reinterpret_cast<int32_t*>(&instruction[2]);
        uintptr_t targetAddress = oldAddress + 6 + *offset;
        *offset = CalculateRelativeOffset(newAddress + 6, targetAddress);
        return true;
    }
    
    return true;
}

int32_t DetoursLite::CalculateRelativeOffset(uintptr_t from, uintptr_t to) {
    return static_cast<int32_t>(to - from);
}

bool DetoursLite::SetMemoryProtection(uintptr_t address, size_t size, DWORD protection, DWORD* oldProtection) {
    DWORD temp;
    return VirtualProtect(reinterpret_cast<void*>(address), size, protection, 
                         oldProtection ? oldProtection : &temp) != 0;
}

// LengthDisassembler implementation
size_t LengthDisassembler::GetLength(const uint8_t* data) {
    size_t offset = 0;
    bool hasRex = false;
    
    // Skip prefixes
    while (IsPrefix(data[offset])) {
        if (IsRexPrefix(data[offset])) {
            hasRex = true;
        }
        offset++;
    }
    
    // Get opcode size and total instruction length
    size_t opcodeSize = GetOpcodeSize(&data[offset], hasRex);
    return offset + opcodeSize;
}

bool LengthDisassembler::IsPrefix(uint8_t byte) {
    // Segment override prefixes
    if (byte == 0x26 || byte == 0x2E || byte == 0x36 || byte == 0x3E ||
        byte == 0x64 || byte == 0x65) {
        return true;
    }
    
    // Operand-size override
    if (byte == 0x66) return true;
    
    // Address-size override
    if (byte == 0x67) return true;
    
    // LOCK, REP, REPNE
    if (byte == 0xF0 || byte == 0xF2 || byte == 0xF3) return true;
    
    // REX prefixes (x64 only)
    if ((byte & 0xF0) == 0x40) return true;
    
    return false;
}

bool LengthDisassembler::IsRexPrefix(uint8_t byte) {
    return (byte & 0xF0) == 0x40;
}

size_t LengthDisassembler::GetOpcodeSize(const uint8_t* data, bool hasRex) {
    uint8_t opcode = data[0];
    size_t size = 1;
    
    // Two-byte opcodes
    if (opcode == 0x0F) {
        opcode = data[1];
        size = 2;
        
        // Three-byte opcodes
        if (opcode == 0x38 || opcode == 0x3A) {
            size = 3;
        }
    }
    
    // Simple opcodes without ModRM
    if ((opcode >= 0x50 && opcode <= 0x5F) || // PUSH/POP
        (opcode >= 0x90 && opcode <= 0x97) || // XCHG/NOP
        (opcode >= 0xA0 && opcode <= 0xA3)) { // MOV AL/AX/EAX
        return size;
    }
    
    // Immediate values
    if (opcode == 0xE8 || opcode == 0xE9) { // CALL/JMP rel32
        return size + 4;
    }
    
    if (opcode == 0xEB) { // JMP rel8
        return size + 1;
    }
    
    // Instructions with ModRM byte
    bool hasModRM = false;
    if ((opcode & 0xFC) == 0x80 || // Group 1
        (opcode & 0xFE) == 0x88 || // MOV
        (opcode & 0xFC) == 0x8C || // MOV seg
        (opcode & 0xFE) == 0xC6 || // MOV imm
        (opcode & 0xFC) == 0xD0 || // Shift group
        (opcode & 0xFE) == 0xF6 || // Group 3
        (opcode & 0xFE) == 0xFE) { // INC/DEC group
        hasModRM = true;
    }
    
    if (hasModRM) {
        uint8_t modRM = data[size];
        size++;
        size += GetModRMSize(modRM, hasRex);
        
        // Check for immediate values after ModRM
        if ((opcode & 0xFC) == 0x80 || (opcode & 0xFE) == 0xC6) {
            if ((opcode & 1) == 0) {
                size += 1; // 8-bit immediate
            } else {
                size += 4; // 32-bit immediate
            }
        }
    }
    
    return size;
}

size_t LengthDisassembler::GetModRMSize(uint8_t modRM, bool hasRex) {
    uint8_t mod = (modRM >> 6) & 0x03;
    uint8_t rm = modRM & 0x07;
    size_t size = 0;
    
    if (mod == 0x00) {
        if (rm == 0x05) {
            size += 4; // disp32
        } else if (rm == 0x04) {
            size += 1; // SIB byte
        }
    } else if (mod == 0x01) {
        size += 1; // disp8
        if (rm == 0x04) {
            size += 1; // SIB byte
        }
    } else if (mod == 0x02) {
        size += 4; // disp32
        if (rm == 0x04) {
            size += 1; // SIB byte
        }
    }
    
    return size;
}

} // namespace InternalEngine