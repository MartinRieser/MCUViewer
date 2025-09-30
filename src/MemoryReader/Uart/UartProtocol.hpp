#ifndef _UARTPROTOCOL_HPP
#define _UARTPROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>

/**
 * @file UartProtocol.hpp
 * @brief UART debug interface protocol definitions and utilities
 *
 * This file defines the complete UART protocol specification for MCUViewer,
 * including packet structures, command codes, payload definitions, and CRC16-CCITT
 * checksum calculation.
 *
 * Packet Structure:
 * ┌─────────┬──────────┬──────────┬─────────┬─────────┬─────────┐
 * │ START   │ CMD      │ LENGTH   │ SEQ     │ PAYLOAD │ CRC16   │
 * │ (0xAA)  │ (1 byte) │ (2 bytes)│ (1 byte)│ (N bytes)│ (2 bytes)│
 * └─────────┴──────────┴──────────┴─────────┴─────────┴─────────┘
 *
 * Total packet size: 7 + payload_length bytes
 */

namespace UartProtocol
{

// ===== Constants =====

static constexpr uint8_t START_BYTE = 0xAA;
static constexpr size_t MAX_PAYLOAD_SIZE = 255;
static constexpr size_t MIN_PACKET_SIZE = 7;          // Header + CRC, no payload
static constexpr size_t MAX_PACKET_SIZE = 262;        // Header + max payload + CRC
static constexpr size_t HEADER_SIZE = 5;              // START + CMD + LENGTH + SEQ
static constexpr size_t CRC_SIZE = 2;

// ===== Command Codes =====

enum class CommandCode : uint8_t
{
    // Basic Memory Operations
    READ_MEMORY = 0x01,
    READ_MEMORY_RESP = 0x02,
    WRITE_MEMORY = 0x03,
    WRITE_MEMORY_RESP = 0x04,

    // Device Information
    GET_INFO = 0x05,
    GET_INFO_RESP = 0x06,
    PING = 0x07,
    PONG = 0x08,

    // Recorder Commands
    SETUP_RECORDER = 0x09,
    SETUP_RECORDER_RESP = 0x0A,
    START_RECORDER = 0x0B,
    START_RECORDER_RESP = 0x0C,
    STOP_RECORDER = 0x0D,
    STOP_RECORDER_RESP = 0x0E,
    GET_BUFFER_DATA = 0x0F,
    GET_BUFFER_DATA_RESP = 0x10,
    GET_RECORDER_STATUS = 0x11,
    GET_RECORDER_STATUS_RESP = 0x12,

    // Trigger Commands
    SETUP_TRIGGER = 0x13,
    SETUP_TRIGGER_RESP = 0x14,
    ARM_TRIGGER = 0x15,
    ARM_TRIGGER_RESP = 0x16,
    DISARM_TRIGGER = 0x17,
    DISARM_TRIGGER_RESP = 0x18,
    FORCE_TRIGGER = 0x19,
    TRIGGER_EVENT = 0x1A,

    // Error
    ERROR = 0xFF
};

// ===== Error Codes =====

enum class ErrorCode : uint8_t
{
    SUCCESS = 0x00,
    INVALID_COMMAND = 0x01,
    INVALID_PAYLOAD_LENGTH = 0x02,
    CRC_ERROR = 0x03,
    TIMEOUT = 0x04,
    MEMORY_ACCESS_ERROR = 0x05,
    NOT_SUPPORTED = 0x06,
    INVALID_PARAMETER = 0x07,
    DEVICE_BUSY = 0x08,
    NOT_READY = 0x09,
    BUFFER_OVERFLOW = 0x0A
};

// ===== Trigger Types and Conditions =====

enum class TriggerType : uint8_t
{
    EDGE = 0,      // Rising/falling edge
    LEVEL = 1,     // Above/below threshold
    WINDOW = 2,    // Enter/exit range
    LOGIC = 3,     // Boolean combination
    PATTERN = 4,   // Sequence of events
    EXTERNAL = 5   // GPIO or software trigger
};

enum class EdgeCondition : uint8_t
{
    RISING = 0,
    FALLING = 1,
    BOTH = 2
};

enum class LevelCondition : uint8_t
{
    ABOVE = 0,
    BELOW = 1
};

enum class WindowCondition : uint8_t
{
    ENTER = 0,
    EXIT = 1
};

enum class TriggerMode : uint8_t
{
    SINGLE_SHOT = 0,
    NORMAL = 1,
    AUTO = 2,
    CONTINUOUS = 3
};

// ===== Recorder States =====

enum class RecorderState : uint8_t
{
    IDLE = 0,
    ARMED = 1,
    TRIGGERED = 2,
    READY = 3
};

// ===== Capability Flags =====

struct Capabilities
{
    static constexpr uint32_t HARDWARE_RECORDER = (1 << 0);
    static constexpr uint32_t TRIGGER_SUPPORT = (1 << 1);
};

// ===== Packet Structure =====

struct Packet
{
    uint8_t start;                       // Always START_BYTE (0xAA)
    CommandCode cmd;                     // Command code
    uint16_t length;                     // Payload length (little-endian)
    uint8_t seq;                         // Sequence number
    std::vector<uint8_t> payload;        // Command-specific payload
    uint16_t crc;                        // CRC16-CCITT (little-endian)

    Packet()
        : start(START_BYTE), cmd(CommandCode::ERROR), length(0), seq(0), crc(0)
    {
    }
};

// ===== Payload Structures =====

// READ_MEMORY payload (6 bytes)
struct ReadMemoryPayload
{
    uint32_t address;    // Memory address (little-endian)
    uint16_t size;       // Number of bytes to read (1-255)
};

// READ_MEMORY_RESP payload (variable)
struct ReadMemoryRespPayload
{
    uint8_t status;                 // ErrorCode
    std::vector<uint8_t> data;      // Memory data (only if status == SUCCESS)
};

// WRITE_MEMORY payload (variable)
struct WriteMemoryPayload
{
    uint32_t address;               // Memory address (little-endian)
    uint16_t size;                  // Number of bytes to write
    std::vector<uint8_t> data;      // Data to write
};

// WRITE_MEMORY_RESP payload (1 byte)
struct WriteMemoryRespPayload
{
    uint8_t status;    // ErrorCode
};

// GET_INFO_RESP payload (variable)
struct GetInfoRespPayload
{
    uint8_t status;              // ErrorCode
    uint16_t protocolVersion;    // Protocol version (e.g., 0x0100 for v1.0)
    uint32_t capabilities;       // Capability flags bitmap
    std::string deviceName;      // Device name (null-terminated string)
};

// PING payload (4 bytes)
struct PingPayload
{
    uint32_t timestamp;    // Timestamp from host
};

// PONG payload (4 bytes)
struct PongPayload
{
    uint32_t timestamp;    // Echo of timestamp from PING
};

// SETUP_RECORDER payload (variable)
struct SetupRecorderPayload
{
    uint32_t bufferSize;           // Buffer size in samples
    uint16_t sampleRateHz;         // Sampling rate in Hz
    uint8_t preTriggerPercent;     // Pre-trigger % (0-100)
    uint8_t numVars;               // Number of variables to record

    struct Variable
    {
        uint32_t address;    // Variable address
        uint8_t size;        // Variable size in bytes
    };
    std::vector<Variable> variables;    // Variable definitions
};

// SETUP_RECORDER_RESP payload (1 byte)
struct SetupRecorderRespPayload
{
    uint8_t status;    // ErrorCode
};

// GET_BUFFER_DATA payload (8 bytes)
struct GetBufferDataPayload
{
    uint32_t offset;    // Offset in buffer (sample index)
    uint32_t count;     // Number of samples to retrieve
};

// GET_BUFFER_DATA_RESP payload (variable)
struct GetBufferDataRespPayload
{
    uint8_t status;                 // ErrorCode
    uint32_t numSamples;            // Number of samples in response
    std::vector<uint8_t> data;      // Sample data (packed binary)
};

// GET_RECORDER_STATUS_RESP payload (9 bytes)
struct GetRecorderStatusRespPayload
{
    uint8_t status;           // RecorderState
    uint32_t totalSamples;    // Total samples captured
    uint32_t triggerIndex;    // Index where trigger fired
};

// SETUP_TRIGGER payload (variable)
struct SetupTriggerPayload
{
    uint8_t triggerId;        // Trigger ID (0-255)
    TriggerType type;         // Trigger type
    uint32_t varAddress;      // Variable address to monitor
    uint8_t condition;        // Type-specific condition
    float value1;             // Threshold value
    float value2;             // Second value (for window trigger)
    uint8_t flags;            // Flags bitmap (bit 0: enable pre-trigger)
};

// SETUP_TRIGGER_RESP payload (1 byte)
struct SetupTriggerRespPayload
{
    uint8_t status;    // ErrorCode
};

// ARM_TRIGGER payload (2 bytes)
struct ArmTriggerPayload
{
    uint8_t triggerId;    // Trigger ID to arm
    TriggerMode mode;     // Trigger mode
};

// ARM_TRIGGER_RESP payload (1 byte)
struct ArmTriggerRespPayload
{
    uint8_t status;    // ErrorCode
};

// DISARM_TRIGGER payload (1 byte)
struct DisarmTriggerPayload
{
    uint8_t triggerId;    // Trigger ID to disarm
};

// DISARM_TRIGGER_RESP payload (1 byte)
struct DisarmTriggerRespPayload
{
    uint8_t status;    // ErrorCode
};

// FORCE_TRIGGER payload (1 byte)
struct ForceTriggerPayload
{
    uint8_t triggerId;    // Trigger ID to force
};

// TRIGGER_EVENT payload (8 bytes)
struct TriggerEventPayload
{
    uint8_t triggerId;     // Trigger ID that fired
    uint32_t timestamp;    // Timestamp when trigger fired
    float value;           // Value at trigger point
};

// ERROR payload (variable)
struct ErrorPayload
{
    uint8_t errorCode;        // ErrorCode
    std::string message;      // Error message (null-terminated, optional)
};

// ===== CRC16-CCITT Calculation =====

/**
 * @brief Calculate CRC16-CCITT checksum
 *
 * Polynomial: 0x1021
 * Initial value: 0xFFFF
 * Final XOR: 0x0000
 * Bit order: MSB first
 *
 * @param data Pointer to data buffer
 * @param length Length of data in bytes
 * @return CRC16 checksum (16-bit value)
 */
inline uint16_t calculateCRC16(const uint8_t* data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

// ===== Packet Serialization/Deserialization =====

/**
 * @brief Serialize a packet into a byte vector
 *
 * Converts a Packet structure into a byte stream ready for transmission.
 * Automatically calculates and sets the CRC16 field.
 *
 * @param packet Packet to serialize
 * @return Serialized byte vector
 */
std::vector<uint8_t> serialize(const Packet& packet);

/**
 * @brief Deserialize a byte vector into a packet
 *
 * Parses a byte stream into a Packet structure and validates the CRC16.
 *
 * @param data Byte vector to deserialize
 * @param packet Output packet structure
 * @return true if deserialization and CRC validation successful, false otherwise
 */
bool deserialize(const std::vector<uint8_t>& data, Packet& packet);

/**
 * @brief Create a READ_MEMORY packet
 *
 * @param address Memory address to read
 * @param size Number of bytes to read (1-255)
 * @param seq Sequence number
 * @return Serialized packet ready for transmission
 */
std::vector<uint8_t> createReadMemoryPacket(uint32_t address, uint16_t size, uint8_t seq);

/**
 * @brief Create a WRITE_MEMORY packet
 *
 * @param address Memory address to write
 * @param data Data to write
 * @param seq Sequence number
 * @return Serialized packet ready for transmission
 */
std::vector<uint8_t> createWriteMemoryPacket(uint32_t address, const std::vector<uint8_t>& data, uint8_t seq);

/**
 * @brief Create a GET_INFO packet
 *
 * @param seq Sequence number
 * @return Serialized packet ready for transmission
 */
std::vector<uint8_t> createGetInfoPacket(uint8_t seq);

/**
 * @brief Create a PING packet
 *
 * @param timestamp Timestamp value
 * @param seq Sequence number
 * @return Serialized packet ready for transmission
 */
std::vector<uint8_t> createPingPacket(uint32_t timestamp, uint8_t seq);

/**
 * @brief Parse a READ_MEMORY_RESP payload
 *
 * @param payload Payload bytes
 * @param outPayload Parsed payload structure
 * @return true on success, false on parse error
 */
bool parseReadMemoryResp(const std::vector<uint8_t>& payload, ReadMemoryRespPayload& outPayload);

/**
 * @brief Parse a GET_INFO_RESP payload
 *
 * @param payload Payload bytes
 * @param outPayload Parsed payload structure
 * @return true on success, false on parse error
 */
bool parseGetInfoResp(const std::vector<uint8_t>& payload, GetInfoRespPayload& outPayload);

/**
 * @brief Get human-readable name for command code
 *
 * @param cmd Command code
 * @return String name of command
 */
std::string getCommandName(CommandCode cmd);

/**
 * @brief Get human-readable name for error code
 *
 * @param error Error code
 * @return String name of error
 */
std::string getErrorName(ErrorCode error);

}    // namespace UartProtocol

#endif    // _UARTPROTOCOL_HPP
