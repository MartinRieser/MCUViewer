/**
 * @file UartSimulator.cpp
 * @brief UART protocol simulator implementation
 */

#include "UartSimulator.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

// Platform-specific includes
#ifndef _WIN32
    #include <fcntl.h>
    #include <unistd.h>
    #ifdef __APPLE__
        #include <util.h>    // macOS: pty functions in util.h
    #else
        #include <pty.h>    // Linux: pty functions in pty.h
    #endif
#endif

namespace UartProtocol
{

// ===== UartSimulator =====

UartSimulator::UartSimulator(size_t memorySize)
    : m_memorySize(memorySize),
      m_deviceName("SimulatedDevice"),
      m_protocolVersion(0x0100),
      m_capabilities(0x03),    // READ_MEMORY + WRITE_MEMORY
      m_running(false),
      m_packetsReceived(0),
      m_packetsSent(0),
      m_crcErrors(0),
      m_invalidCommands(0),
      m_memoryErrors(0),
      m_verbose(false)
{
    m_memory.resize(m_memorySize, 0);
}

UartSimulator::~UartSimulator()
{
    stop();
}

bool UartSimulator::start(const std::string& portName, BaudRate baudRate)
{
    if (m_running)
    {
        std::cerr << "Simulator already running" << std::endl;
        return false;
    }

    m_port = std::make_unique<SerialPort>();
    if (!m_port->open(portName, baudRate))
    {
        std::cerr << "Failed to open port: " << m_port->getLastError() << std::endl;
        return false;
    }

    m_portName = portName;
    m_running = true;
    m_thread = std::make_unique<std::thread>(&UartSimulator::simulatorThread, this);

    if (m_verbose)
        std::cout << "[Simulator] Started on " << portName << std::endl;

    return true;
}

void UartSimulator::stop()
{
    if (!m_running)
        return;

    m_running = false;

    if (m_thread && m_thread->joinable())
        m_thread->join();

    if (m_port)
        m_port->close();

    m_thread.reset();
    m_port.reset();

    if (m_verbose)
        std::cout << "[Simulator] Stopped" << std::endl;
}

bool UartSimulator::isRunning() const
{
    return m_running;
}

std::string UartSimulator::getPortName() const
{
    return m_portName;
}

bool UartSimulator::writeMemory(uint32_t address, const uint8_t* data, size_t size)
{
    if (address + size > m_memorySize)
        return false;

    std::memcpy(&m_memory[address], data, size);
    return true;
}

bool UartSimulator::readMemory(uint32_t address, uint8_t* data, size_t size) const
{
    if (address + size > m_memorySize)
        return false;

    std::memcpy(data, &m_memory[address], size);
    return true;
}

void UartSimulator::setDeviceInfo(const std::string& name, uint16_t version, uint32_t capabilities)
{
    m_deviceName = name.substr(0, 32);    // Limit to 32 chars
    m_protocolVersion = version;
    m_capabilities = capabilities;
}

UartSimulator::Statistics UartSimulator::getStatistics() const
{
    Statistics stats;
    stats.packetsReceived = m_packetsReceived;
    stats.packetsSent = m_packetsSent;
    stats.crcErrors = m_crcErrors;
    stats.invalidCommands = m_invalidCommands;
    stats.memoryErrors = m_memoryErrors;
    return stats;
}

void UartSimulator::resetStatistics()
{
    m_packetsReceived = 0;
    m_packetsSent = 0;
    m_crcErrors = 0;
    m_invalidCommands = 0;
    m_memoryErrors = 0;
}

void UartSimulator::setVerbose(bool enable)
{
    m_verbose = enable;
}

// ===== Background Thread =====

void UartSimulator::simulatorThread()
{
    m_receiveBuffer.clear();
    m_receiveBuffer.reserve(512);

    uint8_t buffer[256];

    while (m_running)
    {
        // Read data with timeout
        size_t bytesRead = m_port->read(buffer, sizeof(buffer), 100);

        if (bytesRead > 0)
        {
            // Append to receive buffer
            m_receiveBuffer.insert(m_receiveBuffer.end(), buffer, buffer + bytesRead);

            // Try to parse packets
            while (m_receiveBuffer.size() >= MIN_PACKET_SIZE)
            {
                // Find START_BYTE
                auto startIt = std::find(m_receiveBuffer.begin(), m_receiveBuffer.end(), START_BYTE);

                if (startIt == m_receiveBuffer.end())
                {
                    // No start byte found, clear buffer
                    m_receiveBuffer.clear();
                    break;
                }

                // Remove data before start byte
                if (startIt != m_receiveBuffer.begin())
                    m_receiveBuffer.erase(m_receiveBuffer.begin(), startIt);

                // Check if we have enough data for header
                if (m_receiveBuffer.size() < HEADER_SIZE)
                    break;

                // Parse length from header
                uint16_t payloadLength = static_cast<uint16_t>(m_receiveBuffer[2]) |
                                         (static_cast<uint16_t>(m_receiveBuffer[3]) << 8);

                size_t packetSize = HEADER_SIZE + payloadLength + CRC_SIZE;

                // Check if we have complete packet
                if (m_receiveBuffer.size() < packetSize)
                    break;

                // Extract packet data
                std::vector<uint8_t> packetData(m_receiveBuffer.begin(), m_receiveBuffer.begin() + packetSize);

                // Remove packet from buffer
                m_receiveBuffer.erase(m_receiveBuffer.begin(), m_receiveBuffer.begin() + packetSize);

                // Try to deserialize packet
                Packet packet;
                if (deserialize(packetData, packet))
                {
                    m_packetsReceived++;
                    processPacket(packet);
                }
                else
                {
                    m_crcErrors++;
                    if (m_verbose)
                        std::cout << "[Simulator] CRC error" << std::endl;
                }
            }
        }

        // Prevent busy-wait
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ===== Packet Processing =====

void UartSimulator::processPacket(const Packet& packet)
{
    if (m_verbose)
    {
        std::cout << "[Simulator] RX: " << getCommandName(packet.cmd) << " (seq=" << (int)packet.seq
                  << ", len=" << packet.length << ")" << std::endl;
    }

    switch (packet.cmd)
    {
        case CommandCode::READ_MEMORY:
            handleReadMemory(packet);
            break;

        case CommandCode::WRITE_MEMORY:
            handleWriteMemory(packet);
            break;

        case CommandCode::GET_INFO:
            handleGetInfo(packet);
            break;

        case CommandCode::PING:
            handlePing(packet);
            break;

        default:
            m_invalidCommands++;
            if (m_verbose)
                std::cout << "[Simulator] Unsupported command: " << getCommandName(packet.cmd) << std::endl;
            sendError(packet.seq, ErrorCode::NOT_SUPPORTED);
            break;
    }
}

void UartSimulator::handleReadMemory(const Packet& packet)
{
    // Parse payload: address (4 bytes) + size (2 bytes)
    if (packet.length != 6)
    {
        sendError(packet.seq, ErrorCode::INVALID_PAYLOAD_LENGTH);
        return;
    }

    uint32_t address = static_cast<uint32_t>(packet.payload[0]) | (static_cast<uint32_t>(packet.payload[1]) << 8) |
                       (static_cast<uint32_t>(packet.payload[2]) << 16) |
                       (static_cast<uint32_t>(packet.payload[3]) << 24);

    uint16_t size =
        static_cast<uint16_t>(packet.payload[4]) | (static_cast<uint16_t>(packet.payload[5]) << 8);

    // Check bounds
    if (address + size > m_memorySize)
    {
        m_memoryErrors++;
        sendError(packet.seq, ErrorCode::MEMORY_ACCESS_ERROR);
        return;
    }

    // Create response
    Packet response;
    response.cmd = CommandCode::READ_MEMORY_RESP;
    response.seq = packet.seq;

    // Payload: status (1 byte) + data
    response.payload.push_back(static_cast<uint8_t>(ErrorCode::SUCCESS));
    response.payload.insert(response.payload.end(), m_memory.begin() + address, m_memory.begin() + address + size);

    response.length = static_cast<uint16_t>(response.payload.size());

    sendPacket(response);
}

void UartSimulator::handleWriteMemory(const Packet& packet)
{
    // Parse payload: address (4 bytes) + size (2 bytes) + data
    if (packet.length < 6)
    {
        sendError(packet.seq, ErrorCode::INVALID_PAYLOAD_LENGTH);
        return;
    }

    uint32_t address = static_cast<uint32_t>(packet.payload[0]) | (static_cast<uint32_t>(packet.payload[1]) << 8) |
                       (static_cast<uint32_t>(packet.payload[2]) << 16) |
                       (static_cast<uint32_t>(packet.payload[3]) << 24);

    uint16_t size =
        static_cast<uint16_t>(packet.payload[4]) | (static_cast<uint16_t>(packet.payload[5]) << 8);

    // Check payload size
    if (packet.length != 6 + size)
    {
        sendError(packet.seq, ErrorCode::INVALID_PAYLOAD_LENGTH);
        return;
    }

    // Check bounds
    if (address + size > m_memorySize)
    {
        m_memoryErrors++;
        sendError(packet.seq, ErrorCode::MEMORY_ACCESS_ERROR);
        return;
    }

    // Write to memory
    std::memcpy(&m_memory[address], &packet.payload[6], size);

    // Create response
    Packet response;
    response.cmd = CommandCode::WRITE_MEMORY_RESP;
    response.seq = packet.seq;
    response.payload.push_back(static_cast<uint8_t>(ErrorCode::SUCCESS));
    response.length = 1;

    sendPacket(response);
}

void UartSimulator::handleGetInfo(const Packet& packet)
{
    // No payload expected
    if (packet.length != 0)
    {
        sendError(packet.seq, ErrorCode::INVALID_PAYLOAD_LENGTH);
        return;
    }

    // Create response
    Packet response;
    response.cmd = CommandCode::GET_INFO_RESP;
    response.seq = packet.seq;

    // Payload: status (1 byte) + version (2 bytes) + capabilities (4 bytes) + device name (null-terminated)
    response.payload.push_back(static_cast<uint8_t>(ErrorCode::SUCCESS));

    // Protocol version (little-endian)
    response.payload.push_back(static_cast<uint8_t>(m_protocolVersion & 0xFF));
    response.payload.push_back(static_cast<uint8_t>((m_protocolVersion >> 8) & 0xFF));

    // Capabilities (little-endian)
    response.payload.push_back(static_cast<uint8_t>(m_capabilities & 0xFF));
    response.payload.push_back(static_cast<uint8_t>((m_capabilities >> 8) & 0xFF));
    response.payload.push_back(static_cast<uint8_t>((m_capabilities >> 16) & 0xFF));
    response.payload.push_back(static_cast<uint8_t>((m_capabilities >> 24) & 0xFF));

    // Device name (null-terminated)
    response.payload.insert(response.payload.end(), m_deviceName.begin(), m_deviceName.end());
    response.payload.push_back(0);    // Null terminator

    response.length = static_cast<uint16_t>(response.payload.size());

    sendPacket(response);
}

void UartSimulator::handlePing(const Packet& packet)
{
    // Payload: timestamp (4 bytes)
    if (packet.length != 4)
    {
        sendError(packet.seq, ErrorCode::INVALID_PAYLOAD_LENGTH);
        return;
    }

    // Create PONG response with same timestamp
    Packet response;
    response.cmd = CommandCode::PONG;
    response.seq = packet.seq;
    response.payload = packet.payload;    // Echo timestamp
    response.length = 4;

    sendPacket(response);
}

void UartSimulator::sendError(uint8_t seq, ErrorCode error)
{
    Packet response;
    response.cmd = CommandCode::ERROR;
    response.seq = seq;
    response.payload.push_back(static_cast<uint8_t>(error));
    response.length = 1;

    sendPacket(response);
}

bool UartSimulator::sendPacket(const Packet& packet)
{
    std::vector<uint8_t> data = serialize(packet);

    if (m_verbose)
    {
        std::cout << "[Simulator] TX: " << getCommandName(packet.cmd) << " (seq=" << (int)packet.seq
                  << ", len=" << packet.length << ")" << std::endl;
    }

    size_t bytesWritten = m_port->write(data.data(), data.size());
    if (bytesWritten == data.size())
    {
        m_packetsSent++;
        return true;
    }

    return false;
}

// ===== VirtualSerialPort =====

#ifndef _WIN32

bool VirtualSerialPort::createPair(std::string& masterPort, std::string& slavePort)
{
    int master, slave;
    char slaveName[256];

    // Open pseudoterminal
    if (openpty(&master, &slave, slaveName, nullptr, nullptr) != 0)
        return false;

    // Get master port name
    char masterName[256];
    if (ttyname_r(master, masterName, sizeof(masterName)) != 0)
    {
        close(master);
        close(slave);
        return false;
    }

    masterPort = masterName;
    slavePort = slaveName;

    // Close file descriptors (caller will reopen)
    close(master);
    close(slave);

    return true;
}

bool VirtualSerialPort::createSocatPair(std::string& port1, std::string& port2, uint32_t baudRate)
{
    // Create symlinks for virtual serial ports
    port1 = "/tmp/vserial0";
    port2 = "/tmp/vserial1";

    // Remove old symlinks
    unlink(port1.c_str());
    unlink(port2.c_str());

    // Launch socat in background
    std::string cmd = "socat -d -d pty,raw,echo=0,link=" + port1 + " pty,raw,echo=0,link=" + port2 + " &";

    int result = system(cmd.c_str());
    if (result != 0)
        return false;

    // Wait for symlinks to be created
    for (int i = 0; i < 50; i++)
    {
        if (access(port1.c_str(), F_OK) == 0 && access(port2.c_str(), F_OK) == 0)
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

#else    // Windows

bool VirtualSerialPort::createPair(std::string& masterPort, std::string& slavePort)
{
    // Windows: requires com0com or similar virtual COM port driver
    // For now, return false (manual setup required)
    return false;
}

bool VirtualSerialPort::createSocatPair(std::string& port1, std::string& port2, uint32_t baudRate)
{
    // Not supported on Windows
    return false;
}

#endif

}    // namespace UartProtocol
