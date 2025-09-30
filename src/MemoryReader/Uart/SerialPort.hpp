/**
 * @file SerialPort.hpp
 * @brief Cross-platform serial port communication class
 *
 * Provides a unified interface for serial port operations across Linux, Windows, and macOS.
 * Supports port enumeration, configuration, and read/write with timeouts.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace UartProtocol
{

/**
 * @brief Serial port information structure
 */
struct SerialPortInfo
{
    std::string port;           ///< Port name (e.g., "/dev/ttyUSB0", "COM3")
    std::string description;    ///< Port description (if available)
    std::string manufacturer;   ///< Manufacturer name (if available)
};

/**
 * @brief Serial port baud rate
 */
enum class BaudRate : uint32_t
{
    BAUD_9600 = 9600,
    BAUD_19200 = 19200,
    BAUD_38400 = 38400,
    BAUD_57600 = 57600,
    BAUD_115200 = 115200,
    BAUD_230400 = 230400,
    BAUD_460800 = 460800,
    BAUD_921600 = 921600
};

/**
 * @brief Serial port parity setting
 */
enum class Parity
{
    NONE,
    ODD,
    EVEN,
    MARK,
    SPACE
};

/**
 * @brief Serial port stop bits
 */
enum class StopBits
{
    ONE,
    ONE_POINT_FIVE,
    TWO
};

/**
 * @brief Serial port flow control
 */
enum class FlowControl
{
    NONE,
    HARDWARE,
    SOFTWARE
};

/**
 * @brief Cross-platform serial port class
 *
 * Provides serial port communication with support for:
 * - Port enumeration
 * - Configuration (baud rate, parity, stop bits, flow control)
 * - Read/write with timeouts
 * - Platform-specific implementations for Linux, Windows, and macOS
 */
class SerialPort
{
  public:
    /**
     * @brief Default constructor
     */
    SerialPort();

    /**
     * @brief Destructor - automatically closes port if open
     */
    ~SerialPort();

    // Prevent copying
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    // Allow moving
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    /**
     * @brief List all available serial ports
     *
     * @return Vector of available serial ports with their information
     *
     * @note On Linux, scans /dev/ttyUSB*, /dev/ttyACM*, /dev/ttyS*
     * @note On Windows, uses registry to enumerate COM ports
     * @note On macOS, scans /dev/cu.* and /dev/tty.*
     */
    static std::vector<SerialPortInfo> listPorts();

    /**
     * @brief Open serial port with specified configuration
     *
     * @param portName Port name (e.g., "/dev/ttyUSB0", "COM3")
     * @param baudRate Baud rate (default: 115200)
     * @param dataBits Number of data bits (default: 8)
     * @param parity Parity setting (default: NONE)
     * @param stopBits Stop bits (default: ONE)
     * @param flowControl Flow control (default: NONE)
     *
     * @return true if port opened successfully, false otherwise
     *
     * @note Previous port connection will be closed if open
     */
    bool open(const std::string& portName,
              BaudRate baudRate = BaudRate::BAUD_115200,
              uint8_t dataBits = 8,
              Parity parity = Parity::NONE,
              StopBits stopBits = StopBits::ONE,
              FlowControl flowControl = FlowControl::NONE);

    /**
     * @brief Close serial port
     *
     * @note Safe to call even if port is not open
     */
    void close();

    /**
     * @brief Check if port is open
     *
     * @return true if port is open, false otherwise
     */
    bool isOpen() const;

    /**
     * @brief Read data from serial port with timeout
     *
     * @param buffer Buffer to store received data
     * @param size Maximum number of bytes to read
     * @param timeoutMs Timeout in milliseconds (0 = non-blocking, -1 = blocking)
     *
     * @return Number of bytes read (0 if timeout or error)
     *
     * @note Returns immediately if any data is available (up to size bytes)
     * @note Returns 0 if timeout expires with no data
     */
    size_t read(uint8_t* buffer, size_t size, int32_t timeoutMs = 1000);

    /**
     * @brief Write data to serial port
     *
     * @param data Data to write
     * @param size Number of bytes to write
     *
     * @return Number of bytes written (may be less than size on error)
     */
    size_t write(const uint8_t* data, size_t size);

    /**
     * @brief Flush input buffer (discard unread data)
     *
     * @return true on success, false on error
     */
    bool flushInput();

    /**
     * @brief Flush output buffer (wait for all data to be transmitted)
     *
     * @return true on success, false on error
     */
    bool flushOutput();

    /**
     * @brief Get number of bytes available to read
     *
     * @return Number of bytes in input buffer, or 0 on error
     */
    size_t available() const;

    /**
     * @brief Get last error message
     *
     * @return Error message string (empty if no error)
     */
    std::string getLastError() const;

    /**
     * @brief Get port name
     *
     * @return Port name (empty if not open)
     */
    std::string getPortName() const;

  private:
    /**
     * @brief Platform-specific port handle
     */
#ifdef _WIN32
    void* m_handle;    ///< HANDLE on Windows
#else
    int m_handle;    ///< File descriptor on Linux/macOS
#endif

    std::string m_portName;      ///< Current port name
    std::string m_lastError;     ///< Last error message
    bool m_isOpen;               ///< Port open status

    /**
     * @brief Set error message
     *
     * @param error Error message
     */
    void setError(const std::string& error);

    /**
     * @brief Platform-specific configuration
     *
     * @return true on success, false on error
     */
    bool configure(BaudRate baudRate, uint8_t dataBits, Parity parity, StopBits stopBits, FlowControl flowControl);
};

}    // namespace UartProtocol
