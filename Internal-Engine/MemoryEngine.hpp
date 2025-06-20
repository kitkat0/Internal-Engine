#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <functional>

namespace InternalEngine {

// 메모리 영역 정보 구조체
struct MemoryRegion {
    uintptr_t baseAddress;
    size_t size;
    DWORD protection;
    DWORD state;
    DWORD type;
    std::string moduleName;
    bool readable;
    bool writable;
    bool executable;
};

// Tri-state enum for filtering memory properties
enum class TriState {
    Any,
    Yes,
    No
};

// 스캔 결과 구조체
struct ScanResult {
    uintptr_t address;
    std::vector<uint8_t> value;
    std::string type;
    std::vector<uint8_t> previousValue; // For next scans
};

// 메모리 스캔 옵션
struct ScanOptions {
    uintptr_t startAddress = 0;
    uintptr_t endAddress = 0;
    size_t alignment = 1;
    
    // Memory protection filters
    TriState filterWritable = TriState::Yes;
    TriState filterExecutable = TriState::Any;
    TriState filterCopyOnWrite = TriState::Any;

    // Advanced options
    bool isFirstScan = true;
    bool caseSensitive = true;
    std::string luaFilter;

    // For next scans
    const std::vector<ScanResult>* previousResults = nullptr;
};

class MemoryEngine {
public:
    MemoryEngine();
    ~MemoryEngine();
    
    // 🛡️ 안전한 메모리 읽기/쓰기
    static bool IsAddressValid(uintptr_t address, size_t size = 1);
    static bool IsAddressReadable(uintptr_t address, size_t size = 1);
    static bool IsAddressWritable(uintptr_t address, size_t size = 1);
    static std::vector<uint8_t> SafeReadBytes(uintptr_t address, size_t size);
    static bool SafeWriteBytes(uintptr_t address, const std::vector<uint8_t>& bytes);
    
    // 📖 기본 메모리 읽기/쓰기 (템플릿)
    template<typename T>
    static std::optional<T> SafeRead(uintptr_t address) {
        if (!IsAddressReadable(address, sizeof(T))) {
            return std::nullopt;
        }
        
        __try {
            T value;
            memcpy(&value, reinterpret_cast<void*>(address), sizeof(T));
            return value;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            return std::nullopt;
        }
    }
    
    template<typename T>
    static bool SafeWrite(uintptr_t address, const T& value) {
        if (!IsAddressWritable(address, sizeof(T))) {
            return false;
        }
        
        __try {
            DWORD oldProtect;
            if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                return false;
            }
            
            memcpy(reinterpret_cast<void*>(address), &value, sizeof(T));
            VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), oldProtect, &oldProtect);
            return true;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }
    
    // 레거시 함수들 (public으로 이동)
    static std::vector<uint8_t> ReadBytes(uintptr_t address, size_t size);
    static bool WriteBytes(uintptr_t address, const std::vector<uint8_t>& bytes);
    
    template<typename T>
    static T Read(uintptr_t address) {
        auto result = SafeRead<T>(address);
        return result.value_or(T{});
    }
    
    template<typename T>
    static bool Write(uintptr_t address, const T& value) {
        return SafeWrite<T>(address, value);
    }
    
    // 🔍 고급 메모리 스캔
    static std::vector<ScanResult> ScanForValue(const std::vector<uint8_t>& value, const ScanOptions& options = {});
    static std::vector<ScanResult> ScanForInt32(int32_t value, const ScanOptions& options = {});
    static std::vector<ScanResult> ScanForFloat(float value, const ScanOptions& options = {});
    static std::vector<ScanResult> ScanForDouble(double value, const ScanOptions& options = {});
    static std::vector<ScanResult> ScanForString(const std::string& value, bool caseSensitive = true, const ScanOptions& options = {});
    
    // 🎯 패턴 스캔 (개선된 버전)
    static std::vector<uintptr_t> PatternScanAll(const std::string& pattern, const std::string& mask, uintptr_t start = 0, uintptr_t end = 0);
    static std::optional<uintptr_t> PatternScanFirst(const std::string& pattern, const std::string& mask, uintptr_t start = 0, uintptr_t end = 0);
    static std::vector<uintptr_t> AOBScanAll(const std::string& pattern, uintptr_t start = 0, uintptr_t end = 0);
    static std::optional<uintptr_t> AOBScanFirst(const std::string& pattern, uintptr_t start = 0, uintptr_t end = 0);
    
    // 레거시 패턴 스캔 (public으로 이동)
    static std::optional<uintptr_t> PatternScan(const std::string& pattern, const std::string& mask, uintptr_t start = 0, uintptr_t end = 0);
    static std::optional<uintptr_t> AOBScan(const std::string& pattern, uintptr_t start = 0, uintptr_t end = 0);
    
    // 🗺️ 메모리 영역 관리
    static std::vector<MemoryRegion> GetMemoryRegions();
    static std::optional<MemoryRegion> GetMemoryRegion(uintptr_t address);
    static std::vector<MemoryRegion> GetExecutableRegions();
    static std::vector<MemoryRegion> GetWritableRegions();
    
    // 🔧 메모리 보호 및 할당
    static bool ChangeProtection(uintptr_t address, size_t size, DWORD newProtect, DWORD* oldProtect = nullptr);
    static uintptr_t AllocateMemory(size_t size, DWORD protection = PAGE_EXECUTE_READWRITE);
    static bool FreeMemory(uintptr_t address);
    
    // 📦 모듈 관리
    static uintptr_t GetModuleBase(const std::string& moduleName = "");
    static size_t GetModuleSize(const std::string& moduleName = "");
    static std::vector<std::string> GetLoadedModules();
    static std::optional<MemoryRegion> GetModuleRegion(const std::string& moduleName);
    
    // 🛠️ 유틸리티 함수
    static std::string BytesToHexString(const std::vector<uint8_t>& bytes);
    static std::vector<uint8_t> HexStringToBytes(const std::string& hexString);
    static std::vector<uint8_t> PatternToBytes(const std::string& pattern);
    static std::string BytesToPattern(const std::vector<uint8_t>& bytes);
    
    // 🔄 값 변환 함수
    static std::string ValueToString(const std::vector<uint8_t>& bytes, const std::string& type);
    static std::vector<uint8_t> StringToValue(const std::string& value, const std::string& type);
    
    // 🎮 게임 해킹 전용 함수
    static bool NopInstruction(uintptr_t address, size_t size);
    static bool PatchBytes(uintptr_t address, const std::vector<uint8_t>& originalBytes, const std::vector<uint8_t>& newBytes);
    static bool RestoreBytes(uintptr_t address, const std::vector<uint8_t>& originalBytes);
    
    // 🧠 스마트 포인터 체이닝
    static std::optional<uintptr_t> FollowPointerChain(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets);
    static std::vector<uintptr_t> FindPointersTo(uintptr_t targetAddress, const ScanOptions& options = {});
    
    // 레거시 NOP 함수 (public으로 이동)
    static bool Nop(uintptr_t address, size_t size);

    // Core scanning functions
    static std::vector<ScanResult> FirstScan(const std::string& value, const std::string& type, const ScanOptions& options);
    static std::vector<ScanResult> NextScan(const std::string& scanType, const std::string& value, const ScanOptions& options);
    static bool IsRegionScannable(const MemoryRegion& region, const ScanOptions& options);
    
    // Enhanced reading functions for disassembler
    static std::optional<std::vector<uint8_t>> ReadValueAtAddress(uintptr_t address, const std::string& type);
    static int CompareValues(const std::vector<uint8_t>& value1, const std::vector<uint8_t>& value2, const std::string& type);

private:
    // 내부 헬퍼 함수들
    static bool ComparePattern(const uint8_t* data, const std::string& pattern, const std::string& mask);
    static bool CompareBytes(const uint8_t* data, const std::vector<uint8_t>& pattern, size_t size);
    static DWORD GetProtectionFlags(uintptr_t address);
};

} // namespace InternalEngine 