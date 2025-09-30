#include "UartProtocol.hpp"

#include <cstring>

namespace UartProtocol
{

// ===== Helper Functions for Little-Endian Conversion =====

static inline void writeUint16LE(std::vector<uint8_t>& vec, uint16_t value)
{
    vec.push_back(static_cast<uint8_t>(value & 0xFF));
    vec.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

static inline void writeUint32LE(std::vector<uint8_t>& vec, uint32_t value)
{
    vec.push_back(static_cast<uint8_t>(value & 0xFF));
    vec.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    vec.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    vec.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

static inline void writeFloatLE(std::vector<uint8_t>& vec, float value)
{
    uint32_t temp;
    std::memcpy(&temp, &value, sizeof(float));
    writeUint32LE(vec, temp);
}

static inline uint16_t readUint16LE(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

static inline uint32_t readUint32LE(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

static inline float readFloatLE(const uint8_t* data)
{
    uint32_t temp = readUint32LE(data);
    float value;
    std::memcpy(&value, &temp, sizeof(float));
    return value;
}

// ===== Packet Serialization =====

std::vector<uint8_t> serialize(const Packet& packet)
{
    std::vector<uint8_t> result;
    result.reserve(HEADER_SIZE + packet.payload.size() + CRC_SIZE);

    // Header
    result.push_back(packet.start);
    result.push_back(static_cast<uint8_t>(packet.cmd));
    writeUint16LE(result, packet.length);
    result.push_back(packet.seq);

    // Payload
    result.insert(result.end(), packet.payload.begin(), packet.payload.end());

    // Calculate CRC over START, CMD, LENGTH, SEQ, and PAYLOAD
    uint16_t crc = calculateCRC16(result.data(), result.size());

    // Append CRC
    writeUint16LE(result, crc);

    return result;
}

// ===== Packet Deserialization =====

bool deserialize(const std::vector<uint8_t>& data, Packet& packet)
{
    // Check minimum packet size
    if (data.size() < MIN_PACKET_SIZE)
        return false;

    // Check START byte
    if (data[0] != START_BYTE)
        return false;

    // Parse header
    packet.start = data[0];
    packet.cmd = static_cast<CommandCode>(data[1]);
    packet.length = readUint16LE(&data[2]);
    packet.seq = data[4];

    // Check payload length matches packet size
    if (data.size() != HEADER_SIZE + packet.length + CRC_SIZE)
        return false;

    // Extract payload
    packet.payload.clear();
    if (packet.length > 0)
    {
        packet.payload.resize(packet.length);
        std::memcpy(packet.payload.data(), &data[HEADER_SIZE], packet.length);
    }

    // Extract CRC
    packet.crc = readUint16LE(&data[HEADER_SIZE + packet.length]);

    // Validate CRC (calculate over START, CMD, LENGTH, SEQ, PAYLOAD)
    uint16_t calculatedCrc = calculateCRC16(data.data(), HEADER_SIZE + packet.length);
    if (calculatedCrc != packet.crc)
        return false;

    return true;
}

// ===== Packet Creation Functions =====

std::vector<uint8_t> createReadMemoryPacket(uint32_t address, uint16_t size, uint8_t seq)
{
    Packet packet;
    packet.cmd = CommandCode::READ_MEMORY;
    packet.seq = seq;

    // Build payload
    writeUint32LE(packet.payload, address);
    writeUint16LE(packet.payload, size);

    packet.length = static_cast<uint16_t>(packet.payload.size());

    return serialize(packet);
}

std::vector<uint8_t> createWriteMemoryPacket(uint32_t address, const std::vector<uint8_t>& data, uint8_t seq)
{
    Packet packet;
    packet.cmd = CommandCode::WRITE_MEMORY;
    packet.seq = seq;

    // Build payload
    writeUint32LE(packet.payload, address);
    writeUint16LE(packet.payload, static_cast<uint16_t>(data.size()));
    packet.payload.insert(packet.payload.end(), data.begin(), data.end());

    packet.length = static_cast<uint16_t>(packet.payload.size());

    return serialize(packet);
}

std::vector<uint8_t> createGetInfoPacket(uint8_t seq)
{
    Packet packet;
    packet.cmd = CommandCode::GET_INFO;
    packet.seq = seq;
    packet.length = 0;    // No payload

    return serialize(packet);
}

std::vector<uint8_t> createPingPacket(uint32_t timestamp, uint8_t seq)
{
    Packet packet;
    packet.cmd = CommandCode::PING;
    packet.seq = seq;

    // Build payload
    writeUint32LE(packet.payload, timestamp);

    packet.length = static_cast<uint16_t>(packet.payload.size());

    return serialize(packet);
}

// ===== Payload Parsing Functions =====

bool parseReadMemoryResp(const std::vector<uint8_t>& payload, ReadMemoryRespPayload& outPayload)
{
    if (payload.empty())
        return false;

    outPayload.status = payload[0];

    // If status is success, extract data
    if (outPayload.status == static_cast<uint8_t>(ErrorCode::SUCCESS))
    {
        if (payload.size() > 1)
        {
            outPayload.data.resize(payload.size() - 1);
            std::memcpy(outPayload.data.data(), &payload[1], payload.size() - 1);
        }
    }

    return true;
}

bool parseGetInfoResp(const std::vector<uint8_t>& payload, GetInfoRespPayload& outPayload)
{
    if (payload.size() < 7)    // Minimum: status + version + capabilities
        return false;

    outPayload.status = payload[0];

    if (outPayload.status != static_cast<uint8_t>(ErrorCode::SUCCESS))
        return true;    // No more data on error

    outPayload.protocolVersion = readUint16LE(&payload[1]);
    outPayload.capabilities = readUint32LE(&payload[3]);

    // Extract device name (null-terminated string)
    if (payload.size() > 7)
    {
        const char* nameStart = reinterpret_cast<const char*>(&payload[7]);
        size_t nameLength = payload.size() - 7;

        // Find null terminator
        for (size_t i = 0; i < nameLength; i++)
        {
            if (nameStart[i] == '\0')
            {
                nameLength = i;
                break;
            }
        }

        outPayload.deviceName = std::string(nameStart, nameLength);
    }

    return true;
}

// ===== String Conversion Functions =====

std::string getCommandName(CommandCode cmd)
{
    switch (cmd)
    {
        case CommandCode::READ_MEMORY:
            return "READ_MEMORY";
        case CommandCode::READ_MEMORY_RESP:
            return "READ_MEMORY_RESP";
        case CommandCode::WRITE_MEMORY:
            return "WRITE_MEMORY";
        case CommandCode::WRITE_MEMORY_RESP:
            return "WRITE_MEMORY_RESP";
        case CommandCode::GET_INFO:
            return "GET_INFO";
        case CommandCode::GET_INFO_RESP:
            return "GET_INFO_RESP";
        case CommandCode::PING:
            return "PING";
        case CommandCode::PONG:
            return "PONG";
        case CommandCode::SETUP_RECORDER:
            return "SETUP_RECORDER";
        case CommandCode::SETUP_RECORDER_RESP:
            return "SETUP_RECORDER_RESP";
        case CommandCode::START_RECORDER:
            return "START_RECORDER";
        case CommandCode::START_RECORDER_RESP:
            return "START_RECORDER_RESP";
        case CommandCode::STOP_RECORDER:
            return "STOP_RECORDER";
        case CommandCode::STOP_RECORDER_RESP:
            return "STOP_RECORDER_RESP";
        case CommandCode::GET_BUFFER_DATA:
            return "GET_BUFFER_DATA";
        case CommandCode::GET_BUFFER_DATA_RESP:
            return "GET_BUFFER_DATA_RESP";
        case CommandCode::GET_RECORDER_STATUS:
            return "GET_RECORDER_STATUS";
        case CommandCode::GET_RECORDER_STATUS_RESP:
            return "GET_RECORDER_STATUS_RESP";
        case CommandCode::SETUP_TRIGGER:
            return "SETUP_TRIGGER";
        case CommandCode::SETUP_TRIGGER_RESP:
            return "SETUP_TRIGGER_RESP";
        case CommandCode::ARM_TRIGGER:
            return "ARM_TRIGGER";
        case CommandCode::ARM_TRIGGER_RESP:
            return "ARM_TRIGGER_RESP";
        case CommandCode::DISARM_TRIGGER:
            return "DISARM_TRIGGER";
        case CommandCode::DISARM_TRIGGER_RESP:
            return "DISARM_TRIGGER_RESP";
        case CommandCode::FORCE_TRIGGER:
            return "FORCE_TRIGGER";
        case CommandCode::TRIGGER_EVENT:
            return "TRIGGER_EVENT";
        case CommandCode::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

std::string getErrorName(ErrorCode error)
{
    switch (error)
    {
        case ErrorCode::SUCCESS:
            return "SUCCESS";
        case ErrorCode::INVALID_COMMAND:
            return "INVALID_COMMAND";
        case ErrorCode::INVALID_PAYLOAD_LENGTH:
            return "INVALID_PAYLOAD_LENGTH";
        case ErrorCode::CRC_ERROR:
            return "CRC_ERROR";
        case ErrorCode::TIMEOUT:
            return "TIMEOUT";
        case ErrorCode::MEMORY_ACCESS_ERROR:
            return "MEMORY_ACCESS_ERROR";
        case ErrorCode::NOT_SUPPORTED:
            return "NOT_SUPPORTED";
        case ErrorCode::INVALID_PARAMETER:
            return "INVALID_PARAMETER";
        case ErrorCode::DEVICE_BUSY:
            return "DEVICE_BUSY";
        case ErrorCode::NOT_READY:
            return "NOT_READY";
        case ErrorCode::BUFFER_OVERFLOW:
            return "BUFFER_OVERFLOW";
        default:
            return "UNKNOWN_ERROR";
    }
}

}    // namespace UartProtocol