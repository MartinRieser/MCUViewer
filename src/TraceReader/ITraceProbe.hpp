/**
 * @file ITraceProbe.hpp
 * @brief Interface for SWO trace probe implementations.
 *
 * This interface provides a unified API for capturing SWO (Serial Wire Output) trace data
 * from ARM Cortex-M microcontrollers. SWO is a single-pin trace output that can stream
 * debug messages, printf output, and instrumentation data without halting the MCU.
 *
 * SWO supports up to 32 ITM (Instrumentation Trace Macrocell) channels for different
 * data streams, making it ideal for real-time debugging and performance analysis.
 */

#ifndef _ITRACEPROBE_HPP
#define _ITRACEPROBE_HPP

#include <stdint.h>

#include <vector>

/**
 * @class ITraceProbe
 * @brief Abstract interface for SWO trace probe devices.
 *
 * This interface defines the contract for trace probe implementations that capture
 * SWO trace data from ARM Cortex-M microcontrollers. Concrete implementations include
 * STLink and JLink trace probes.
 *
 * SWO Trace Overview:
 * - Single-wire output from MCU's SWO pin
 * - Supports up to 32 independent ITM channels
 * - Non-intrusive - minimal impact on target execution
 * - Requires 3-4 wire connection: SWDIO/SWCLK/SWO/GND
 * - Typical use cases: printf debugging, event logging, performance monitoring
 *
 * Usage example:
 * @code
 * std::shared_ptr<ITraceProbe> traceProbe = std::make_shared<StlinkTraceProbe>(logger);
 *
 * ITraceProbe::TraceProbeSettings settings;
 * settings.debugProbe = 0; // STLink
 * settings.speedkHz = 4000;
 *
 * uint32_t coreFreq = 168000000; // 168 MHz core
 * uint32_t prescaler = 10;       // SWO baudrate = coreFreq / prescaler
 * uint32_t channelMask = 0x01;   // Enable channel 0
 *
 * if (traceProbe->startTrace(settings, coreFreq, prescaler, channelMask, true)) {
 *     uint8_t buffer[1024];
 *     int32_t bytesRead = traceProbe->readTraceBuffer(buffer, sizeof(buffer));
 *     // Process trace data
 * }
 * @endcode
 */
class ITraceProbe
{
   public:
	/**
	 * @struct TraceProbeSettings
	 * @brief Configuration settings for trace probe connection.
	 */
	typedef struct
	{
		uint32_t debugProbe = 0;       /**< Debug probe type (0=STLink, 1=JLink) */
		std::string serialNumber = ""; /**< Serial number for specific probe selection */
		std::string device = "";       /**< Target device name/identifier */
		uint32_t speedkHz = 10000;     /**< SWD communication speed in kHz */

	} TraceProbeSettings;

	virtual ~ITraceProbe() = default;

	/**
	 * @brief Start SWO trace capture from target MCU.
	 *
	 * Initializes the debug probe, configures the target's SWO output, and begins
	 * capturing trace data. The SWO baudrate is calculated as coreFrequency / tracePrescaler.
	 *
	 * @param probeSettings Probe configuration (type, serial number, SWD speed)
	 * @param coreFrequency Target MCU core frequency in Hz (e.g., 168000000 for 168 MHz)
	 *                      This must match the actual CPU frequency for proper trace decoding
	 * @param tracePrescaler SWO prescaler value (typically 10-100). SWO baudrate = coreFrequency/tracePrescaler
	 *                       Lower values = higher trace bandwidth but require faster SWD speeds
	 * @param activeChannelMask Bitmask of ITM channels to enable (bit 0 = channel 0, etc.)
	 *                          Use 0xFFFFFFFF to enable all 32 channels, or specific bits for selective capture
	 * @param shouldReset If true, reset the target MCU before starting trace capture
	 *                    Ensures clean start state but will restart target application
	 * @return true if trace started successfully, false on error
	 *
	 * @note Target firmware must configure ITM for trace output (typically via CMSIS macros)
	 * @note Incorrect coreFrequency will result in garbled trace data
	 * @note SWO pin must be physically connected to probe (4-wire setup)
	 */
	virtual bool startTrace(const TraceProbeSettings& probeSettings, uint32_t coreFrequency, uint32_t tracePrescaler, uint32_t activeChannelMask, bool shouldReset) = 0;

	/**
	 * @brief Stop trace capture and release probe resources.
	 *
	 * Stops SWO trace collection and disconnects from the debug probe.
	 * Target MCU continues running independently.
	 *
	 * @return true if stopped successfully, false on error
	 */
	virtual bool stopTrace() = 0;

	/**
	 * @brief Read captured trace data from the probe's buffer.
	 *
	 * Retrieves raw SWO trace bytes from the probe's internal buffer. This data
	 * contains encoded ITM packets that must be parsed to extract channel data,
	 * timestamps, and payloads. Returns immediately with available data (non-blocking).
	 *
	 * @param buffer Pointer to buffer for storing trace data - must be at least 'size' bytes
	 * @param size Maximum number of bytes to read into buffer (typically 1024-16384)
	 * @return Number of bytes actually read (0 if no data available, negative on error)
	 *
	 * @note Call frequently to prevent probe buffer overflow and data loss
	 * @note Returns raw ITM packet stream - use TraceReader for decoding
	 * @note Buffer should be large enough to handle burst traffic (>=1024 bytes recommended)
	 */
	virtual int32_t readTraceBuffer(uint8_t* buffer, uint32_t size) = 0;

	/**
	 * @brief Get target MCU name from probe.
	 *
	 * @return String containing target device identifier
	 */
	virtual std::string getTargetName() = 0;

	/**
	 * @brief Get list of connected trace-capable probes.
	 *
	 * Scans for connected debug probes that support SWO trace capture.
	 *
	 * @return Vector of device descriptions (serial numbers or device IDs)
	 */
	virtual std::vector<std::string> getConnectedDevices() = 0;
};

#endif