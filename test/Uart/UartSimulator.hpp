/**
 * @file UartSimulator.hpp
 * @brief UART protocol simulator for testing without real hardware
 *
 * Simulates a target microcontroller that responds to UART protocol commands.
 * Creates a virtual serial port pair for testing MCUViewer UART functionality.
 */

#pragma once

#include "SerialPort.hpp"
#include "UartProtocol.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace UartProtocol
{

/**
 * @brief UART protocol simulator
 *
 * Simulates a microcontroller target that responds to UART protocol commands.
 * Provides a virtual memory map for testing READ/WRITE operations.
 *
 * Usage:
 * @code
 * UartSimulator sim;
 * if (sim.start("/tmp/vserial")) {
 *     // Connect MCUViewer to the virtual port
 *     // Simulator responds to all protocol commands
 *     sim.stop();
 * }
 * @endcode
 */
class UartSimulator
{
  public:
    /**
     * @brief Constructor
     *
     * @param memorySize Size of simulated memory in bytes (default: 64KB)
     */
    explicit UartSimulator(size_t memorySize = 65536);

    /**
     * @brief Destructor - automatically stops simulator
     */
    ~UartSimulator();

    // Prevent copying
    UartSimulator(const UartSimulator&) = delete;
    UartSimulator& operator=(const UartSimulator&) = delete;

    /**
     * @brief Start simulator on specified port
     *
     * @param portName Serial port to use (e.g., "/dev/ttyUSB0", "COM3")
     *                 On Linux/macOS, can use pty for virtual port
     * @param baudRate Baud rate (default: 115200)
     *
     * @return true if started successfully, false otherwise
     *
     * @note Starts background thread that processes incoming commands
     * @note Call stop() or destroy object to terminate
     */
    bool start(const std::string& portName, BaudRate baudRate = BaudRate::BAUD_115200);

    /**
     * @brief Stop simulator
     *
     * @note Stops background thread and closes serial port
     * @note Safe to call multiple times
     */
    void stop();

    /**
     * @brief Check if simulator is running
     *
     * @return true if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Get port name
     *
     * @return Port name (empty if not running)
     */
    std::string getPortName() const;

    /**
     * @brief Write data to simulated memory
     *
     * @param address Memory address
     * @param data Data to write
     * @param size Number of bytes to write
     *
     * @return true if successful, false if address out of range
     *
     * @note Use this to pre-populate memory before testing
     */
    bool writeMemory(uint32_t address, const uint8_t* data, size_t size);

    /**
     * @brief Read data from simulated memory
     *
     * @param address Memory address
     * @param data Buffer to store read data
     * @param size Number of bytes to read
     *
     * @return true if successful, false if address out of range
     */
    bool readMemory(uint32_t address, uint8_t* data, size_t size) const;

    /**
     * @brief Set device information
     *
     * @param name Device name (max 32 chars)
     * @param version Protocol version (default: 0x0100 = v1.0)
     * @param capabilities Capabilities bitmask (default: 0x03 = read+write)
     */
    void setDeviceInfo(const std::string& name, uint16_t version = 0x0100, uint32_t capabilities = 0x03);

    /**
     * @brief Get statistics
     */
    struct Statistics
    {
        uint64_t packetsReceived;
        uint64_t packetsSent;
        uint64_t crcErrors;
        uint64_t invalidCommands;
        uint64_t memoryErrors;
    };

    Statistics getStatistics() const;

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics();

    /**
     * @brief Enable/disable verbose logging
     *
     * @param enable true to enable, false to disable
     */
    void setVerbose(bool enable);

  private:
    /**
     * @brief Background thread function
     */
    void simulatorThread();

    /**
     * @brief Process received packet
     *
     * @param packet Received packet
     */
    void processPacket(const Packet& packet);

    /**
     * @brief Handle READ_MEMORY command
     */
    void handleReadMemory(const Packet& packet);

    /**
     * @brief Handle WRITE_MEMORY command
     */
    void handleWriteMemory(const Packet& packet);

    /**
     * @brief Handle GET_INFO command
     */
    void handleGetInfo(const Packet& packet);

    /**
     * @brief Handle PING command
     */
    void handlePing(const Packet& packet);

    /**
     * @brief Send error response
     */
    void sendError(uint8_t seq, ErrorCode error);

    /**
     * @brief Send packet
     */
    bool sendPacket(const Packet& packet);

    // Serial port
    std::unique_ptr<SerialPort> m_port;
    std::string m_portName;

    // Simulated memory
    std::vector<uint8_t> m_memory;
    size_t m_memorySize;

    // Device information
    std::string m_deviceName;
    uint16_t m_protocolVersion;
    uint32_t m_capabilities;

    // Background thread
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_running;

    // Statistics
    mutable std::atomic<uint64_t> m_packetsReceived;
    mutable std::atomic<uint64_t> m_packetsSent;
    mutable std::atomic<uint64_t> m_crcErrors;
    mutable std::atomic<uint64_t> m_invalidCommands;
    mutable std::atomic<uint64_t> m_memoryErrors;

    // Settings
    std::atomic<bool> m_verbose;

    // Receive buffer
    std::vector<uint8_t> m_receiveBuffer;
};

/**
 * @brief Helper class to create virtual serial port pair
 *
 * Uses platform-specific methods to create a virtual serial port pair:
 * - Linux/macOS: Uses pty (pseudoterminal)
 * - Windows: Uses com0com or similar virtual COM port driver
 */
class VirtualSerialPort
{
  public:
    /**
     * @brief Create virtual serial port pair
     *
     * @param masterPort Output: master port name
     * @param slavePort Output: slave port name
     *
     * @return true if successful, false otherwise
     *
     * @note On Linux/macOS, creates a pty pair
     * @note Master port is used by simulator, slave port by client
     */
    static bool createPair(std::string& masterPort, std::string& slavePort);

    /**
     * @brief Create virtual serial port using socat (Linux/macOS)
     *
     * @param port1 Output: first port name
     * @param port2 Output: second port name
     * @param baudRate Baud rate
     *
     * @return true if successful, false otherwise
     *
     * @note Requires socat to be installed
     * @note Creates /tmp/vserial0 and /tmp/vserial1 symlinks
     */
    static bool createSocatPair(std::string& port1, std::string& port2, uint32_t baudRate = 115200);
};

}    // namespace UartProtocol
