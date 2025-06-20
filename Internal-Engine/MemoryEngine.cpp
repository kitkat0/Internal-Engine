#include "MemoryEngine.hpp"
#include <Psapi.h>
#include <TlHelp32.h>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstring>

namespace InternalEngine {

MemoryEngine::MemoryEngine() {}
MemoryEngine::~MemoryEngine() {}

// 🛡️ 안전한 메모리 접근 확인
bool MemoryEngine::IsAddressValid(uintptr_t address, size_t size) {
    if (address == 0) return false;
    
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == 0) {
        return false;
    }
    
    // 커밋된 메모리인지 확인
    if (mbi.State != MEM_COMMIT) {
        return false;
    }
    
    // 주소 범위가 유효한지 확인
    uintptr_t endAddress = address + size - 1;
    uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize - 1;
    
    return endAddress <= regionEnd;
}

bool MemoryEngine::IsAddressReadable(uintptr_t address, size_t size) {
    if (!IsAddressValid(address, size)) return false;
    
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi));
    
    // 읽기 가능한 보호 속성 확인
    return (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | 
                          PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
}

bool MemoryEngine::IsAddressWritable(uintptr_t address, size_t size) {
    if (!IsAddressValid(address, size)) return false;
    
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi));
    
    // 쓰기 가능한 보호 속성 확인
    return (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | 
                          PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
}

// SEH 전용 헬퍼 함수 (C++ 객체 없음)
static bool SafeMemcpy(void* dest, const void* src, size_t size) {
    __try {
        memcpy(dest, src, size);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

std::vector<uint8_t> MemoryEngine::SafeReadBytes(uintptr_t address, size_t size) {
    std::vector<uint8_t> buffer;
    
    if (!IsAddressReadable(address, size)) {
        return buffer; // 빈 벡터 반환
    }
    
    buffer.resize(size);
    
    // SEH를 별도 함수로 분리하여 C++ 객체 해제 문제 해결
    if (!SafeMemcpy(buffer.data(), reinterpret_cast<void*>(address), size)) {
        buffer.clear(); // 예외 발생 시 빈 벡터 반환
    }
    
    return buffer;
}

// SEH 전용 쓰기 헬퍼 함수 (C++ 객체 없음)
static bool SafeWriteMemory(uintptr_t address, const void* data, size_t size) {
    __try {
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            return false;
        }
        
        memcpy(reinterpret_cast<void*>(address), data, size);
        VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool MemoryEngine::SafeWriteBytes(uintptr_t address, const std::vector<uint8_t>& bytes) {
    if (bytes.empty() || !IsAddressValid(address, bytes.size())) {
        return false;
    }
    
    return SafeWriteMemory(address, bytes.data(), bytes.size());
}

// 🔍 고급 메모리 스캔
std::vector<ScanResult> MemoryEngine::ScanForValue(const std::vector<uint8_t>& value, const ScanOptions& options) {
    std::vector<ScanResult> results;
    if (value.empty()) return results;
    
    auto regions = GetMemoryRegions();
    
    for (const auto& region : regions) {
        if (!IsRegionScannable(region, options)) continue;
        
        uintptr_t start = max(region.baseAddress, options.startAddress);
        uintptr_t end = options.endAddress == 0 ? region.baseAddress + region.size : 
                       min(region.baseAddress + region.size, options.endAddress);
        
        if (start >= end) continue;
        
        auto regionData = SafeReadBytes(start, end - start);
        if (regionData.empty()) continue;
        
        for (size_t i = 0; i <= regionData.size() - value.size(); i += options.alignment) {
            if (CompareBytes(regionData.data() + i, value, value.size())) {
                uintptr_t foundAddress = start + i;
                
                // if (options.customFilter && !options.customFilter(foundAddress)) {
                //     continue;
                // }
                
                ScanResult result;
                result.address = foundAddress;
                result.value = value;
                result.type = "bytes";
                results.push_back(result);
            }
        }
    }
    
    return results;
}

std::vector<ScanResult> MemoryEngine::ScanForInt32(int32_t value, const ScanOptions& options) {
    std::vector<uint8_t> bytes(sizeof(int32_t));
    memcpy(bytes.data(), &value, sizeof(int32_t));
    
    auto results = ScanForValue(bytes, options);
    for (auto& result : results) {
        result.type = "int32";
    }
    return results;
}

std::vector<ScanResult> MemoryEngine::ScanForFloat(float value, const ScanOptions& options) {
    std::vector<uint8_t> bytes(sizeof(float));
    memcpy(bytes.data(), &value, sizeof(float));
    
    auto results = ScanForValue(bytes, options);
    for (auto& result : results) {
        result.type = "float";
    }
    return results;
}

std::vector<ScanResult> MemoryEngine::ScanForDouble(double value, const ScanOptions& options) {
    std::vector<uint8_t> bytes(sizeof(double));
    memcpy(bytes.data(), &value, sizeof(double));
    
    auto results = ScanForValue(bytes, options);
    for (auto& result : results) {
        result.type = "double";
    }
    return results;
}

std::vector<ScanResult> MemoryEngine::ScanForString(const std::string& value, bool caseSensitive, const ScanOptions& options) {
    std::vector<ScanResult> results;
    if (value.empty()) return results;
    
    std::vector<uint8_t> searchBytes(value.begin(), value.end());
    std::string lowerValue = value;
    if (!caseSensitive) {
        std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
    }
    
    auto regions = GetMemoryRegions();
    
    for (const auto& region : regions) {
        if (!IsRegionScannable(region, options)) continue;
        
        uintptr_t start = max(region.baseAddress, options.startAddress);
        uintptr_t end = options.endAddress == 0 ? region.baseAddress + region.size : 
                       min(region.baseAddress + region.size, options.endAddress);
        
        auto regionData = SafeReadBytes(start, end - start);
        if (regionData.empty()) continue;
        
        for (size_t i = 0; i <= regionData.size() - value.size(); i += options.alignment) {
            bool match = false;
            
            if (caseSensitive) {
                match = memcmp(regionData.data() + i, searchBytes.data(), value.size()) == 0;
            } else {
                std::string regionStr(reinterpret_cast<char*>(regionData.data() + i), value.size());
                std::transform(regionStr.begin(), regionStr.end(), regionStr.begin(), ::tolower);
                match = regionStr == lowerValue;
            }
            
            if (match) {
                uintptr_t foundAddress = start + i;
                
                // if (options.customFilter && !options.customFilter(foundAddress)) {
                //     continue;
                // }
                
                ScanResult result;
                result.address = foundAddress;
                result.value = std::vector<uint8_t>(regionData.begin() + i, regionData.begin() + i + value.size());
                result.type = "string";
                results.push_back(result);
            }
        }
    }
    
    return results;
}

// 🎯 패턴 스캔 (개선된 버전)
std::vector<uintptr_t> MemoryEngine::PatternScanAll(const std::string& pattern, const std::string& mask, uintptr_t start, uintptr_t end) {
    std::vector<uintptr_t> results;
    
    if (start == 0) start = GetModuleBase();
    if (end == 0) end = start + GetModuleSize();
    
    auto regions = GetMemoryRegions();
    
    for (const auto& region : regions) {
        if (!region.readable) continue;
        
        uintptr_t regionStart = max(region.baseAddress, start);
        uintptr_t regionEnd = min(region.baseAddress + region.size, end);
        
        if (regionStart >= regionEnd) continue;
        
        auto data = SafeReadBytes(regionStart, regionEnd - regionStart);
        if (data.empty()) continue;
        
        for (size_t i = 0; i <= data.size() - pattern.length(); i++) {
            if (ComparePattern(data.data() + i, pattern, mask)) {
                results.push_back(regionStart + i);
            }
        }
    }
    
    return results;
}

std::optional<uintptr_t> MemoryEngine::PatternScanFirst(const std::string& pattern, const std::string& mask, uintptr_t start, uintptr_t end) {
    auto results = PatternScanAll(pattern, mask, start, end);
    return results.empty() ? std::nullopt : std::make_optional(results[0]);
}

std::vector<uintptr_t> MemoryEngine::AOBScanAll(const std::string& pattern, uintptr_t start, uintptr_t end) {
    std::vector<uint8_t> patternBytes = PatternToBytes(pattern);
    std::string mask;
    
    std::istringstream iss(pattern);
    std::string byte;
    while (iss >> byte) {
        mask += (byte == "?" || byte == "??") ? '?' : 'x';
    }
    
    std::string patternStr(patternBytes.begin(), patternBytes.end());
    return PatternScanAll(patternStr, mask, start, end);
}

std::optional<uintptr_t> MemoryEngine::AOBScanFirst(const std::string& pattern, uintptr_t start, uintptr_t end) {
    auto results = AOBScanAll(pattern, start, end);
    return results.empty() ? std::nullopt : std::make_optional(results[0]);
}

// 🗺️ 메모리 영역 관리
std::vector<MemoryRegion> MemoryEngine::GetMemoryRegions() {
    std::vector<MemoryRegion> regions;
    
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t address = 0;
    
    while (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_COMMIT) {
            MemoryRegion region;
            region.baseAddress = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            region.size = mbi.RegionSize;
            region.protection = mbi.Protect;
            region.state = mbi.State;
            region.type = mbi.Type;
            
            // 권한 플래그 설정
            region.readable = (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | 
                                            PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
            region.writable = (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | 
                                            PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
            region.executable = (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | 
                                              PAGE_EXECUTE_WRITECOPY)) != 0;
            
            // 모듈 이름 찾기
            HMODULE hModule;
            if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<LPCTSTR>(address), &hModule)) {
                char moduleName[MAX_PATH];
                if (GetModuleFileNameA(hModule, moduleName, MAX_PATH)) {
                    region.moduleName = moduleName;
                }
            }
            
            regions.push_back(region);
        }
        
        address = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (address == 0) break; // 오버플로우 방지
    }
    
    return regions;
}

std::optional<MemoryRegion> MemoryEngine::GetMemoryRegion(uintptr_t address) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == 0) {
        return std::nullopt;
    }
    
    MemoryRegion region;
    region.baseAddress = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    region.size = mbi.RegionSize;
    region.protection = mbi.Protect;
    region.state = mbi.State;
    region.type = mbi.Type;
    
    region.readable = (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | 
                                    PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
    region.writable = (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | 
                                    PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) != 0;
    region.executable = (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | 
                                      PAGE_EXECUTE_WRITECOPY)) != 0;
    
    return region;
}

std::vector<MemoryRegion> MemoryEngine::GetExecutableRegions() {
    auto regions = GetMemoryRegions();
    std::vector<MemoryRegion> executableRegions;
    
    std::copy_if(regions.begin(), regions.end(), std::back_inserter(executableRegions),
                [](const MemoryRegion& region) { return region.executable; });
    
    return executableRegions;
}

std::vector<MemoryRegion> MemoryEngine::GetWritableRegions() {
    auto regions = GetMemoryRegions();
    std::vector<MemoryRegion> writableRegions;
    
    std::copy_if(regions.begin(), regions.end(), std::back_inserter(writableRegions),
                [](const MemoryRegion& region) { return region.writable; });
    
    return writableRegions;
}

// 🔧 메모리 보호 및 할당
bool MemoryEngine::ChangeProtection(uintptr_t address, size_t size, DWORD newProtect, DWORD* oldProtect) {
    DWORD temp;
    return VirtualProtect(reinterpret_cast<void*>(address), size, newProtect, oldProtect ? oldProtect : &temp);
}

uintptr_t MemoryEngine::AllocateMemory(size_t size, DWORD protection) {
    return reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, protection));
}

bool MemoryEngine::FreeMemory(uintptr_t address) {
    return VirtualFree(reinterpret_cast<void*>(address), 0, MEM_RELEASE);
}

// 📦 모듈 관리
uintptr_t MemoryEngine::GetModuleBase(const std::string& moduleName) {
    if (moduleName.empty()) {
        return reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
    }
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName.c_str()));
}

size_t MemoryEngine::GetModuleSize(const std::string& moduleName) {
    HMODULE hModule = moduleName.empty() ? GetModuleHandle(nullptr) : GetModuleHandleA(moduleName.c_str());
    if (!hModule) return 0;
    
    MODULEINFO modInfo;
    if (GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) {
        return modInfo.SizeOfImage;
    }
    return 0;
}

std::vector<std::string> MemoryEngine::GetLoadedModules() {
    std::vector<std::string> modules;
    
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (hSnap == INVALID_HANDLE_VALUE) return modules;
    
    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);
    
    if (Module32First(hSnap, &me32)) {
        do {
            modules.push_back(me32.szModule);
        } while (Module32Next(hSnap, &me32));
    }
    
    CloseHandle(hSnap);
    return modules;
}

std::optional<MemoryRegion> MemoryEngine::GetModuleRegion(const std::string& moduleName) {
    uintptr_t base = GetModuleBase(moduleName);
    if (base == 0) return std::nullopt;
    
    return GetMemoryRegion(base);
}

// 🛠️ 유틸리티 함수
std::string MemoryEngine::BytesToHexString(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    for (uint8_t byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte) << " ";
    }
    std::string result = ss.str();
    if (!result.empty()) result.pop_back(); // 마지막 공백 제거
    return result;
}

std::vector<uint8_t> MemoryEngine::HexStringToBytes(const std::string& hexString) {
    std::vector<uint8_t> bytes;
    std::istringstream iss(hexString);
    std::string byte;
    
    while (iss >> byte) {
        if (byte.length() == 2) {
            bytes.push_back(static_cast<uint8_t>(std::stoul(byte, nullptr, 16)));
        }
    }
    
    return bytes;
}

std::vector<uint8_t> MemoryEngine::PatternToBytes(const std::string& pattern) {
    std::vector<uint8_t> bytes;
    std::istringstream iss(pattern);
    std::string byte;
    
    while (iss >> byte) {
        if (byte == "?" || byte == "??") {
            bytes.push_back(0);
        } else {
            bytes.push_back(static_cast<uint8_t>(std::stoul(byte, nullptr, 16)));
        }
    }
    return bytes;
}

std::string MemoryEngine::BytesToPattern(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); i++) {
        if (i > 0) ss << " ";
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

// 🔄 값 변환 함수
std::string MemoryEngine::ValueToString(const std::vector<uint8_t>& bytes, const std::string& type) {
    if (bytes.empty()) return "";
    
    if (type == "int32" && bytes.size() >= 4) {
        int32_t value;
        memcpy(&value, bytes.data(), sizeof(int32_t));
        return std::to_string(value);
    } else if (type == "float" && bytes.size() >= 4) {
        float value;
        memcpy(&value, bytes.data(), sizeof(float));
        return std::to_string(value);
    } else if (type == "double" && bytes.size() >= 8) {
        double value;
        memcpy(&value, bytes.data(), sizeof(double));
        return std::to_string(value);
    } else if (type == "string") {
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    } else {
        return BytesToHexString(bytes);
    }
}

std::vector<uint8_t> MemoryEngine::StringToValue(const std::string& value, const std::string& type) {
    std::vector<uint8_t> bytes;
    
    if (type == "int32") {
        int32_t intValue = std::stoi(value);
        bytes.resize(sizeof(int32_t));
        memcpy(bytes.data(), &intValue, sizeof(int32_t));
    } else if (type == "float") {
        float floatValue = std::stof(value);
        bytes.resize(sizeof(float));
        memcpy(bytes.data(), &floatValue, sizeof(float));
    } else if (type == "double") {
        double doubleValue = std::stod(value);
        bytes.resize(sizeof(double));
        memcpy(bytes.data(), &doubleValue, sizeof(double));
    } else if (type == "string") {
        bytes.assign(value.begin(), value.end());
    } else {
        bytes = HexStringToBytes(value);
    }
    
    return bytes;
}

// 🎮 게임 해킹 전용 함수
bool MemoryEngine::NopInstruction(uintptr_t address, size_t size) {
    std::vector<uint8_t> nops(size, 0x90);
    return SafeWriteBytes(address, nops);
}

bool MemoryEngine::PatchBytes(uintptr_t address, const std::vector<uint8_t>& originalBytes, const std::vector<uint8_t>& newBytes) {
    auto currentBytes = SafeReadBytes(address, originalBytes.size());
    if (currentBytes != originalBytes) {
        return false; // 원본 바이트가 일치하지 않음
    }
    
    return SafeWriteBytes(address, newBytes);
}

bool MemoryEngine::RestoreBytes(uintptr_t address, const std::vector<uint8_t>& originalBytes) {
    return SafeWriteBytes(address, originalBytes);
}

// 🧠 스마트 포인터 체이닝
std::optional<uintptr_t> MemoryEngine::FollowPointerChain(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets) {
    uintptr_t currentAddress = baseAddress;
    
    for (size_t i = 0; i < offsets.size(); i++) {
        auto ptrValue = SafeRead<uintptr_t>(currentAddress);
        if (!ptrValue.has_value()) {
            return std::nullopt;
        }
        
        currentAddress = ptrValue.value() + offsets[i];
        
        // 마지막 오프셋이 아닌 경우 포인터 역참조 필요
        if (i < offsets.size() - 1) {
            if (!IsAddressReadable(currentAddress, sizeof(uintptr_t))) {
                return std::nullopt;
            }
        }
    }
    
    return currentAddress;
}

std::vector<uintptr_t> MemoryEngine::FindPointersTo(uintptr_t targetAddress, const ScanOptions& options) {
    std::vector<uint8_t> targetBytes(sizeof(uintptr_t));
    memcpy(targetBytes.data(), &targetAddress, sizeof(uintptr_t));
    
    auto results = ScanForValue(targetBytes, options);
    std::vector<uintptr_t> pointers;
    
    for (const auto& result : results) {
        pointers.push_back(result.address);
    }
    
    return pointers;
}

// Private 헬퍼 함수들
bool MemoryEngine::ComparePattern(const uint8_t* data, const std::string& pattern, const std::string& mask) {
    for (size_t i = 0; i < mask.length(); i++) {
        if (mask[i] == 'x' && data[i] != static_cast<uint8_t>(pattern[i])) {
            return false;
        }
    }
    return true;
}

bool MemoryEngine::CompareBytes(const uint8_t* data, const std::vector<uint8_t>& pattern, size_t size) {
    return memcmp(data, pattern.data(), size) == 0;
}

DWORD MemoryEngine::GetProtectionFlags(uintptr_t address) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == 0) {
        return 0;
    }
    return mbi.Protect;
}

// Scan filter function
bool MemoryEngine::IsRegionScannable(const MemoryRegion& region, const ScanOptions& options) {
    if (!region.readable) return false;

    auto checkState = [](TriState state, bool condition) {
        if (state == TriState::Yes && !condition) return false;
        if (state == TriState::No && condition) return false;
        return true;
    };

    if (!checkState(options.filterWritable, region.writable)) return false;
    if (!checkState(options.filterExecutable, region.executable)) return false;
    
    // NOTE: CopyOnWrite check may require more specific logic based on MEMORY_BASIC_INFORMATION 'Protect' flags
    // For now, we assume if it's writable, it could be copy-on-write. This is a simplification.
    if (options.filterCopyOnWrite == TriState::Yes && !(region.protection & PAGE_WRITECOPY || region.protection & PAGE_EXECUTE_WRITECOPY)) {
        return false;
    }
    if (options.filterCopyOnWrite == TriState::No && (region.protection & PAGE_WRITECOPY || region.protection & PAGE_EXECUTE_WRITECOPY)) {
        return false;
    }
    
    return true;
}

// First Scan Implementation
std::vector<ScanResult> MemoryEngine::FirstScan(const std::string& value, const std::string& type, const ScanOptions& options) {
    std::vector<ScanResult> results;
    std::vector<uint8_t> valueBytes = StringToValue(value, type);
    if (valueBytes.empty()) return results;

    auto regions = GetMemoryRegions();
    
    for (const auto& region : regions) {
        if (!IsRegionScannable(region, options)) continue;
        
        uintptr_t start = max(region.baseAddress, options.startAddress);
        uintptr_t end = (options.endAddress == 0) ? (region.baseAddress + region.size) : min(region.baseAddress + region.size, options.endAddress);
        
        if (start >= end) continue;

        auto regionData = SafeReadBytes(start, end - start);
        if (regionData.empty()) continue;
        
        for (size_t i = 0; i <= regionData.size() - valueBytes.size(); i += options.alignment) {
            if (memcmp(regionData.data() + i, valueBytes.data(), valueBytes.size()) == 0) {
                ScanResult result;
                result.address = start + i;
                result.value = valueBytes;
                result.type = type;
                results.push_back(result);
            }
        }
    }
    return results;
}

// 레거시 함수들 (하위 호환성)
std::vector<uint8_t> MemoryEngine::ReadBytes(uintptr_t address, size_t size) {
    return SafeReadBytes(address, size);
}

bool MemoryEngine::WriteBytes(uintptr_t address, const std::vector<uint8_t>& bytes) {
    return SafeWriteBytes(address, bytes);
}

std::optional<uintptr_t> MemoryEngine::PatternScan(const std::string& pattern, const std::string& mask, uintptr_t start, uintptr_t end) {
    return PatternScanFirst(pattern, mask, start, end);
}

std::optional<uintptr_t> MemoryEngine::AOBScan(const std::string& pattern, uintptr_t start, uintptr_t end) {
    return AOBScanFirst(pattern, start, end);
}

bool MemoryEngine::Nop(uintptr_t address, size_t size) {
    return NopInstruction(address, size);
}

// Type-aware value comparison helper
template<typename T>
bool CompareNumericValues(const T& current, const T& previous, const std::string& scanType) {
    if (scanType == "increased") return current > previous;
    if (scanType == "decreased") return current < previous;
    // Potentially other comparisons like "increased by", "decreased by" could be added here
    return false;
}

// Main Next Scan Implementation
std::vector<ScanResult> MemoryEngine::NextScan(const std::string& scanType, const std::string& value, const ScanOptions& options) {
    std::vector<ScanResult> newResults;
    if (!options.previousResults || options.previousResults->empty()) {
        return newResults;
    }

    for (const auto& prevResult : *options.previousResults) {
        auto currentValueBytes = SafeReadBytes(prevResult.address, prevResult.value.size());
        if (currentValueBytes.empty() || currentValueBytes.size() != prevResult.value.size()) {
            continue; // Skip addresses that can no longer be read
        }

        bool match = false;

        if (scanType == "exact") {
            std::vector<uint8_t> valueToCompare = StringToValue(value, prevResult.type);
            if (!valueToCompare.empty() && memcmp(currentValueBytes.data(), valueToCompare.data(), currentValueBytes.size()) == 0) {
                match = true;
            }
        } else if (scanType == "unchanged") {
            if (memcmp(currentValueBytes.data(), prevResult.value.data(), currentValueBytes.size()) == 0) {
                match = true;
            }
        } else if (scanType == "changed") {
            if (memcmp(currentValueBytes.data(), prevResult.value.data(), currentValueBytes.size()) != 0) {
                match = true;
            }
        } else { // Handle numeric comparisons
            if (prevResult.type == "int32") {
                int32_t currentVal, prevVal;
                memcpy(&currentVal, currentValueBytes.data(), sizeof(int32_t));
                memcpy(&prevVal, prevResult.value.data(), sizeof(int32_t));
                match = CompareNumericValues(currentVal, prevVal, scanType);
            } else if (prevResult.type == "int64") {
                int64_t currentVal, prevVal;
                memcpy(&currentVal, currentValueBytes.data(), sizeof(int64_t));
                memcpy(&prevVal, prevResult.value.data(), sizeof(int64_t));
                match = CompareNumericValues(currentVal, prevVal, scanType);
            } else if (prevResult.type == "float") {
                float currentVal, prevVal;
                memcpy(&currentVal, currentValueBytes.data(), sizeof(float));
                memcpy(&prevVal, prevResult.value.data(), sizeof(float));
                match = CompareNumericValues(currentVal, prevVal, scanType);
            } else if (prevResult.type == "double") {
                double currentVal, prevVal;
                memcpy(&currentVal, currentValueBytes.data(), sizeof(double));
                memcpy(&prevVal, prevResult.value.data(), sizeof(double));
                match = CompareNumericValues(currentVal, prevVal, scanType);
            }
        }

        if (match) {
            ScanResult newResult;
            newResult.address = prevResult.address;
            newResult.value = currentValueBytes;
            newResult.previousValue = prevResult.value;
            newResult.type = prevResult.type;
            newResults.push_back(newResult);
        }
    }
    return newResults;
}

// Enhanced reading functions for disassembler
std::optional<std::vector<uint8_t>> MemoryEngine::ReadValueAtAddress(uintptr_t address, const std::string& type) {
    try {
        if (type == "int32" || type == "int") {
            auto value = SafeRead<int32_t>(address);
            if (!value.has_value()) return std::nullopt;
            
            std::vector<uint8_t> bytes(sizeof(int32_t));
            memcpy(bytes.data(), &value.value(), sizeof(int32_t));
            return bytes;
        }
        else if (type == "int64") {
            auto value = SafeRead<int64_t>(address);
            if (!value.has_value()) return std::nullopt;
            
            std::vector<uint8_t> bytes(sizeof(int64_t));
            memcpy(bytes.data(), &value.value(), sizeof(int64_t));
            return bytes;
        }
        else if (type == "float") {
            auto value = SafeRead<float>(address);
            if (!value.has_value()) return std::nullopt;
            
            std::vector<uint8_t> bytes(sizeof(float));
            memcpy(bytes.data(), &value.value(), sizeof(float));
            return bytes;
        }
        else if (type == "double") {
            auto value = SafeRead<double>(address);
            if (!value.has_value()) return std::nullopt;
            
            std::vector<uint8_t> bytes(sizeof(double));
            memcpy(bytes.data(), &value.value(), sizeof(double));
            return bytes;
        }
        else if (type == "byte") {
            auto value = SafeRead<uint8_t>(address);
            if (!value.has_value()) return std::nullopt;
            
            return std::vector<uint8_t>{value.value()};
        }
        else if (type == "string") {
            auto bytes = SafeReadBytes(address, 256);
            if (bytes.empty()) return std::nullopt;
            
            // Find null terminator
            size_t nullPos = 0;
            for (size_t i = 0; i < bytes.size(); i++) {
                if (bytes[i] == 0) {
                    nullPos = i;
                    break;
                }
                nullPos = i + 1;
            }
            
            return std::vector<uint8_t>(bytes.begin(), bytes.begin() + nullPos);
        }
        else if (type == "bytes") {
            return SafeReadBytes(address, 16);
        }
        
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

int MemoryEngine::CompareValues(const std::vector<uint8_t>& value1, const std::vector<uint8_t>& value2, const std::string& type) {
    if (value1.empty() || value2.empty()) return 0;
    
    try {
        if (type == "int32" || type == "int") {
            if (value1.size() < sizeof(int32_t) || value2.size() < sizeof(int32_t)) return 0;
            
            int32_t val1, val2;
            memcpy(&val1, value1.data(), sizeof(int32_t));
            memcpy(&val2, value2.data(), sizeof(int32_t));
            
            if (val1 > val2) return 1;
            if (val1 < val2) return -1;
            return 0;
        }
        else if (type == "int64") {
            if (value1.size() < sizeof(int64_t) || value2.size() < sizeof(int64_t)) return 0;
            
            int64_t val1, val2;
            memcpy(&val1, value1.data(), sizeof(int64_t));
            memcpy(&val2, value2.data(), sizeof(int64_t));
            
            if (val1 > val2) return 1;
            if (val1 < val2) return -1;
            return 0;
        }
        else if (type == "float") {
            if (value1.size() < sizeof(float) || value2.size() < sizeof(float)) return 0;
            
            float val1, val2;
            memcpy(&val1, value1.data(), sizeof(float));
            memcpy(&val2, value2.data(), sizeof(float));
            
            if (val1 > val2) return 1;
            if (val1 < val2) return -1;
            return 0;
        }
        else if (type == "double") {
            if (value1.size() < sizeof(double) || value2.size() < sizeof(double)) return 0;
            
            double val1, val2;
            memcpy(&val1, value1.data(), sizeof(double));
            memcpy(&val2, value2.data(), sizeof(double));
            
            if (val1 > val2) return 1;
            if (val1 < val2) return -1;
            return 0;
        }
        else if (type == "byte") {
            uint8_t val1 = value1[0];
            uint8_t val2 = value2[0];
            
            if (val1 > val2) return 1;
            if (val1 < val2) return -1;
            return 0;
        }
        else {
            // For strings and bytes, do lexicographical comparison
            if (value1.size() != value2.size()) {
                return (value1.size() > value2.size()) ? 1 : -1;
            }
            
            int result = memcmp(value1.data(), value2.data(), value1.size());
            if (result > 0) return 1;
            if (result < 0) return -1;
            return 0;
        }
    } catch (...) {
        return 0;
    }
}

} // namespace InternalEngine 