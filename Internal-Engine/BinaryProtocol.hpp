#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace InternalEngine {

// Ultra-fast binary protocol for maximum performance
// Reduces JSON parsing overhead for real-time memory monitoring

enum class BinaryOpcode : uint8_t {
    MEMORY_READ = 0x01,
    MEMORY_WRITE = 0x02,
    MEMORY_SCAN = 0x03,
    VALUE_UPDATE = 0x04,
    BULK_UPDATE = 0x05,
    PROCESS_INFO = 0x06,
    MODULE_LIST = 0x07,
    PING = 0x08,
    PONG = 0x09
};

enum class DataType : uint8_t {
    INT32 = 0x01,
    INT64 = 0x02,
    FLOAT = 0x03,
    DOUBLE = 0x04,
    STRING = 0x05,
    BYTES = 0x06
};

struct BinaryHeader {
    uint32_t magic = 0x494E544C; // 'INTL' for Internal Engine
    uint16_t version = 0x0001;
    BinaryOpcode opcode;
    uint8_t flags;
    uint32_t payloadSize;
    uint32_t requestId;
};

struct MemoryReadRequest {
    uint64_t address;
    uint32_t size;
    DataType type;
};

struct MemoryWriteRequest {
    uint64_t address;
    DataType type;
    uint32_t dataSize;
    // Followed by data bytes
};

struct ValueUpdateNotification {
    uint64_t address;
    DataType type;
    uint32_t valueSize;
    // Followed by value bytes
};

struct BulkUpdateNotification {
    uint32_t count;
    // Followed by count * ValueUpdateNotification
};

class BinaryProtocol {
public:
    // Encode binary message
    static std::vector<uint8_t> EncodeMemoryRead(uint32_t requestId, uint64_t address, uint32_t size, DataType type);
    static std::vector<uint8_t> EncodeMemoryWrite(uint32_t requestId, uint64_t address, DataType type, const std::vector<uint8_t>& data);
    static std::vector<uint8_t> EncodeValueUpdate(uint64_t address, DataType type, const std::vector<uint8_t>& value);
    static std::vector<uint8_t> EncodeBulkUpdate(const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& updates);
    
    // Decode binary message
    static bool DecodeHeader(const std::vector<uint8_t>& data, BinaryHeader& header);
    static bool DecodeMemoryRead(const std::vector<uint8_t>& payload, MemoryReadRequest& request);
    static bool DecodeMemoryWrite(const std::vector<uint8_t>& payload, MemoryWriteRequest& request, std::vector<uint8_t>& data);
    
    // Utility functions
    static std::string DataTypeToString(DataType type);
    static DataType StringToDataType(const std::string& typeStr);
    static uint64_t StringToAddress(const std::string& addressStr);
    static std::string AddressToString(uint64_t address);
    
private:
    static void WriteUInt32(std::vector<uint8_t>& buffer, uint32_t value);
    static void WriteUInt64(std::vector<uint8_t>& buffer, uint64_t value);
    static uint32_t ReadUInt32(const uint8_t* data);
    static uint64_t ReadUInt64(const uint8_t* data);
};

} // namespace InternalEngine