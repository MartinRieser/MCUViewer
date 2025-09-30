/**
 * @file StlinkDebugProbe.hpp
 * @brief STMicroelectronics STLink debug probe implementation.
 *
 * This class provides an implementation of the IDebugProbe interface for STLink
 * debug probes (V2, V2-1, V3). STLink probes are commonly used with STM32 microcontrollers
 * and provide SWD/JTAG debugging capabilities.
 *
 * @note STLink probes only support NORMAL mode (no HSS support)
 * @note Requires libusb and stlink library dependencies
 */

#ifndef _StlinkDebugProbe_HPP
#define _StlinkDebugProbe_HPP

#include <mutex>
#include <string>
#include <vector>

#include "IDebugProbe.hpp"
#include "stlink.h"

#ifdef __APPLE__
extern "C" {
#include "read_write.h"
#include "usb.h"
}
#endif

#include "spdlog/spdlog.h"

/**
 * @class StlinkDebugProbe
 * @brief Concrete implementation for STLink debug probes.
 *
 * Provides full debug probe functionality for STLink devices including:
 * - Device enumeration and connection
 * - Memory read/write operations via SWD/JTAG
 * - Target MCU detection and identification
 * - Error handling and diagnostics
 *
 * Usage example:
 * @code
 * auto logger = spdlog::get("logger");
 * auto probe = std::make_shared<StlinkDebugProbe>(logger);
 *
 * // Get connected devices
 * auto devices = probe->getConnectedDevices();
 * if (!devices.empty()) {
 *     IDebugProbe::DebugProbeSettings settings;
 *     settings.debugProbe = 0; // STLink
 *     settings.speedkHz = 4000; // 4 MHz SWD speed
 *     settings.mode = IDebugProbe::Mode::NORMAL;
 *
 *     std::vector<std::pair<uint32_t, uint8_t>> vars = {{0x20000000, 4}};
 *     if (probe->startAcqusition(settings, vars, 100)) {
 *         uint8_t data[4];
 *         probe->readMemory(0x20000000, data, 4);
 *     }
 * }
 * @endcode
 */
class StlinkDebugProbe : public IDebugProbe
{
   public:
	/**
	 * @brief Construct a new STLink Debug Probe object.
	 *
	 * Initializes the probe interface but does not connect to hardware yet.
	 * Call startAcqusition() to establish connection.
	 *
	 * @param logger Pointer to spdlog logger instance for diagnostic output
	 */
	StlinkDebugProbe(spdlog::logger* logger);

	/**
	 * @brief Start data acquisition from target MCU via STLink probe.
	 *
	 * Connects to the specified STLink device, establishes SWD/JTAG connection with target,
	 * and prepares for memory read operations. Only NORMAL mode is supported.
	 *
	 * @param probeSettings Configuration including serial number, speed, and device
	 * @param addressSizeVector Vector of (address, size) pairs for variables to track
	 * @param samplingFreqency Requested sampling frequency in Hz (informational only for STLink)
	 * @return true if connection established successfully, false on failure
	 *
	 * @note HSS mode is not supported by STLink - will use NORMAL mode regardless
	 * @note Target must be powered on and accessible via SWD/JTAG
	 */
	bool startAcqusition(const DebugProbeSettings& probeSettings, std::vector<std::pair<uint32_t, uint8_t>>& addressSizeVector, uint32_t samplingFreqency) override;

	/**
	 * @brief Stop acquisition and disconnect from probe.
	 *
	 * Releases the STLink device and terminates the debug session. Target MCU
	 * continues running independently.
	 *
	 * @return true if disconnected successfully, false on error
	 */
	bool stopAcqusition() override;

	/**
	 * @brief Check if probe connection is valid.
	 *
	 * @return true if STLink is connected and operational
	 */
	bool isValid() const override;

	/**
	 * @brief Get target MCU name (not implemented for STLink).
	 *
	 * @return Empty string (STLink library doesn't provide reliable device names)
	 */
	std::string getTargetName() override { return std::string(); }

	/**
	 * @brief Read single entry from HSS buffer (not supported).
	 *
	 * STLink does not support HSS (High Speed Sampling) mode. This method
	 * always returns nullopt.
	 *
	 * @return std::nullopt always, as HSS is not available on STLink
	 */
	std::optional<IDebugProbe::varEntryType> readSingleEntry() override;

	/**
	 * @brief Read memory from target MCU.
	 *
	 * Performs a direct memory read via SWD/JTAG. Can read from any accessible
	 * memory region (RAM, Flash, peripherals) while target is running or halted.
	 *
	 * @param address 32-bit memory address to read from
	 * @param buf Pointer to buffer for storing read data - must be at least 'size' bytes
	 * @param size Number of bytes to read (1-4096 recommended for optimal performance)
	 * @return true if read succeeded, false on communication error or invalid address
	 *
	 * @note For reliable reading of rapidly changing variables, consider halting target
	 * @note Reading from invalid addresses may cause target to fault
	 */
	bool readMemory(uint32_t address, uint8_t* buf, uint32_t size) override;

	/**
	 * @brief Write memory to target MCU.
	 *
	 * Performs a direct memory write via SWD/JTAG. Can write to RAM and peripheral
	 * registers. Flash writes require special procedures (not handled by this method).
	 *
	 * @param address 32-bit memory address to write to
	 * @param buf Pointer to data buffer to write from
	 * @param size Number of bytes to write (1-4096 recommended)
	 * @return true if write succeeded, false on communication error or write-protected address
	 *
	 * @note Target should typically be halted for safe memory modification
	 * @note Flash/ROM regions are write-protected and require erase/unlock sequences
	 */
	bool writeMemory(uint32_t address, uint8_t* buf, uint32_t size) override;

	/**
	 * @brief Get the most recent error message.
	 *
	 * @return String describing the last error that occurred during probe operations
	 */
	std::string getLastErrorMsg() const override;

	/**
	 * @brief Get list of connected STLink devices.
	 *
	 * Scans USB bus for all connected STLink programmers (V2, V2-1, V3).
	 *
	 * @return Vector of device descriptions (typically serial numbers or device paths)
	 */
	std::vector<std::string> getConnectedDevices() override;

   private:
	stlink_t* sl = nullptr;    /**< Pointer to stlink device handle from libstlink */
	spdlog::logger* logger;    /**< Logger instance for diagnostic messages */
};

#endif