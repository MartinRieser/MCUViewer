/**
 * @file UartDebugProbe.hpp
 * @brief UART-based debug probe implementation
 *
 * Implements IDebugProbe interface for UART communication with target firmware.
 * Supports basic memory read/write operations via the UART protocol.
 */

#ifndef _UARTDEBUGPROBE_HPP
#define _UARTDEBUGPROBE_HPP

#include <memory>
#include <string>
#include <vector>

#include "IDebugProbe.hpp"
#include "SerialPort.hpp"
#include "UartProtocol.hpp"
#include "spdlog/spdlog.h"

class UartDebugProbe : public IDebugProbe
{
  public:
	/**
	 * @brief Constructor
	 *
	 * @param logger Shared logger instance for debug output
	 */
	explicit UartDebugProbe(std::shared_ptr<spdlog::logger> logger);

	/**
	 * @brief Destructor - automatically closes connection
	 */
	~UartDebugProbe() override;

	/**
	 * @brief Get list of available UART ports
	 *
	 * @return Vector of port names (e.g., "/dev/ttyUSB0", "COM3")
	 *
	 * @note Lists all available serial ports on the system
	 */
	std::vector<std::string> getConnectedDevices() override;

	/**
	 * @brief Start data acquisition session
	 *
	 * Opens the UART port and sends GET_INFO command to verify connection.
	 *
	 * @param probeSettings Settings containing UART port name and baud rate
	 * @param addressSizeVector Vector of address-size pairs (not used for UART)
	 * @param samplingFreqency Sampling frequency in Hz (not used for basic probe)
	 * @return true if connection successful, false otherwise
	 *
	 * @note probeSettings.device should contain port name (e.g., "/dev/ttyUSB0")
	 * @note probeSettings.speedkHz will be interpreted as baud rate directly
	 */
	bool startAcqusition(const DebugProbeSettings& probeSettings,
	                     std::vector<std::pair<uint32_t, uint8_t>>& addressSizeVector,
	                     uint32_t samplingFreqency) override;

	/**
	 * @brief Stop data acquisition session
	 *
	 * Closes the UART port.
	 *
	 * @return true on success
	 */
	bool stopAcqusition() override;

	/**
	 * @brief Check if probe is connected and valid
	 *
	 * @return true if UART port is open and connection is valid
	 */
	bool isValid() const override;

	/**
	 * @brief Get target device name
	 *
	 * @return Device name from GET_INFO response, or "Unknown" if not connected
	 */
	std::string getTargetName() override;

	/**
	 * @brief Read memory from target (not implemented for basic UART probe)
	 *
	 * @return empty optional
	 */
	std::optional<varEntryType> readSingleEntry() override;

	/**
	 * @brief Read memory from target
	 *
	 * Sends READ_MEMORY command and waits for response.
	 *
	 * @param address Memory address to read
	 * @param buf Buffer to store read data
	 * @param size Number of bytes to read (1-255)
	 * @return true if read successful, false on error/timeout
	 *
	 * @note Maximum read size is 255 bytes per transaction
	 * @note Blocks until response received or timeout (1000ms)
	 */
	bool readMemory(uint32_t address, uint8_t* buf, uint32_t size) override;

	/**
	 * @brief Write memory to target
	 *
	 * Sends WRITE_MEMORY command and waits for response.
	 *
	 * @param address Memory address to write
	 * @param buf Buffer containing data to write
	 * @param size Number of bytes to write (1-255)
	 * @return true if write successful, false on error/timeout
	 *
	 * @note Maximum write size is 255 bytes per transaction
	 * @note Blocks until response received or timeout (1000ms)
	 */
	bool writeMemory(uint32_t address, uint8_t* buf, uint32_t size) override;

	/**
	 * @brief Get last error message
	 *
	 * @return Error message string
	 */
	std::string getLastErrorMsg() const override;

  private:
	std::shared_ptr<spdlog::logger> logger;
	std::unique_ptr<UartProtocol::SerialPort> serialPort;

	std::string targetName;
	uint16_t protocolVersion;
	uint32_t capabilities;

	uint8_t sequenceNumber;

	static constexpr int RESPONSE_TIMEOUT_MS = 1000;
	static constexpr int RESPONSE_POLL_INTERVAL_MS = 10;

	/**
	 * @brief Get next sequence number
	 *
	 * @return Incremented sequence number (wraps around at 255)
	 */
	uint8_t getNextSequence();

	/**
	 * @brief Send a packet and wait for response
	 *
	 * @param requestPacket Packet to send
	 * @param responsePacket Received response packet
	 * @param expectedCmd Expected response command code
	 * @param timeoutMs Timeout in milliseconds
	 * @return true if response received and valid, false on error/timeout
	 */
	bool sendAndReceive(const std::vector<uint8_t>& requestPacket,
	                    UartProtocol::Packet& responsePacket,
	                    UartProtocol::CommandCode expectedCmd,
	                    int timeoutMs = RESPONSE_TIMEOUT_MS);

	/**
	 * @brief Receive a packet from serial port
	 *
	 * @param packet Output packet
	 * @param timeoutMs Timeout in milliseconds
	 * @return true if packet received and valid, false on error/timeout
	 */
	bool receivePacket(UartProtocol::Packet& packet, int timeoutMs);

	/**
	 * @brief Send GET_INFO command to retrieve device information
	 *
	 * @return true if GET_INFO successful, false on error
	 */
	bool sendGetInfo();
};

#endif    // _UARTDEBUGPROBE_HPP
