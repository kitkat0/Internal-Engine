#include "CommandRouter.hpp"
#include "MemoryEngine.hpp"
#include "HookManager.hpp"
#include <sstream>
#include <iomanip>
#include <Psapi.h>
#include <TlHelp32.h>
#include <algorithm>
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

using namespace std;

namespace InternalEngine {

// Global instance
CommandRouter* g_CommandRouter = nullptr;

// Simple JSON parser helpers (minimal implementation)
std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
    if (valueStart == std::string::npos) return "";
    
    if (json[valueStart] == '"') {
        size_t valueEnd = json.find("\"", valueStart + 1);
        if (valueEnd == std::string::npos) return "";
        return json.substr(valueStart + 1, valueEnd - valueStart - 1);
    } else {
        size_t valueEnd = json.find_first_of(",}", valueStart);
        if (valueEnd == std::string::npos) valueEnd = json.length();
        return json.substr(valueStart, valueEnd - valueStart);
    }
}

// Helper function to escape JSON strings
std::string EscapeJsonString(const std::string& input) {
    std::string output;
    output.reserve(input.length() * 2);
    
    for (char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20) {
                    output += "\\u";
                    output += "0123456789abcdef"[(c >> 12) & 0xF];
                    output += "0123456789abcdef"[(c >> 8) & 0xF];
                    output += "0123456789abcdef"[(c >> 4) & 0xF];
                    output += "0123456789abcdef"[c & 0xF];
                } else {
                    output += c;
                }
                break;
        }
    }
    
    return output;
}

CommandRouter::CommandRouter() {
    RegisterBuiltinCommands();
}

CommandRouter::~CommandRouter() {
}

void CommandRouter::RegisterCommand(const std::string& command, CommandHandler handler) {
    commands[command] = handler;
}

void CommandRouter::UnregisterCommand(const std::string& command) {
    commands.erase(command);
}

std::string CommandRouter::ExecuteCommand(const std::string& jsonRequest) {
    try {
        std::string command = ExtractJsonValue(jsonRequest, "command");
        std::string id = ExtractJsonValue(jsonRequest, "id");
        
        if (command.empty()) {
            return CreateResponse(false, "", "No command specified", id);
        }
        
        auto it = commands.find(command);
        if (it == commands.end()) {
            return CreateResponse(false, "", "Unknown command: " + command, id);
        }
        
        // Execute the command and get the response
        std::string response = it->second(jsonRequest);
        
        // Add the ID to the response if it's not already there
        if (!id.empty() && response.find("\"id\"") == std::string::npos) {
            // Insert the ID at the beginning of the JSON object
            size_t openBrace = response.find('{');
            if (openBrace != std::string::npos) {
                response.insert(openBrace + 1, "\"id\":\"" + id + "\",");
            }
        }
        
        return response;
    } catch (const std::exception& e) {
        std::string id = ExtractJsonValue(jsonRequest, "id");
        return CreateResponse(false, "", std::string("Exception: ") + e.what(), id);
    }
}

void CommandRouter::RegisterBuiltinCommands() {
    RegisterCommand("memory.read", [this](const std::string& p) { return HandleMemoryRead(p); });
    RegisterCommand("memory.write", [this](const std::string& p) { return HandleMemoryWrite(p); });
    RegisterCommand("memory.scan", [this](const std::string& p) { return HandleMemoryScan(p); });
    RegisterCommand("memory.regions", [this](const std::string& p) { return HandleMemoryRegions(p); });
    RegisterCommand("memory.validate", [this](const std::string& p) { return HandleMemoryValidate(p); });
    RegisterCommand("pattern.scan", [this](const std::string& p) { return HandlePatternScan(p); });
    RegisterCommand("pattern.scanall", [this](const std::string& p) { return HandlePatternScanAll(p); });
    RegisterCommand("module.list", [this](const std::string& p) { return HandleModuleList(p); });
    RegisterCommand("module.info", [this](const std::string& p) { return HandleModuleInfo(p); });
    RegisterCommand("process.info", [this](const std::string& p) { return HandleProcessInfo(p); });
    RegisterCommand("hook.install", [this](const std::string& p) { return HandleHookInstall(p); });
    RegisterCommand("hook.remove", [this](const std::string& p) { return HandleHookRemove(p); });
    RegisterCommand("hook.list", [this](const std::string& p) { return HandleHookList(p); });
    RegisterCommand("hook.toggle", [this](const std::string& p) { return HandleHookToggle(p); });
    RegisterCommand("memory.allocate", [this](const std::string& p) { return HandleAllocateMemory(p); });
    RegisterCommand("memory.free", [this](const std::string& p) { return HandleFreeMemory(p); });
    RegisterCommand("memory.patch", [this](const std::string& p) { return HandleMemoryPatch(p); });
    RegisterCommand("memory.nop", [this](const std::string& p) { return HandleMemoryNop(p); });
    RegisterCommand("pointer.chain", [this](const std::string& p) { return HandlePointerChain(p); });
    RegisterCommand("pointer.find", [this](const std::string& p) { return HandlePointerFind(p); });
    
    // New enhanced commands
    RegisterCommand("memory.read_value", [this](const std::string& p) { return HandleMemoryReadValue(p); });
    RegisterCommand("memory.disassemble", [this](const std::string& p) { return HandleMemoryDisassemble(p); });
    RegisterCommand("module.from_address", [this](const std::string& p) { return HandleModuleFromAddress(p); });
}

std::string CommandRouter::CreateResponse(bool success, const std::string& data, const std::string& error, const std::string& id) {
    std::stringstream ss;
    ss << "{";
    
    // Add ID first if provided
    if (!id.empty()) {
        ss << "\"id\":\"" << id << "\",";
    }
    
    ss << "\"success\":" << (success ? "true" : "false");
    if (!data.empty()) {
        ss << ",\"data\":" << data;
    }
    if (!error.empty()) {
        ss << ",\"error\":\"" << error << "\"";
    }
    ss << "}";
    return ss.str();
}

std::string CommandRouter::HandleMemoryRead(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string sizeStr = ExtractJsonValue(params, "size");
    std::string typeStr = ExtractJsonValue(params, "type");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty() || sizeStr.empty()) {
        return CreateResponse(false, "", "Missing address or size parameter", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        size_t size = std::stoull(sizeStr);
        
        // 주소 유효성 검사
        if (!MemoryEngine::IsAddressValid(address, size)) {
            return CreateResponse(false, "", "Invalid memory address or size", id);
        }
        
        if (!MemoryEngine::IsAddressReadable(address, size)) {
            return CreateResponse(false, "", "Memory address is not readable", id);
        }
        
        if (typeStr.empty() || typeStr == "bytes") {
            // 안전한 바이트 읽기
            auto bytes = MemoryEngine::SafeReadBytes(address, size);
            if (bytes.empty()) {
                return CreateResponse(false, "", "Failed to read memory - access denied or invalid address", id);
            }
            
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < bytes.size(); i++) {
                if (i > 0) ss << ",";
                ss << static_cast<int>(bytes[i]);
            }
            ss << "]";
            return CreateResponse(true, ss.str(), "", id);
        } else if (typeStr == "int") {
            auto value = MemoryEngine::SafeRead<int32_t>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read integer value", id);
            }
            return CreateResponse(true, std::to_string(value.value()), "", id);
        } else if (typeStr == "float") {
            auto value = MemoryEngine::SafeRead<float>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read float value", id);
            }
            return CreateResponse(true, std::to_string(value.value()), "", id);
        } else if (typeStr == "double") {
            auto value = MemoryEngine::SafeRead<double>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read double value", id);
            }
            return CreateResponse(true, std::to_string(value.value()), "", id);
        } else if (typeStr == "string") {
            auto bytes = MemoryEngine::SafeReadBytes(address, min(size, static_cast<size_t>(256)));
            if (bytes.empty()) {
                return CreateResponse(false, "", "Failed to read string", id);
            }
            
            // 널 종료 문자열 찾기
            size_t nullPos = 0;
            for (size_t i = 0; i < bytes.size(); i++) {
                if (bytes[i] == 0) {
                    nullPos = i;
                    break;
                }
                nullPos = i + 1;
            }
            
            std::string result(reinterpret_cast<char*>(bytes.data()), nullPos);
            return CreateResponse(true, "\"" + EscapeJsonString(result) + "\"", "", id);
        }
        
        return CreateResponse(false, "", "Unknown type: " + typeStr, id);
    } catch (const std::exception& e) {
        return CreateResponse(false, "", std::string("Exception: ") + e.what(), id);
    } catch (...) {
        return CreateResponse(false, "", "Unknown error occurred while reading memory", id);
    }
}

std::string CommandRouter::HandleMemoryWrite(const std::string& params) {
    std::string id = ExtractJsonValue(params, "id");
    try {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string valueStr = ExtractJsonValue(params, "value");
    std::string typeStr = ExtractJsonValue(params, "type");
    
        if (addressStr.empty() || valueStr.empty() || typeStr.empty()) {
            return CreateResponse(false, "", "Missing address, value, or type parameter", id);
        }

        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        std::vector<uint8_t> bytesToWrite = InternalEngine::MemoryEngine::StringToValue(valueStr, typeStr);

        if (bytesToWrite.empty()) {
            return CreateResponse(false, "", "Invalid value or type for writing", id);
        }

        if (MemoryEngine::SafeWriteBytes(address, bytesToWrite)) {
            return CreateResponse(true, "{}", "Memory written successfully", id);
        } else {
            return CreateResponse(false, "", "Failed to write to memory. Address may not be writable.", id);
        }
    } catch (const std::exception& e) {
        return CreateResponse(false, "", std::string("Write error: ") + e.what(), id);
    } catch (...) {
        return CreateResponse(false, "", "Unknown error during memory write", id);
    }
}

// Cached module information
struct ModuleRange {
    uintptr_t base;
    uintptr_t end;
    std::string name;
};

static std::vector<ModuleRange> g_moduleCache;
static DWORD g_lastModuleCacheUpdate = 0;
static const DWORD MODULE_CACHE_VALIDITY_MS = 5000; // 5 seconds

// Update module cache
void UpdateModuleCache() {
    DWORD currentTime = GetTickCount();
    if (currentTime - g_lastModuleCacheUpdate < MODULE_CACHE_VALIDITY_MS && !g_moduleCache.empty()) {
        return; // Cache is still valid
    }

    g_moduleCache.clear();
    HANDLE hProcess = GetCurrentProcess();
    HMODULE hModules[1024];
    DWORD cbNeeded;

    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
        for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            MODULEINFO modInfo;
            if (GetModuleInformation(hProcess, hModules[i], &modInfo, sizeof(modInfo))) {
                char moduleName[MAX_PATH];
                if (GetModuleFileNameExA(hProcess, hModules[i], moduleName, MAX_PATH)) {
                    ModuleRange range;
                    range.base = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
                    range.end = range.base + modInfo.SizeOfImage;
                    range.name = PathFindFileNameA(moduleName);
                    g_moduleCache.push_back(range);
                }
            }
        }
    }
    
    g_lastModuleCacheUpdate = currentTime;
}

// Fast module lookup using cache
std::string GetModuleInfoForAddress(uintptr_t address) {
    UpdateModuleCache();
    
    for (const auto& module : g_moduleCache) {
        if (address >= module.base && address < module.end) {
            uintptr_t offset = address - module.base;
            std::stringstream ss;
            ss << module.name << "+0x" << std::hex << offset;
            return ss.str();
        }
    }
    
    return ""; // Not found in any module
}

std::string CommandRouter::HandleMemoryScan(const std::string& params) {
    std::string id = ExtractJsonValue(params, "id");
    try {
        std::string valueStr = ExtractJsonValue(params, "value");
        std::string typeStr = ExtractJsonValue(params, "valueType");
        std::string scanTypeStr = ExtractJsonValue(params, "scanType");
        
        bool isFirstScan = (ExtractJsonValue(params, "firstScan") == "true");

        ScanOptions options;
        options.isFirstScan = isFirstScan;

        std::string startStr = ExtractJsonValue(params, "startAddress");
        std::string endStr = ExtractJsonValue(params, "endAddress");
        if (!startStr.empty()) options.startAddress = std::stoull(startStr, nullptr, 16);
        if (!endStr.empty()) options.endAddress = std::stoull(endStr, nullptr, 16);

        auto parseTriState = [](const std::string& s) {
            if (s == "yes") return TriState::Yes;
            if (s == "no") return TriState::No;
            return TriState::Any;
        };
        options.filterWritable = parseTriState(ExtractJsonValue(params, "writable"));
        options.filterExecutable = parseTriState(ExtractJsonValue(params, "executable"));
        options.filterCopyOnWrite = parseTriState(ExtractJsonValue(params, "copyOnWrite"));
        
        std::vector<ScanResult> results;
        std::vector<ScanResult> previousResults;

        if (!isFirstScan) {
            // For Next Scan, we need to read current values of previous scan results
            // and filter them based on scan criteria
            std::string prevResultsStr = ExtractJsonValue(params, "previousResults");
            
            // Parse previous results from JSON array
            prevResultsStr.erase(0, prevResultsStr.find_first_not_of(" \t\r\n"));
            if (!prevResultsStr.empty() && prevResultsStr.front() == '[') prevResultsStr.erase(0, 1);
            if (!prevResultsStr.empty() && prevResultsStr.back() == ']') prevResultsStr.pop_back();

            size_t pos = 0;
            while ((pos = prevResultsStr.find('{', pos)) != std::string::npos) {
                size_t endPos = prevResultsStr.find('}', pos);
                if (endPos == std::string::npos) break;

                std::string objStr = prevResultsStr.substr(pos, endPos - pos + 1);
                
                std::string addrStr = ExtractJsonValue(objStr, "address");
                std::string valStr = ExtractJsonValue(objStr, "value");
                
                if (!addrStr.empty()) {
                    try {
                        uintptr_t address = std::stoull(addrStr, nullptr, 16);
                        
                        // Read current value at this address
                        auto currentValueOpt = MemoryEngine::ReadValueAtAddress(address, typeStr);
                        if (currentValueOpt.has_value()) {
                            auto currentValue = currentValueOpt.value();
                            auto previousValue = MemoryEngine::StringToValue(valStr, typeStr);
                            
                            bool include = false;
                            
                            // Apply scan filter
                            if (scanTypeStr == "exact") {
                                auto targetValue = MemoryEngine::StringToValue(valueStr, typeStr);
                                include = (currentValue == targetValue);
                            }
                            else if (scanTypeStr == "changed") {
                                include = (currentValue != previousValue);
                            }
                            else if (scanTypeStr == "unchanged") {
                                include = (currentValue == previousValue);
                            }
                            else if (scanTypeStr == "increased") {
                                include = MemoryEngine::CompareValues(currentValue, previousValue, typeStr) > 0;
                            }
                            else if (scanTypeStr == "decreased") {
                                include = MemoryEngine::CompareValues(currentValue, previousValue, typeStr) < 0;
                            }
                            else {
                                include = true; // Unknown scan type, include all
                            }
                            
                            if (include) {
                                ScanResult result;
                                result.address = address;
                                result.value = currentValue;
                                result.previousValue = previousValue;
                                result.type = typeStr;
                                results.push_back(result);
                            }
                        }
                    } catch (...) {
                        // Skip invalid addresses
                    }
                }
                pos = endPos + 1;
            }
        } else {
            if (valueStr.empty() || typeStr.empty()) {
                return CreateResponse(false, "", "Missing value or type for first scan", id);
            }
            results = MemoryEngine::FirstScan(valueStr, typeStr, options);
        }

        std::stringstream ss;
        ss << "[";
        size_t maxResults = results.size(); // No limit - stream all results
        
        for (size_t i = 0; i < maxResults; i++) {
            if (i > 0) ss << ",";
            ss << "{";
            ss << "\"address\":\"0x" << std::hex << results[i].address << "\",";
            ss << "\"value\":\"" << MemoryEngine::ValueToString(results[i].value, results[i].type) << "\"";
            if (!isFirstScan && !results[i].previousValue.empty()) {
                ss << ",\"previousValue\":\"" << MemoryEngine::ValueToString(results[i].previousValue, results[i].type) << "\"";
            }
            
            // Add module information
            std::string moduleInfo = GetModuleInfoForAddress(results[i].address);
            if (!moduleInfo.empty()) {
                ss << ",\"module\":\"" << EscapeJsonString(moduleInfo) << "\"";
            }
            
            ss << "}";
        }
        
        ss << "]";
        
        std::string message = "Found " + std::to_string(results.size()) + " results (all displayed)";
        
        return CreateResponse(true, ss.str(), message, id);
    } catch (const std::exception& e) {
        return CreateResponse(false, "", std::string("Scan error: ") + e.what(), id);
    } catch (...) {
        return CreateResponse(false, "", "Unknown error during scan", id);
    }
}

std::string CommandRouter::HandleMemoryRegions(const std::string& params) {
    std::string id = ExtractJsonValue(params, "id");
    std::string filterStr = ExtractJsonValue(params, "filter"); // "readable", "writable", "executable"
    
    try {
        auto regions = MemoryEngine::GetMemoryRegions();
        
        // 필터 적용
        if (!filterStr.empty()) {
            auto filteredRegions = regions;
            regions.clear();
            
            for (const auto& region : filteredRegions) {
                if (filterStr == "readable" && region.readable) {
                    regions.push_back(region);
                } else if (filterStr == "writable" && region.writable) {
                    regions.push_back(region);
                } else if (filterStr == "executable" && region.executable) {
                    regions.push_back(region);
                }
            }
        }
        
        std::stringstream ss;
        ss << "[";
        
        for (size_t i = 0; i < regions.size(); i++) {
            if (i > 0) ss << ",";
            
            ss << "{";
            ss << "\"baseAddress\":\"0x" << std::hex << regions[i].baseAddress << "\",";
            ss << "\"size\":" << std::dec << regions[i].size << ",";
            ss << "\"protection\":" << regions[i].protection << ",";
            ss << "\"readable\":" << (regions[i].readable ? "true" : "false") << ",";
            ss << "\"writable\":" << (regions[i].writable ? "true" : "false") << ",";
            ss << "\"executable\":" << (regions[i].executable ? "true" : "false") << ",";
            ss << "\"moduleName\":\"" << EscapeJsonString(regions[i].moduleName) << "\"";
            ss << "}";
        }
        
        ss << "]";
        
        return CreateResponse(true, ss.str(), "", id);
    } catch (...) {
        return CreateResponse(false, "", "Failed to get memory regions", id);
    }
}

std::string CommandRouter::HandleMemoryValidate(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string sizeStr = ExtractJsonValue(params, "size");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty()) {
        return CreateResponse(false, "", "Missing address parameter", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        size_t size = sizeStr.empty() ? 1 : std::stoull(sizeStr);
        
        bool isValid = MemoryEngine::IsAddressValid(address, size);
        bool isReadable = MemoryEngine::IsAddressReadable(address, size);
        bool isWritable = MemoryEngine::IsAddressWritable(address, size);
        
        auto region = MemoryEngine::GetMemoryRegion(address);
        
        std::stringstream ss;
        ss << "{";
        ss << "\"valid\":" << (isValid ? "true" : "false") << ",";
        ss << "\"readable\":" << (isReadable ? "true" : "false") << ",";
        ss << "\"writable\":" << (isWritable ? "true" : "false");
        
        if (region.has_value()) {
            ss << ",\"region\":{";
            ss << "\"baseAddress\":\"0x" << std::hex << region->baseAddress << "\",";
            ss << "\"size\":" << std::dec << region->size << ",";
            ss << "\"protection\":" << region->protection << ",";
            ss << "\"moduleName\":\"" << EscapeJsonString(region->moduleName) << "\"";
            ss << "}";
        }
        
        ss << "}";
        
        return CreateResponse(true, ss.str(), "", id);
    } catch (...) {
        return CreateResponse(false, "", "Failed to validate address", id);
    }
}

std::string CommandRouter::HandlePatternScanAll(const std::string& params) {
    std::string pattern = ExtractJsonValue(params, "pattern");
    std::string startStr = ExtractJsonValue(params, "start");
    std::string endStr = ExtractJsonValue(params, "end");
    std::string id = ExtractJsonValue(params, "id");
    
    if (pattern.empty()) {
        return CreateResponse(false, "", "Missing pattern parameter", id);
    }
    
    try {
        uintptr_t start = startStr.empty() ? 0 : std::stoull(startStr, nullptr, 16);
        uintptr_t end = endStr.empty() ? 0 : std::stoull(endStr, nullptr, 16);
        
        auto results = MemoryEngine::AOBScanAll(pattern, start, end);
        
        std::stringstream ss;
        ss << "[";
        
        size_t maxResults = min(results.size(), static_cast<size_t>(100));
        for (size_t i = 0; i < maxResults; i++) {
            if (i > 0) ss << ",";
            ss << "\"0x" << std::hex << results[i] << "\"";
        }
        
        ss << "]";
        
        std::string message = "Found " + std::to_string(results.size()) + " matches";
        if (results.size() > maxResults) {
            message += " (showing first " + std::to_string(maxResults) + ")";
        }
        
        return CreateResponse(true, ss.str(), message, id);
    } catch (...) {
        return CreateResponse(false, "", "Pattern scan failed", id);
    }
}

std::string CommandRouter::HandleModuleInfo(const std::string& params) {
    std::string moduleName = ExtractJsonValue(params, "name");
    std::string id = ExtractJsonValue(params, "id");
    
    if (moduleName.empty()) {
        return CreateResponse(false, "", "Missing module name parameter", id);
    }
    
    try {
        auto region = MemoryEngine::GetModuleRegion(moduleName);
        if (!region.has_value()) {
            return CreateResponse(false, "", "Module not found: " + moduleName, id);
        }
        
        uintptr_t base = MemoryEngine::GetModuleBase(moduleName);
        size_t size = MemoryEngine::GetModuleSize(moduleName);
        
        std::stringstream ss;
        ss << "{";
        ss << "\"name\":\"" << EscapeJsonString(moduleName) << "\",";
        ss << "\"baseAddress\":\"0x" << std::hex << base << "\",";
        ss << "\"size\":" << std::dec << size << ",";
        ss << "\"endAddress\":\"0x" << std::hex << (base + size) << "\",";
        ss << "\"protection\":" << region->protection << ",";
        ss << "\"readable\":" << (region->readable ? "true" : "false") << ",";
        ss << "\"writable\":" << (region->writable ? "true" : "false") << ",";
        ss << "\"executable\":" << (region->executable ? "true" : "false") << ",";
        ss << "\"path\":\"" << EscapeJsonString(region->moduleName) << "\"";
        ss << "}";
        
        return CreateResponse(true, ss.str(), "", id);
    } catch (...) {
        return CreateResponse(false, "", "Failed to get module info", id);
    }
}

std::string CommandRouter::HandleMemoryPatch(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string originalStr = ExtractJsonValue(params, "original");
    std::string newStr = ExtractJsonValue(params, "new");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty() || originalStr.empty() || newStr.empty()) {
        return CreateResponse(false, "", "Missing required parameters", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        auto originalBytes = MemoryEngine::HexStringToBytes(originalStr);
        auto newBytes = MemoryEngine::HexStringToBytes(newStr);
        
        if (MemoryEngine::PatchBytes(address, originalBytes, newBytes)) {
            return CreateResponse(true, "{}", "Patch applied successfully", id);
        } else {
            return CreateResponse(false, "", "Failed to apply patch - original bytes don't match or write failed", id);
        }
    } catch (...) {
        return CreateResponse(false, "", "Patch operation failed", id);
    }
}

std::string CommandRouter::HandleMemoryNop(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string sizeStr = ExtractJsonValue(params, "size");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty() || sizeStr.empty()) {
        return CreateResponse(false, "", "Missing address or size parameter", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        size_t size = std::stoull(sizeStr);
        
        if (MemoryEngine::NopInstruction(address, size)) {
            return CreateResponse(true, "{}", "NOP patch applied successfully", id);
        } else {
            return CreateResponse(false, "", "Failed to apply NOP patch", id);
        }
    } catch (...) {
        return CreateResponse(false, "", "NOP operation failed", id);
    }
}

std::string CommandRouter::HandlePointerChain(const std::string& params) {
    std::string baseStr = ExtractJsonValue(params, "base");
    std::string offsetsStr = ExtractJsonValue(params, "offsets");
    std::string id = ExtractJsonValue(params, "id");
    
    if (baseStr.empty() || offsetsStr.empty()) {
        return CreateResponse(false, "", "Missing base or offsets parameter", id);
    }
    
    try {
        uintptr_t baseAddress = std::stoull(baseStr, nullptr, 16);
        
        // 오프셋 파싱 [0x10, 0x20, 0x30] 형태
        std::vector<uintptr_t> offsets;
        std::string cleanOffsets = offsetsStr;
        
        // 대괄호 제거
        if (cleanOffsets.front() == '[') cleanOffsets.erase(0, 1);
        if (cleanOffsets.back() == ']') cleanOffsets.pop_back();
        
        std::istringstream iss(cleanOffsets);
        std::string offset;
        while (std::getline(iss, offset, ',')) {
            // 공백 제거
            offset.erase(0, offset.find_first_not_of(" \t"));
            offset.erase(offset.find_last_not_of(" \t") + 1);
            
            if (!offset.empty()) {
                offsets.push_back(std::stoull(offset, nullptr, 16));
            }
        }
        
        auto result = MemoryEngine::FollowPointerChain(baseAddress, offsets);
        
        if (result.has_value()) {
            std::stringstream ss;
            ss << "\"0x" << std::hex << result.value() << "\"";
            return CreateResponse(true, ss.str(), "Pointer chain followed successfully", id);
        } else {
            return CreateResponse(false, "", "Failed to follow pointer chain - invalid address encountered", id);
        }
    } catch (...) {
        return CreateResponse(false, "", "Pointer chain operation failed", id);
    }
}

std::string CommandRouter::HandlePointerFind(const std::string& params) {
    std::string targetStr = ExtractJsonValue(params, "target");
    std::string startStr = ExtractJsonValue(params, "start");
    std::string endStr = ExtractJsonValue(params, "end");
    std::string id = ExtractJsonValue(params, "id");
    
    if (targetStr.empty()) {
        return CreateResponse(false, "", "Missing target address parameter", id);
    }
    
    try {
        uintptr_t targetAddress = std::stoull(targetStr, nullptr, 16);
        
        ScanOptions options;
        if (!startStr.empty()) options.startAddress = std::stoull(startStr, nullptr, 16);
        if (!endStr.empty()) options.endAddress = std::stoull(endStr, nullptr, 16);
        
        auto pointers = MemoryEngine::FindPointersTo(targetAddress, options);
        
        std::stringstream ss;
        ss << "[";
        
        size_t maxResults = min(pointers.size(), static_cast<size_t>(100));
        for (size_t i = 0; i < maxResults; i++) {
            if (i > 0) ss << ",";
            ss << "\"0x" << std::hex << pointers[i] << "\"";
        }
        
        ss << "]";
        
        std::string message = "Found " + std::to_string(pointers.size()) + " pointers";
        if (pointers.size() > maxResults) {
            message += " (showing first " + std::to_string(maxResults) + ")";
        }
        
        return CreateResponse(true, ss.str(), message, id);
    } catch (...) {
        return CreateResponse(false, "", "Pointer search failed", id);
    }
}

std::string CommandRouter::HandlePatternScan(const std::string& params) {
    std::string pattern = ExtractJsonValue(params, "pattern");
    std::string startStr = ExtractJsonValue(params, "start");
    std::string endStr = ExtractJsonValue(params, "end");
    std::string id = ExtractJsonValue(params, "id");
    
    if (pattern.empty()) {
        return CreateResponse(false, "", "Missing pattern parameter", id);
    }
    
    try {
        uintptr_t start = startStr.empty() ? 0 : std::stoull(startStr, nullptr, 16);
        uintptr_t end = endStr.empty() ? 0 : std::stoull(endStr, nullptr, 16);
        
        auto result = MemoryEngine::AOBScanFirst(pattern, start, end);
        
        if (result.has_value()) {
            std::stringstream ss;
            ss << "\"0x" << std::hex << result.value() << "\"";
            return CreateResponse(true, ss.str(), "", id);
        } else {
            return CreateResponse(false, "", "Pattern not found", id);
        }
    } catch (...) {
        return CreateResponse(false, "", "Pattern scan failed", id);
    }
}

std::string CommandRouter::HandleModuleList(const std::string& params) {
    std::string id = ExtractJsonValue(params, "id");
    
    try {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (hSnap == INVALID_HANDLE_VALUE) {
            return CreateResponse(false, "", "Failed to create snapshot", id);
        }
        
        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);
        
        std::stringstream ss;
        ss << "[";
        
        bool first = true;
        if (Module32First(hSnap, &me32)) {
            do {
                if (!first) ss << ",";
                first = false;
                
                ss << "{";
                ss << "\"name\":\"" << EscapeJsonString(me32.szModule) << "\",";
                ss << "\"path\":\"" << EscapeJsonString(me32.szExePath) << "\",";
                ss << "\"base\":\"0x" << std::hex << reinterpret_cast<uintptr_t>(me32.modBaseAddr) << "\",";
                ss << "\"size\":" << std::dec << me32.modBaseSize;
                ss << "}";
            } while (Module32Next(hSnap, &me32));
        }
        
        ss << "]";
        CloseHandle(hSnap);
        
        return CreateResponse(true, ss.str(), "", id);
    } catch (...) {
        return CreateResponse(false, "", "Failed to list modules", id);
    }
}

std::string CommandRouter::HandleProcessInfo(const std::string& params) {
    std::string id = ExtractJsonValue(params, "id");
    
    try {
        DWORD pid = GetCurrentProcessId();
        HANDLE hProcess = GetCurrentProcess();
        
        char processPath[MAX_PATH];
        GetModuleFileNameA(nullptr, processPath, MAX_PATH);
        const char* processNameOnly = PathFindFileNameA(processPath);
        
        BOOL isWow64 = FALSE;
        IsWow64Process(hProcess, &isWow64);
        
        #ifdef _WIN64
        std::string platform = "x64";
        int addressWidth = 64;
        #else
        std::string platform = isWow64 ? "x86 (WoW64)" : "x86";
        int addressWidth = 32;
        #endif
        
        HMODULE hModule = GetModuleHandle(nullptr);
        MODULEINFO modInfo = {0};
        GetModuleInformation(hProcess, hModule, &modInfo, sizeof(MODULEINFO));

        auto regions = MemoryEngine::GetMemoryRegions();
        size_t totalSize = 0, writableSize = 0, executableSize = 0;
        for(const auto& region : regions) {
            totalSize += region.size;
            if(region.writable) writableSize += region.size;
            if(region.executable) executableSize += region.size;
        }

        std::stringstream ss;
        ss << "{";
        ss << "\"pid\":" << pid << ",";
        ss << "\"name\":\"" << EscapeJsonString(processNameOnly) << "\",";
        ss << "\"platform\":\"" << platform << "\",";
        ss << "\"addressWidth\":" << addressWidth << ",";
        ss << "\"mainModule\":{";
        ss << "\"baseAddress\":" << reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll) << ",";
        ss << "\"size\":" << modInfo.SizeOfImage;
        ss << "},";
        ss << "\"memoryMetrics\":{";
        ss << "\"total\":" << totalSize << ",";
        ss << "\"writable\":" << writableSize << ",";
        ss << "\"executable\":" << executableSize;
        ss << "}";
        ss << "}";
        
        return CreateResponse(true, ss.str(), "", id);
    } catch (...) {
        return CreateResponse(false, "", "Failed to get process info", id);
    }
}

std::string CommandRouter::HandleHookList(const std::string& params) {
    if (!g_HookManager) {
        return CreateResponse(false, "", "Hook manager not initialized", params);
    }
    
    try {
        auto hooks = g_HookManager->GetAllHooks();
        
        std::stringstream ss;
        ss << "[";
        
        bool first = true;
        for (const auto& hook : hooks) {
            if (!first) ss << ",";
            first = false;
            
            ss << "{";
            ss << "\"name\":\"" << EscapeJsonString(hook.name) << "\",";
            ss << "\"target\":\"0x" << std::hex << hook.targetAddress << "\",";
            ss << "\"detour\":\"0x" << std::hex << hook.detourAddress << "\",";
            ss << "\"original\":\"0x" << std::hex << hook.originalAddress << "\",";
            ss << "\"active\":" << (hook.isActive ? "true" : "false");
            ss << "}";
        }
        
        ss << "]";
        
        return CreateResponse(true, ss.str(), "", params);
    } catch (...) {
        return CreateResponse(false, "", "Failed to list hooks", params);
    }
}

std::string CommandRouter::HandleAllocateMemory(const std::string& params) {
    std::string sizeStr = ExtractJsonValue(params, "size");
    std::string protectionStr = ExtractJsonValue(params, "protection");
    std::string id = ExtractJsonValue(params, "id");
    
    if (sizeStr.empty()) {
        return CreateResponse(false, "", "Missing size parameter", id);
    }
    
    try {
        size_t size = std::stoull(sizeStr);
        DWORD protection = PAGE_EXECUTE_READWRITE;
        
        if (!protectionStr.empty()) {
            protection = std::stoul(protectionStr, nullptr, 16);
        }
        
        uintptr_t address = MemoryEngine::AllocateMemory(size, protection);
        
        if (address) {
            std::stringstream ss;
            ss << "\"0x" << std::hex << address << "\"";
            return CreateResponse(true, ss.str(), "", id);
        } else {
            return CreateResponse(false, "", "Failed to allocate memory", id);
        }
    } catch (...) {
        return CreateResponse(false, "", "Memory allocation failed", id);
    }
}

std::string CommandRouter::HandleFreeMemory(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty()) {
        return CreateResponse(false, "", "Missing address parameter", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        
        if (MemoryEngine::FreeMemory(address)) {
            return CreateResponse(true, "{}", "", id);
        } else {
            return CreateResponse(false, "", "Failed to free memory", id);
        }
    } catch (...) {
        return CreateResponse(false, "", "Memory free failed", id);
    }
}

// Stub implementations for remaining handlers
std::string CommandRouter::HandleHookInstall(const std::string& params) {
    return CreateResponse(false, "", "Hook functionality not implemented yet", params);
}

std::string CommandRouter::HandleHookRemove(const std::string& params) {
    return CreateResponse(false, "", "Hook functionality not implemented yet", params);
}

std::string CommandRouter::HandleHookToggle(const std::string& params) {
    return CreateResponse(false, "", "Hook functionality not implemented yet", params);
}

// Enhanced memory.read_value command for better type handling
std::string CommandRouter::HandleMemoryReadValue(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string typeStr = ExtractJsonValue(params, "type");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty() || typeStr.empty()) {
        return CreateResponse(false, "", "Missing address or type parameter", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        
        // Validate address
        if (!MemoryEngine::IsAddressValid(address, 8)) {
            return CreateResponse(false, "", "Invalid memory address", id);
        }
        
        if (!MemoryEngine::IsAddressReadable(address, 8)) {
            return CreateResponse(false, "", "Memory address is not readable", id);
        }
        
        if (typeStr == "int32" || typeStr == "int") {
            auto value = MemoryEngine::SafeRead<int32_t>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read integer value", id);
            }
            return CreateResponse(true, std::to_string(value.value()), "", id);
        } 
        else if (typeStr == "int64") {
            auto value = MemoryEngine::SafeRead<int64_t>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read int64 value", id);
            }
            return CreateResponse(true, std::to_string(value.value()), "", id);
        }
        else if (typeStr == "float") {
            auto value = MemoryEngine::SafeRead<float>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read float value", id);
            }
            return CreateResponse(true, std::to_string(value.value()), "", id);
        } 
        else if (typeStr == "double") {
            auto value = MemoryEngine::SafeRead<double>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read double value", id);
            }
            return CreateResponse(true, std::to_string(value.value()), "", id);
        } 
        else if (typeStr == "byte") {
            auto value = MemoryEngine::SafeRead<uint8_t>(address);
            if (!value.has_value()) {
                return CreateResponse(false, "", "Failed to read byte value", id);
            }
            return CreateResponse(true, std::to_string(static_cast<int>(value.value())), "", id);
        }
        else if (typeStr == "string") {
            auto bytes = MemoryEngine::SafeReadBytes(address, 256);
            if (bytes.empty()) {
                return CreateResponse(false, "", "Failed to read string", id);
            }
            
            // Find null terminator
            size_t nullPos = 0;
            for (size_t i = 0; i < bytes.size(); i++) {
                if (bytes[i] == 0) {
                    nullPos = i;
                    break;
                }
                nullPos = i + 1;
            }
            
            std::string result(reinterpret_cast<char*>(bytes.data()), nullPos);
            return CreateResponse(true, "\"" + EscapeJsonString(result) + "\"", "", id);
        }
        else if (typeStr == "bytes") {
            auto bytes = MemoryEngine::SafeReadBytes(address, 16);
            if (bytes.empty()) {
                return CreateResponse(false, "", "Failed to read bytes", id);
            }
            
            std::stringstream ss;
            for (size_t i = 0; i < bytes.size(); i++) {
                if (i > 0) ss << " ";
                ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(bytes[i]);
            }
            return CreateResponse(true, "\"" + ss.str() + "\"", "", id);
        }
        
        return CreateResponse(false, "", "Unknown type: " + typeStr, id);
    } catch (...) {
        return CreateResponse(false, "", "Failed to read memory value", id);
    }
}

// Get module information for a given address
std::string CommandRouter::HandleModuleFromAddress(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty()) {
        return CreateResponse(false, "", "Missing address parameter", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        
        HANDLE hProcess = GetCurrentProcess();
        HMODULE hModules[1024];
        DWORD cbNeeded;
        
        if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
            for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
                MODULEINFO modInfo;
                if (GetModuleInformation(hProcess, hModules[i], &modInfo, sizeof(modInfo))) {
                    uintptr_t moduleBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
                    uintptr_t moduleEnd = moduleBase + modInfo.SizeOfImage;
                    
                    if (address >= moduleBase && address < moduleEnd) {
                        char moduleName[MAX_PATH];
                        if (GetModuleFileNameExA(hProcess, hModules[i], moduleName, MAX_PATH)) {
                            // Extract just the filename
                            char* fileName = PathFindFileNameA(moduleName);
                            uintptr_t offset = address - moduleBase;
                            
                            std::stringstream ss;
                            ss << "{";
                            ss << "\"moduleName\":\"" << fileName << "\",";
                            ss << "\"baseAddress\":\"0x" << std::hex << moduleBase << "\",";
                            ss << "\"offset\":\"0x" << std::hex << offset << "\",";
                            ss << "\"displayName\":\"" << fileName << "+0x" << std::hex << offset << "\"";
                            ss << "}";
                            
                            return CreateResponse(true, ss.str(), "", id);
                        }
                    }
                }
            }
        }
        
        return CreateResponse(false, "", "Address not found in any loaded module", id);
    } catch (...) {
        return CreateResponse(false, "", "Failed to get module information", id);
    }
}

// Advanced x86/x64 instruction analysis
struct DisasmInstruction {
    uintptr_t address;
    std::vector<uint8_t> bytes;
    std::string mnemonic;
    std::string operands;
    size_t length;
    bool isJump;
    bool isCall;
    bool isRet;
    uintptr_t targetAddress; // For jumps/calls
};

// Basic x86/x64 instruction decoder (simplified implementation)
class SimpleDisassembler {
public:
    static std::vector<DisasmInstruction> Disassemble(uintptr_t address, const std::vector<uint8_t>& bytes, bool is64bit) {
        std::vector<DisasmInstruction> instructions;
        size_t offset = 0;
        
        while (offset < bytes.size() && instructions.size() < 100) { // Limit to 100 instructions
            DisasmInstruction inst;
            inst.address = address + offset;
            
            size_t instLen = DecodeInstruction(bytes, offset, inst, is64bit);
            if (instLen == 0) {
                // Unknown instruction, treat as single byte
                inst.bytes.push_back(bytes[offset]);
                inst.mnemonic = "db";
                inst.operands = "0x" + std::to_string(bytes[offset]);
                inst.length = 1;
                instLen = 1;
            }
            
            instructions.push_back(inst);
            offset += instLen;
        }
        
        return instructions;
    }
    
private:
    static size_t DecodeInstruction(const std::vector<uint8_t>& bytes, size_t offset, DisasmInstruction& inst, bool is64bit) {
        if (offset >= bytes.size()) return 0;
        
        uint8_t opcode = bytes[offset];
        inst.bytes.clear();
        inst.bytes.push_back(opcode);
        inst.isJump = inst.isCall = inst.isRet = false;
        inst.targetAddress = 0;
        
        // Simple x86/x64 instruction decoding
        switch (opcode) {
            case 0x90: // NOP
                inst.mnemonic = "nop";
                inst.operands = "";
                inst.length = 1;
                return 1;
                
            case 0xC3: // RET
                inst.mnemonic = "ret";
                inst.operands = "";
                inst.isRet = true;
                inst.length = 1;
                return 1;
                
            case 0x55: // PUSH EBP/RBP
                inst.mnemonic = "push";
                inst.operands = is64bit ? "rbp" : "ebp";
                inst.length = 1;
                return 1;
                
            case 0x5D: // POP EBP/RBP
                inst.mnemonic = "pop";
                inst.operands = is64bit ? "rbp" : "ebp";
                inst.length = 1;
                return 1;
                
            case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x56: case 0x57: // PUSH reg
                inst.mnemonic = "push";
                inst.operands = GetRegisterName(opcode - 0x50, is64bit, true);
                inst.length = 1;
                return 1;
                
            case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5E: case 0x5F: // POP reg
                inst.mnemonic = "pop";
                inst.operands = GetRegisterName(opcode - 0x58, is64bit, true);
                inst.length = 1;
                return 1;
                
            case 0x8B: // MOV reg, r/m
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "mov";
                    inst.operands = DecodeModRM(modrm, is64bit);
                    inst.length = 2;
                    return 2;
                }
                break;
                
            case 0x89: // MOV r/m, reg
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "mov";
                    inst.operands = DecodeModRM(modrm, is64bit, true); // Reverse operands
                    inst.length = 2;
                    return 2;
                }
                break;
                
            case 0xE8: // CALL rel32
                if (offset + 4 < bytes.size()) {
                    for (int i = 1; i <= 4; i++) inst.bytes.push_back(bytes[offset + i]);
                    int32_t rel32 = *reinterpret_cast<const int32_t*>(&bytes[offset + 1]);
                    inst.mnemonic = "call";
                    inst.targetAddress = inst.address + 5 + rel32;
                    std::stringstream addrStream;
                    addrStream << std::hex << std::uppercase << inst.targetAddress;
                    inst.operands = "0x" + addrStream.str();
                    inst.isCall = true;
                    inst.length = 5;
                    return 5;
                }
                break;
                
            case 0xE9: // JMP rel32
                if (offset + 4 < bytes.size()) {
                    for (int i = 1; i <= 4; i++) inst.bytes.push_back(bytes[offset + i]);
                    int32_t rel32 = *reinterpret_cast<const int32_t*>(&bytes[offset + 1]);
                    inst.mnemonic = "jmp";
                    inst.targetAddress = inst.address + 5 + rel32;
                    std::stringstream addrStream;
                    addrStream << std::hex << std::uppercase << inst.targetAddress;
                    inst.operands = "0x" + addrStream.str();
                    inst.isJump = true;
                    inst.length = 5;
                    return 5;
                }
                break;
                
            case 0xCC: // INT3 (breakpoint)
                inst.mnemonic = "int3";
                inst.operands = "";
                inst.length = 1;
                return 1;
                
            case 0xCB: // RET far
                inst.mnemonic = "retf";
                inst.operands = "";
                inst.isRet = true;
                inst.length = 1;
                return 1;
                
            case 0xC2: // RET near imm16
                if (offset + 2 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    inst.bytes.push_back(bytes[offset + 2]);
                    uint16_t imm16 = *reinterpret_cast<const uint16_t*>(&bytes[offset + 1]);
                    inst.mnemonic = "ret";
                    inst.operands = "0x" + std::to_string(imm16);
                    inst.isRet = true;
                    inst.length = 3;
                    return 3;
                }
                break;
                
            case 0x6A: // PUSH imm8
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    int8_t imm8 = static_cast<int8_t>(bytes[offset + 1]);
                    inst.mnemonic = "push";
                    inst.operands = "0x" + std::to_string(imm8);
                    inst.length = 2;
                    return 2;
                }
                break;
                
            case 0x68: // PUSH imm32
                if (offset + 4 < bytes.size()) {
                    for (int i = 1; i <= 4; i++) inst.bytes.push_back(bytes[offset + i]);
                    int32_t imm32 = *reinterpret_cast<const int32_t*>(&bytes[offset + 1]);
                    inst.mnemonic = "push";
                    inst.operands = "0x" + std::to_string(imm32);
                    inst.length = 5;
                    return 5;
                }
                break;
                
            case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7: // MOV reg8, imm8
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t imm8 = bytes[offset + 1];
                    inst.mnemonic = "mov";
                    inst.operands = GetRegisterName8(opcode - 0xB0) + ", 0x" + std::to_string(imm8);
                    inst.length = 2;
                    return 2;
                }
                break;
                
            case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF: // MOV reg, imm32
                if (offset + 4 < bytes.size()) {
                    for (int i = 1; i <= 4; i++) inst.bytes.push_back(bytes[offset + i]);
                    uint32_t imm32 = *reinterpret_cast<const uint32_t*>(&bytes[offset + 1]);
                    inst.mnemonic = "mov";
                    inst.operands = GetRegisterName(opcode - 0xB8, is64bit, true) + ", 0x" + std::to_string(imm32);
                    inst.length = 5;
                    return 5;
                }
                break;
                
            case 0x01: // ADD r/m, reg
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "add";
                    inst.operands = DecodeModRM(modrm, is64bit, true);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0x03: // ADD reg, r/m
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "add";
                    inst.operands = DecodeModRM(modrm, is64bit);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0x29: // SUB r/m, reg
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "sub";
                    inst.operands = DecodeModRM(modrm, is64bit, true);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0x2B: // SUB reg, r/m
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "sub";
                    inst.operands = DecodeModRM(modrm, is64bit);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0x39: // CMP r/m, reg
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "cmp";
                    inst.operands = DecodeModRM(modrm, is64bit, true);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0x3B: // CMP reg, r/m
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "cmp";
                    inst.operands = DecodeModRM(modrm, is64bit);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0x85: // TEST r/m, reg
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    inst.mnemonic = "test";
                    inst.operands = DecodeModRM(modrm, is64bit, true);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0xFF: // CALL/JMP r/m (based on ModR/M reg field)
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    uint8_t modrm = bytes[offset + 1];
                    int reg = (modrm >> 3) & 7;
                    if (reg == 2) { // CALL r/m
                        inst.mnemonic = "call";
                        inst.isCall = true;
                    } else if (reg == 4) { // JMP r/m
                        inst.mnemonic = "jmp";
                        inst.isJump = true;
                    } else {
                        inst.mnemonic = "???";
                    }
                    inst.operands = DecodeModRMOperand(modrm, is64bit);
                    inst.length = 2 + GetModRMExtraBytes(modrm, bytes, offset + 2);
                    return inst.length;
                }
                break;
                
            case 0xEB: // JMP rel8
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    int8_t rel8 = static_cast<int8_t>(bytes[offset + 1]);
                    inst.mnemonic = "jmp";
                    inst.targetAddress = inst.address + 2 + rel8;
                    std::stringstream addrStream;
                    addrStream << std::hex << std::uppercase << inst.targetAddress;
                    inst.operands = "0x" + addrStream.str();
                    inst.isJump = true;
                    inst.length = 2;
                    return 2;
                }
                break;
                
            case 0x0F: // Two-byte opcodes
                if (offset + 1 < bytes.size()) {
                    uint8_t opcode2 = bytes[offset + 1];
                    inst.bytes.push_back(opcode2);
                    
                    // Conditional jumps (near form) 0x0F 0x8x
                    if ((opcode2 & 0xF0) == 0x80) {
                        if (offset + 5 < bytes.size()) {
                            for (int i = 2; i <= 5; i++) inst.bytes.push_back(bytes[offset + i]);
                            int32_t rel32 = *reinterpret_cast<const int32_t*>(&bytes[offset + 2]);
                            inst.mnemonic = GetConditionalJumpName(opcode2 - 0x10); // Convert 0x8x to 0x7x
                            inst.targetAddress = inst.address + 6 + rel32;
                            std::stringstream addrStream;
                            addrStream << std::hex << std::uppercase << inst.targetAddress;
                            inst.operands = "0x" + addrStream.str();
                            inst.isJump = true;
                            inst.length = 6;
                            return 6;
                        }
                    }
                    
                    // Other two-byte instructions can be added here
                    inst.mnemonic = "db";
                    inst.operands = "0x0F, 0x" + std::to_string(opcode2);
                    inst.length = 2;
                    return 2;
                }
                break;
                
            // Conditional jumps (short form) - Enhanced to support all conditional jumps
            case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
                if (offset + 1 < bytes.size()) {
                    inst.bytes.push_back(bytes[offset + 1]);
                    int8_t rel8 = static_cast<int8_t>(bytes[offset + 1]);
                    inst.mnemonic = GetConditionalJumpName(opcode);
                    inst.targetAddress = inst.address + 2 + rel8;
                    std::stringstream addrStream;
                    addrStream << std::hex << std::uppercase << inst.targetAddress;
                    inst.operands = "0x" + addrStream.str();
                    inst.isJump = true;
                    inst.length = 2;
                    return 2;
                }
                break;
        }
        
        return 0; // Unknown instruction
    }
    
    static std::string GetRegisterName(int reg, bool is64bit, bool fullSize = false) {
        const char* regs32[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
        const char* regs64[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"};
        
        if (reg < 0 || reg > 7) return "???";
        return fullSize ? (is64bit ? regs64[reg] : regs32[reg]) : regs32[reg];
    }
    
    static std::string GetConditionalJumpName(uint8_t opcode) {
        switch (opcode) {
            case 0x70: return "jo";
            case 0x71: return "jno";
            case 0x72: return "jb";
            case 0x73: return "jae";
            case 0x74: return "je";
            case 0x75: return "jne";
            case 0x76: return "jbe";
            case 0x77: return "ja";
            case 0x78: return "js";
            case 0x79: return "jns";
            case 0x7A: return "jp";
            case 0x7B: return "jnp";
            case 0x7C: return "jl";
            case 0x7D: return "jge";
            case 0x7E: return "jle";
            case 0x7F: return "jg";
            default: return "jcc";
        }
    }
    
    static std::string GetRegisterName8(int reg) {
        const char* regs8[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
        if (reg < 0 || reg > 7) return "???";
        return regs8[reg];
    }
    
    static size_t GetModRMExtraBytes(uint8_t modrm, const std::vector<uint8_t>& bytes, size_t offset) {
        int mod = (modrm >> 6) & 3;
        int rm = modrm & 7;
        
        if (mod == 0) {
            if (rm == 5) return 4; // [disp32]
            if (rm == 4) return 1; // SIB byte
        } else if (mod == 1) {
            return (rm == 4) ? 2 : 1; // [reg + disp8] or [SIB + disp8]
        } else if (mod == 2) {
            return (rm == 4) ? 5 : 4; // [reg + disp32] or [SIB + disp32]
        }
        return 0; // mod == 3 (register)
    }
    
    static std::string DecodeModRMOperand(uint8_t modrm, bool is64bit) {
        int mod = (modrm >> 6) & 3;
        int rm = modrm & 7;
        
        if (mod == 3) {
            return GetRegisterName(rm, is64bit);
        } else {
            return "[" + GetRegisterName(rm, is64bit) + "]";
        }
    }
    
    static std::string DecodeModRM(uint8_t modrm, bool is64bit, bool reverse = false) {
        int mod = (modrm >> 6) & 3;
        int reg = (modrm >> 3) & 7;
        int rm = modrm & 7;
        
        std::string regName = GetRegisterName(reg, is64bit);
        std::string rmName;
        
        if (mod == 3) {
            rmName = GetRegisterName(rm, is64bit);
        } else {
            rmName = "[" + GetRegisterName(rm, is64bit) + "]";
        }
        
        return reverse ? (rmName + ", " + regName) : (regName + ", " + rmName);
    }
};

// Enhanced disassembly command
std::string CommandRouter::HandleMemoryDisassemble(const std::string& params) {
    std::string addressStr = ExtractJsonValue(params, "address");
    std::string sizeStr = ExtractJsonValue(params, "size");
    std::string id = ExtractJsonValue(params, "id");
    
    if (addressStr.empty() || sizeStr.empty()) {
        return CreateResponse(false, "", "Missing address or size parameter", id);
    }
    
    try {
        uintptr_t address = std::stoull(addressStr, nullptr, 16);
        size_t size = std::stoull(sizeStr);
        
        // Validate address
        if (!MemoryEngine::IsAddressValid(address, size)) {
            return CreateResponse(false, "", "Invalid memory address or size", id);
        }
        
        if (!MemoryEngine::IsAddressReadable(address, size)) {
            return CreateResponse(false, "", "Memory address is not readable", id);
        }
        
        auto bytes = MemoryEngine::SafeReadBytes(address, size);
        if (bytes.empty()) {
            return CreateResponse(false, "", "Failed to read memory for disassembly", id);
        }
        
        // Detect architecture (simplified check)
        bool is64bit = sizeof(void*) == 8;
        
        // Disassemble instructions
        auto instructions = SimpleDisassembler::Disassemble(address, bytes, is64bit);
        
        std::stringstream ss;
        ss << "[";
        
        for (size_t i = 0; i < instructions.size(); i++) {
            if (i > 0) ss << ",";
            
            const auto& inst = instructions[i];
            ss << "{";
            ss << "\"address\":\"0x" << std::hex << inst.address << "\",";
            
            // Format bytes
            std::stringstream bytesStr;
            for (size_t j = 0; j < inst.bytes.size(); j++) {
                if (j > 0) bytesStr << " ";
                bytesStr << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(inst.bytes[j]);
            }
            ss << "\"bytes\":\"" << bytesStr.str() << "\",";
            
            ss << "\"mnemonic\":\"" << inst.mnemonic << "\",";
            ss << "\"operands\":\"" << EscapeJsonString(inst.operands) << "\",";
            ss << "\"length\":" << inst.length << ",";
            ss << "\"isJump\":" << (inst.isJump ? "true" : "false") << ",";
            ss << "\"isCall\":" << (inst.isCall ? "true" : "false") << ",";
            ss << "\"isRet\":" << (inst.isRet ? "true" : "false");
            
            if (inst.targetAddress != 0) {
                ss << ",\"target\":\"0x" << std::hex << inst.targetAddress << "\"";
            }
            
            ss << "}";
        }
        
        ss << "]";
        return CreateResponse(true, ss.str(), "", id);
    } catch (...) {
        return CreateResponse(false, "", "Disassembly failed", id);
    }
}

} // namespace InternalEngine