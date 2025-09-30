/**
 * @file JlinkDebugProbe.hpp
 * @brief SEGGER J-Link debug probe implementation with HSS support.
 *
 * This class provides an implementation of the IDebugProbe interface for SEGGER J-Link
 * debug probes. J-Link supports both standard memory read/write (NORMAL mode) and
 * High-Speed Sampling (HSS mode) for efficient continuous variable monitoring.
 *
 * @note Supports both NORMAL and HSS modes
 * @note HSS mode can sample up to 100 variables at very high frequencies (kHz range)
 * @note Requires SEGGER J-Link SDK/DLL
 */

#ifndef _JlinkDebugProbe_HPP
#define _JlinkDebugProbe_HPP

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "IDebugProbe.hpp"
#include "jlink.h"
#include "spdlog/spdlog.h"

/**
 * @class JlinkDebugProbe
 * @brief Concrete implementation for SEGGER J-Link debug probes.
 *
 * Provides full debug probe functionality including:
 * - Device enumeration and connection
 * - Memory read/write operations via SWD/JTAG (NORMAL mode)
 * - High-Speed Sampling for continuous variable monitoring (HSS mode)
 * - Target MCU detection with device name retrieval
 * - Efficient buffered data acquisition with ring buffer
 *
 * HSS Mode Benefits:
 * - Samples variables without halting the target MCU
 * - Minimal target performance impact
 * - Efficient timestamped data capture
 * - Can track up to 100 variables simultaneously
 *
 * Usage example:
 * @code
 * auto logger = spdlog::get("logger");
 * auto probe = std::make_shared<JlinkDebugProbe>(logger);
 *
 * IDebugProbe::DebugProbeSettings settings;
 * settings.debugProbe = 1; // J-Link
 * settings.speedkHz = 10000;
 * settings.mode = IDebugProbe::Mode::HSS; // Enable HSS mode
 *
 * std::vector<std::pair<uint32_t, uint8_t>> vars = {
 *     {0x20000000, 4}, // Track 4-byte variable
 *     {0x20000010, 2}  // Track 2-byte variable
 * };
 *
 * if (probe->startAcqusition(settings, vars, 1000)) { // 1 kHz sampling
 *     while (auto entry = probe->readSingleEntry()) {
 *         double timestamp = entry->first;
 *         auto& values = entry->second;
 *         // Process timestamped variable values
 *     }
 * }
 * @endcode
 */
class JlinkDebugProbe : public IDebugProbe
{
   public:
	/**
	 * @brief Construct a new JLink Debug Probe object.
	 *
	 * Initializes the J-Link interface. Call startAcqusition() to connect.
	 *
	 * @param logger Pointer to spdlog logger instance for diagnostic output
	 */
	JlinkDebugProbe(spdlog::logger* logger);

	/**
	 * @brief Start data acquisition from target MCU via J-Link probe.
	 *
	 * Connects to the J-Link device and initializes either NORMAL or HSS mode.
	 * In HSS mode, configures the J-Link to continuously sample specified variables
	 * and buffer results for efficient retrieval.
	 *
	 * @param probeSettings Configuration including serial number, speed, mode, and device
	 * @param addressSizeVector Vector of (address, size) pairs for variables to monitor (max 100)
	 * @param samplingFreqency Sampling rate in Hz (relevant for HSS mode, typ. 100-10000 Hz)
	 * @return true if acquisition started successfully, false on error
	 *
	 * @note HSS mode requires J-Link with HSS support (most modern J-Links)
	 * @note Maximum 100 variables can be tracked in HSS mode
	 * @note samplingFreqency affects CPU load - higher rates need faster SWD speeds
	 */
	bool startAcqusition(const DebugProbeSettings& probeSettings, std::vector<std::pair<uint32_t, uint8_t>>& addressSizeVector, uint32_t samplingFreqency) override;

	/**
	 * @brief Stop acquisition and disconnect from J-Link.
	 *
	 * Stops HSS sampling (if active), releases the J-Link device, and cleans up resources.
	 *
	 * @return true if stopped successfully, false on error
	 */
	bool stopAcqusition() override;

	/**
	 * @brief Check if J-Link connection is valid.
	 *
	 * @return true if J-Link is connected and operational
	 */
	bool isValid() const override;

	/**
	 * @brief Get target MCU name from J-Link.
	 *
	 * Queries the connected target device and returns its identifier.
	 *
	 * @return String containing target device name (e.g., "STM32F407VG", "NXP_K64F")
	 */
	std::string getTargetName() override;

	/**
	 * @brief Read a single entry from the HSS buffer (HSS mode only).
	 *
	 * Retrieves one timestamped sample containing all tracked variable values.
	 * This is the primary method for reading data in HSS mode. Data is buffered
	 * in a ring buffer, so call frequently to avoid losing samples.
	 *
	 * @return Optional containing timestamp and address-value map if data available, nullopt if buffer empty
	 *
	 * @note Only works in HSS mode - returns nullopt in NORMAL mode
	 * @note Non-blocking - returns immediately
	 * @note Ring buffer holds up to 2000 entries - call frequently to prevent overflow
	 */
	std::optional<IDebugProbe::varEntryType> readSingleEntry() override;

	/**
	 * @brief Read memory from target MCU (NORMAL mode).
	 *
	 * Performs direct memory read via SWD/JTAG. Works in NORMAL mode or when
	 * HSS is not active.
	 *
	 * @param address 32-bit memory address to read from
	 * @param buf Buffer to store read data - must be at least 'size' bytes
	 * @param size Number of bytes to read
	 * @return true if read succeeded, false on error
	 *
	 * @note J-Link can read while target is running (non-intrusive)
	 * @note For high-frequency reading, consider using HSS mode instead
	 */
	bool readMemory(uint32_t address, uint8_t* buf, uint32_t size) override;

	/**
	 * @brief Write memory to target MCU (NORMAL mode).
	 *
	 * Performs direct memory write via SWD/JTAG.
	 *
	 * @param address 32-bit memory address to write to
	 * @param buf Buffer containing data to write
	 * @param size Number of bytes to write
	 * @return true if write succeeded, false on error
	 *
	 * @note Target should be halted for safe memory modification
	 * @note Flash writes require special procedures (not handled here)
	 */
	bool writeMemory(uint32_t address, uint8_t* buf, uint32_t size) override;

	/**
	 * @brief Get the most recent error message.
	 *
	 * @return String describing the last error from J-Link operations
	 */
	std::string getLastErrorMsg() const override;

	/**
	 * @brief Get list of connected J-Link devices.
	 *
	 * Scans USB and network for available J-Link probes.
	 *
	 * @return Vector of device descriptions (typically serial numbers)
	 */
	std::vector<std::string> getConnectedDevices() override;

   private:
	static constexpr size_t maxDevices = 10;            /**< Maximum number of J-Link devices to enumerate */
	static constexpr size_t maxVariables = 100;         /**< Maximum variables that can be tracked in HSS mode */
	static constexpr size_t fifoSize = 2000;            /**< Ring buffer size for HSS data entries */
	static constexpr uint32_t maxSpeedkHz = 50000;      /**< Maximum SWD/JTAG speed (50 MHz) */
	static constexpr double timestampResolution = 1e-6; /**< HSS timestamp resolution (microseconds) */
	static constexpr uint32_t bufferSizeHSS = 16384;    /**< Internal buffer size for HSS data from J-Link */

	JLINK_HSS_MEM_BLOCK_DESC variableDesc[maxVariables]{}; /**< HSS memory block descriptors for tracked variables */
	size_t trackedVarsCount = 0;                            /**< Number of variables currently being tracked */
	size_t trackedVarsTotalSize = 0;                        /**< Total size in bytes of all tracked variables */

	size_t emptyMessageErrorThreshold = 100000;             /**< Threshold for consecutive empty reads before error */
	size_t emptyMessageErrorCnt = 0;                        /**< Counter for consecutive empty HSS reads */

	std::unordered_map<uint32_t, uint8_t> addressSizeMap;  /**< Map of variable addresses to their sizes */
	RingBuffer<varEntryType, fifoSize> varTable;            /**< Ring buffer storing HSS timestamped samples */

	spdlog::logger* logger;                                 /**< Logger instance for diagnostic messages */
};

#endif