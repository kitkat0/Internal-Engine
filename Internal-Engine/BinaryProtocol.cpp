#include "BinaryProtocol.hpp"
#include <cstring>
#include <algorithm>

namespace InternalEngine {

std::vector<uint8_t> BinaryProtocol::EncodeMemoryRead(uint32_t requestId, uint64_t address, uint32_t size, DataType type) {
    std::vector<uint8_t> message;
    
    // Header
    BinaryHeader header;
    header.opcode = BinaryOpcode::MEMORY_READ;
    header.flags = 0;
    header.payloadSize = sizeof(MemoryReadRequest);
    header.requestId = requestId;
    
    message.resize(sizeof(BinaryHeader) + sizeof(MemoryReadRequest));
    memcpy(message.data(), &header, sizeof(BinaryHeader));
    
    // Payload
    MemoryReadRequest request;
    request.address = address;
    request.size = size;
    request.type = type;
    
    memcpy(message.data() + sizeof(BinaryHeader), &request, sizeof(MemoryReadRequest));
    
    return message;
}

std::vector<uint8_t> BinaryProtocol::EncodeMemoryWrite(uint32_t requestId, uint64_t address, DataType type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> message;
    
    // Header
    BinaryHeader header;
    header.opcode = BinaryOpcode::MEMORY_WRITE;
    header.flags = 0;
    header.payloadSize = sizeof(MemoryWriteRequest) + static_cast<uint32_t>(data.size());
    header.requestId = requestId;
    
    message.resize(sizeof(BinaryHeader) + sizeof(MemoryWriteRequest) + data.size());
    memcpy(message.data(), &header, sizeof(BinaryHeader));
    
    // Payload
    MemoryWriteRequest request;
    request.address = address;
    request.type = type;
    request.dataSize = static_cast<uint32_t>(data.size());
    
    memcpy(message.data() + sizeof(BinaryHeader), &request, sizeof(MemoryWriteRequest));
    memcpy(message.data() + sizeof(BinaryHeader) + sizeof(MemoryWriteRequest), data.data(), data.size());
    
    return message;
}

std::vector<uint8_t> BinaryProtocol::EncodeValueUpdate(uint64_t address, DataType type, const std::vector<uint8_t>& value) {
    std::vector<uint8_t> message;
    
    // Header
    BinaryHeader header;
    header.opcode = BinaryOpcode::VALUE_UPDATE;
    header.flags = 0;
    header.payloadSize = sizeof(ValueUpdateNotification) + static_cast<uint32_t>(value.size());
    header.requestId = 0; // Not a request
    
    message.resize(sizeof(BinaryHeader) + sizeof(ValueUpdateNotification) + value.size());
    memcpy(message.data(), &header, sizeof(BinaryHeader));
    
    // Payload
    ValueUpdateNotification notification;
    notification.address = address;
    notification.type = type;
    notification.valueSize = static_cast<uint32_t>(value.size());
    
    memcpy(message.data() + sizeof(BinaryHeader), &notification, sizeof(ValueUpdateNotification));
    memcpy(message.data() + sizeof(BinaryHeader) + sizeof(ValueUpdateNotification), value.data(), value.size());
    
    return message;
}

std::vector<uint8_t> BinaryProtocol::EncodeBulkUpdate(const std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& updates) {
    std::vector<uint8_t> message;
    
    // Calculate total payload size
    uint32_t totalPayloadSize = sizeof(BulkUpdateNotification);
    for (const auto& update : updates) {
        totalPayloadSize += sizeof(ValueUpdateNotification) + static_cast<uint32_t>(update.second.size());
    }
    
    // Header
    BinaryHeader header;
    header.opcode = BinaryOpcode::BULK_UPDATE;
    header.flags = 0;
    header.payloadSize = totalPayloadSize;
    header.requestId = 0; // Not a request
    
    message.resize(sizeof(BinaryHeader) + totalPayloadSize);
    memcpy(message.data(), &header, sizeof(BinaryHeader));
    
    // Bulk notification header
    BulkUpdateNotification bulkNotification;
    bulkNotification.count = static_cast<uint32_t>(updates.size());
    
    size_t offset = sizeof(BinaryHeader);
    memcpy(message.data() + offset, &bulkNotification, sizeof(BulkUpdateNotification));
    offset += sizeof(BulkUpdateNotification);
    
    // Individual updates
    for (const auto& update : updates) {
        ValueUpdateNotification notification;
        notification.address = update.first;
        notification.type = DataType::BYTES; // Default to bytes for flexibility
        notification.valueSize = static_cast<uint32_t>(update.second.size());
        
        memcpy(message.data() + offset, &notification, sizeof(ValueUpdateNotification));
        offset += sizeof(ValueUpdateNotification);
        
        memcpy(message.data() + offset, update.second.data(), update.second.size());
        offset += update.second.size();
    }
    
    return message;
}

bool BinaryProtocol::DecodeHeader(const std::vector<uint8_t>& data, BinaryHeader& header) {
    if (data.size() < sizeof(BinaryHeader)) {
        return false;
    }
    
    memcpy(&header, data.data(), sizeof(BinaryHeader));
    
    // Validate magic number
    return header.magic == 0x494E544C;
}

bool BinaryProtocol::DecodeMemoryRead(const std::vector<uint8_t>& payload, MemoryReadRequest& request) {
    if (payload.size() < sizeof(MemoryReadRequest)) {
        return false;
    }
    
    memcpy(&request, payload.data(), sizeof(MemoryReadRequest));
    return true;
}

bool BinaryProtocol::DecodeMemoryWrite(const std::vector<uint8_t>& payload, MemoryWriteRequest& request, std::vector<uint8_t>& data) {
    if (payload.size() < sizeof(MemoryWriteRequest)) {
        return false;
    }
    
    memcpy(&request, payload.data(), sizeof(MemoryWriteRequest));
    
    if (payload.size() < sizeof(MemoryWriteRequest) + request.dataSize) {
        return false;
    }
    
    data.resize(request.dataSize);
    memcpy(data.data(), payload.data() + sizeof(MemoryWriteRequest), request.dataSize);
    
    return true;
}

std::string BinaryProtocol::DataTypeToString(DataType type) {
    switch (type) {
        case DataType::INT32: return "int32";
        case DataType::INT64: return "int64";
        case DataType::FLOAT: return "float";
        case DataType::DOUBLE: return "double";
        case DataType::STRING: return "string";
        case DataType::BYTES: return "bytes";
        default: return "unknown";
    }
}

DataType BinaryProtocol::StringToDataType(const std::string& typeStr) {
    if (typeStr == "int32") return DataType::INT32;
    if (typeStr == "int64") return DataType::INT64;
    if (typeStr == "float") return DataType::FLOAT;
    if (typeStr == "double") return DataType::DOUBLE;
    if (typeStr == "string") return DataType::STRING;
    if (typeStr == "bytes") return DataType::BYTES;
    return DataType::BYTES; // Default fallback
}

uint64_t BinaryProtocol::StringToAddress(const std::string& addressStr) {
    return std::stoull(addressStr, nullptr, 16);
}

std::string BinaryProtocol::AddressToString(uint64_t address) {
    char buffer[32];
    sprintf_s(buffer, "0x%016llX", address);
    return std::string(buffer);
}

void BinaryProtocol::WriteUInt32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void BinaryProtocol::WriteUInt64(std::vector<uint8_t>& buffer, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

uint32_t BinaryProtocol::ReadUInt32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t BinaryProtocol::ReadUInt64(const uint8_t* data) {
    uint64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    return result;
}

} // namespace InternalEngine