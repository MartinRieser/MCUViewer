/**
 * @file StlinkTraceProbe.hpp
 * @brief STLink SWO trace capture implementation.
 *
 * Provides SWO trace functionality for STLink debug probes. Enables capture
 * of Serial Wire Output data from ARM Cortex-M microcontrollers for real-time
 * debugging and instrumentation.
 *
 * @note Requires libusb and stlink library
 * @note macOS-specific compatibility layer included for Homebrew stlink
 */

#ifndef _STLINKTRACEPROBE_HPP
#define _STLINKTRACEPROBE_HPP

#include <memory>

#include "ITraceProbe.hpp"
#include "spdlog/spdlog.h"
#include "stlink.h"

#ifdef __APPLE__
extern "C" {
#include "usb.h"
#include "read_write.h"
}
// Function name compatibility for Homebrew stlink
#define stlink_trace_enable _stlink_usb_enable_trace
#define stlink_trace_disable _stlink_usb_disable_trace
#define stlink_trace_read _stlink_usb_read_trace
// stlink_write_debug32 already exists in read_write.h
#endif

/**
 * @class StlinkTraceProbe
 * @brief STLink implementation for SWO trace capture.
 *
 * Handles SWO trace operations for STLink V2, V2-1, and V3 debug probes.
 * Configures target ITM/SWO and retrieves trace data for parsing by TraceReader.
 *
 * Usage example:
 * @code
 * auto logger = spdlog::get("logger");
 * auto traceProbe = std::make_shared<StlinkTraceProbe>(logger);
 *
 * ITraceProbe::TraceProbeSettings settings;
 * settings.speedkHz = 4000;
 *
 * if (traceProbe->startTrace(settings, 168000000, 10, 0xFF, false)) {
 *     uint8_t buffer[2048];
 *     int32_t bytes = traceProbe->readTraceBuffer(buffer, sizeof(buffer));
 *     // Pass buffer to TraceReader for decoding
 * }
 * @endcode
 */
class StlinkTraceProbe : public ITraceProbe
{
   public:
	/**
	 * @brief Construct STLink trace probe interface.
	 *
	 * @param logger Pointer to spdlog logger for diagnostics
	 */
	explicit StlinkTraceProbe(spdlog::logger* logger);

	/**
	 * @brief Start SWO trace capture via STLink.
	 *
	 * Connects to STLink, configures target MCU's ITM/TPIU for SWO output,
	 * and begins trace data collection.
	 *
	 * @param probeSettings Probe configuration (serial number, SWD speed)
	 * @param coreFrequency Target CPU frequency in Hz for SWO baudrate calculation
	 * @param tracePrescaler SWO prescaler (SWO baudrate = coreFrequency / tracePrescaler)
	 * @param activeChannelMask ITM channel enable mask (32 bits, 1=enabled)
	 * @param shouldReset Reset target before starting trace
	 * @return true if trace started successfully
	 */
	bool startTrace(const TraceProbeSettings& probeSettings, uint32_t coreFrequency, uint32_t tracePrescaler, uint32_t activeChannelMask, bool shouldReset) override;

	/**
	 * @brief Stop trace capture and release STLink.
	 *
	 * @return true if stopped successfully
	 */
	bool stopTrace() override;

	/**
	 * @brief Read raw SWO trace data from STLink buffer.
	 *
	 * Retrieves accumulated trace bytes from probe's internal buffer.
	 *
	 * @param buffer Buffer for trace data storage
	 * @param size Buffer size in bytes (recommend 2048+)
	 * @return Number of bytes read (0 if none available, negative on error)
	 */
	int32_t readTraceBuffer(uint8_t* buffer, uint32_t size) override;

	/**
	 * @brief Get target name (not implemented).
	 *
	 * @return Empty string
	 */
	std::string getTargetName() override { return std::string(); }

	/**
	 * @brief Get connected STLink devices.
	 *
	 * @return Vector of STLink device descriptions
	 */
	std::vector<std::string> getConnectedDevices() override;

   private:
	stlink_t* sl = nullptr;    /**< STLink device handle */
	spdlog::logger* logger;    /**< Logger instance */
};
#endif