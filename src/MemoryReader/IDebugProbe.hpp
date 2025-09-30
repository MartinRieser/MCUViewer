/**
 * @file IDebugProbe.hpp
 * @brief Interface for debug probe implementations (STLink, JLink) that enable variable reading from microcontrollers.
 *
 * This interface provides a unified API for reading variables from MCU memory via debug probes using
 * the SWDIO/SWCLK interface. It supports two operational modes:
 * - NORMAL mode: Direct memory read/write operations
 * - HSS (High Speed Sampling) mode: Optimized for continuous variable sampling at high frequencies
 */

#ifndef _IVARIABLEREADER_HPP
#define _IVARIABLEREADER_HPP

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "RingBuffer.hpp"

/**
 * @class IDebugProbe
 * @brief Abstract interface for debug probe devices.
 *
 * This interface defines the contract for debug probe implementations that communicate with
 * microcontrollers via debug interfaces (SWD/JTAG). Concrete implementations include STLink
 * and JLink debug probes.
 *
 * Usage example:
 * @code
 * std::shared_ptr<IDebugProbe> probe = std::make_shared<StlinkDebugProbe>(logger);
 * IDebugProbe::DebugProbeSettings settings;
 * settings.mode = IDebugProbe::Mode::NORMAL;
 * settings.speedkHz = 10000;
 *
 * std::vector<std::pair<uint32_t, uint8_t>> variables = {{0x20000000, 4}};
 * if (probe->startAcqusition(settings, variables, 100)) {
 *     uint8_t buffer[4];
 *     probe->readMemory(0x20000000, buffer, 4);
 * }
 * @endcode
 */
class IDebugProbe
{
   public:
	/**
	 * @enum Mode
	 * @brief Operational modes for the debug probe.
	 */
	enum Mode
	{
		NORMAL = 0,  /**< Standard mode - direct memory read/write operations with explicit addressing */
		HSS = 1,     /**< High Speed Sampling mode - optimized for continuous variable monitoring (JLink only) */
	};

	/**
	 * @struct DebugProbeSettings
	 * @brief Configuration settings for debug probe connection and operation.
	 */
	typedef struct
	{
		uint32_t debugProbe = 0;              /**< Debug probe type identifier (0=STLink, 1=JLink) */
		std::string serialNumber = "";        /**< Serial number to identify specific probe when multiple are connected */
		std::string device = "";              /**< Target MCU device name/identifier */
		Mode mode = Mode::NORMAL;             /**< Operational mode selection */
		uint32_t speedkHz = 10000;            /**< SWD/JTAG communication speed in kHz (typical: 4000-50000) */

	} DebugProbeSettings;

	/**
	 * @typedef varEntryType
	 * @brief Data structure for HSS mode variable entries.
	 *
	 * Contains a timestamp and a map of address-value pairs captured at that moment.
	 * First element: timestamp in seconds
	 * Second element: map of memory addresses to their double-precision values
	 */
	using varEntryType = std::pair<double, std::unordered_map<uint32_t, double>>;

	virtual ~IDebugProbe() = default;

	/**
	 * @brief Start data acquisition from the target MCU.
	 *
	 * Initializes the debug probe connection and begins monitoring specified memory addresses.
	 * In NORMAL mode, sets up the connection for on-demand memory reads.
	 * In HSS mode (JLink only), configures continuous sampling of variables.
	 *
	 * @param probeSettings Configuration including probe type, speed, and mode
	 * @param addressSizeVector Vector of (address, size) pairs specifying which variables to monitor
	 * @param samplingFreqency Desired sampling rate in Hz (relevant for HSS mode)
	 * @return true if acquisition started successfully, false on error
	 *
	 * @note Call getLastErrorMsg() on failure to retrieve error details
	 * @note addressSizeVector must remain valid for the duration of acquisition
	 */
	virtual bool startAcqusition(const DebugProbeSettings& probeSettings, std::vector<std::pair<uint32_t, uint8_t>>& addressSizeVector, uint32_t samplingFreqency) = 0;

	/**
	 * @brief Stop data acquisition and release probe resources.
	 *
	 * Safely terminates the connection to the debug probe and stops all sampling operations.
	 *
	 * @return true if acquisition stopped successfully, false on error
	 */
	virtual bool stopAcqusition() = 0;

	/**
	 * @brief Check if the probe connection is valid and operational.
	 *
	 * @return true if probe is connected and ready for operations
	 */
	virtual bool isValid() const = 0;

	/**
	 * @brief Get the target MCU name/identifier.
	 *
	 * @return String containing the target device name (e.g., "STM32F407", "NXP K64F")
	 */
	virtual std::string getTargetName() = 0;

	/**
	 * @brief Read a single entry from the HSS sampling buffer.
	 *
	 * This method is only available in HSS mode (JLink). It retrieves one timestamped
	 * set of variable values that were captured by the high-speed sampling system.
	 *
	 * @return Optional containing timestamp and address-value map if data available, nullopt otherwise
	 *
	 * @note Only functional in HSS mode - returns nullopt in NORMAL mode
	 * @note Non-blocking - returns immediately even if no data is available
	 */
	virtual std::optional<varEntryType> readSingleEntry() = 0;

	/**
	 * @brief Read a block of memory from the target MCU (NORMAL mode).
	 *
	 * Performs a direct memory read operation at the specified address. This is the
	 * standard method for reading variables in NORMAL mode.
	 *
	 * @param address Memory address to read from (e.g., 0x20000000 for RAM)
	 * @param buf Buffer to store the read data - must be at least 'size' bytes
	 * @param size Number of bytes to read
	 * @return true if read succeeded, false on error
	 *
	 * @note Target must be halted or in debug mode for reliable reads
	 * @note For HSS mode, use readSingleEntry() instead
	 */
	virtual bool readMemory(uint32_t address, uint8_t* buf, uint32_t size) = 0;

	/**
	 * @brief Write a block of memory to the target MCU (NORMAL mode).
	 *
	 * Performs a direct memory write operation at the specified address. Useful for
	 * modifying variables or configuration during debugging.
	 *
	 * @param address Memory address to write to (e.g., 0x20000000 for RAM)
	 * @param buf Buffer containing data to write
	 * @param size Number of bytes to write
	 * @return true if write succeeded, false on error
	 *
	 * @note Target must be halted or in debug mode for safe writes
	 * @note Writing to flash/ROM regions may require special unlock sequences
	 */
	virtual bool writeMemory(uint32_t address, uint8_t* buf, uint32_t size) = 0;

	/**
	 * @brief Get the last error message from probe operations.
	 *
	 * @return String containing human-readable error description
	 */
	virtual std::string getLastErrorMsg() const = 0;

	/**
	 * @brief Get list of all connected debug probes.
	 *
	 * Scans USB buses for connected debug probes of this type (STLink or JLink).
	 *
	 * @return Vector of strings describing connected probes (e.g., serial numbers, device IDs)
	 */
	virtual std::vector<std::string> getConnectedDevices() = 0;

   protected:
	std::atomic<bool> isRunning = false;  /**< Atomic flag indicating if acquisition is active */
	std::string lastErrorMsg = "";        /**< Last error message for diagnostic purposes */
	mutable std::mutex mtx;               /**< Mutex for thread-safe access to probe resources */
};

#endif