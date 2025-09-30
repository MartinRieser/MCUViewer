/**
 * @file JlinkTraceProbe.hpp
 * @brief J-Link SWO trace capture implementation.
 *
 * Provides SWO trace functionality for SEGGER J-Link debug probes. Enables
 * high-performance capture of Serial Wire Output data from ARM Cortex-M
 * microcontrollers with minimal setup overhead.
 *
 * @note Requires SEGGER J-Link SDK/DLL
 */

#ifndef _JLINKTRACEPROBE_HPP
#define _JLINKTRACEPROBE_HPP

#include <memory>

#include "ITraceProbe.hpp"
#include "jlink.h"
#include "spdlog/spdlog.h"

/**
 * @class JlinkTraceProbe
 * @brief J-Link implementation for SWO trace capture.
 *
 * Handles SWO trace operations for SEGGER J-Link probes. J-Link provides
 * efficient SWO capture with automatic target device detection and
 * high-bandwidth trace streaming.
 *
 * Usage example:
 * @code
 * auto logger = spdlog::get("logger");
 * auto traceProbe = std::make_shared<JlinkTraceProbe>(logger);
 *
 * ITraceProbe::TraceProbeSettings settings;
 * settings.speedkHz = 10000;
 * settings.serialNumber = "12345678"; // Optional
 *
 * // 168 MHz core, prescaler 10 = 16.8 MHz SWO
 * if (traceProbe->startTrace(settings, 168000000, 10, 0x01, false)) {
 *     uint8_t buffer[4096];
 *     int32_t bytes = traceProbe->readTraceBuffer(buffer, sizeof(buffer));
 * }
 * @endcode
 */
class JlinkTraceProbe : public ITraceProbe
{
   public:
	/**
	 * @brief Construct J-Link trace probe interface.
	 *
	 * @param logger Pointer to spdlog logger for diagnostics
	 */
	explicit JlinkTraceProbe(spdlog::logger* logger);

	/**
	 * @brief Start SWO trace capture via J-Link.
	 *
	 * Connects to J-Link, automatically detects target, configures ITM/TPIU,
	 * and begins SWO trace collection.
	 *
	 * @param probeSettings Probe configuration (serial number optional)
	 * @param coreFrequency Target CPU frequency in Hz
	 * @param tracePrescaler SWO prescaler value
	 * @param activeChannelMask ITM channel enable mask
	 * @param shouldReset Reset target before trace start
	 * @return true if trace started successfully
	 */
	bool startTrace(const TraceProbeSettings& probeSettings, uint32_t coreFrequency, uint32_t tracePrescaler, uint32_t activeChannelMask, bool shouldReset) override;

	/**
	 * @brief Stop trace capture and release J-Link.
	 *
	 * @return true if stopped successfully
	 */
	bool stopTrace() override;

	/**
	 * @brief Read raw SWO trace data from J-Link buffer.
	 *
	 * @param buffer Buffer for trace data
	 * @param size Buffer size (recommend 4096+)
	 * @return Bytes read (0 if none, negative on error)
	 */
	int32_t readTraceBuffer(uint8_t* buffer, uint32_t size) override;

	/**
	 * @brief Get target MCU name detected by J-Link.
	 *
	 * @return Target device identifier string
	 */
	std::string getTargetName() override;

	/**
	 * @brief Get connected J-Link devices.
	 *
	 * @return Vector of J-Link device descriptions
	 */
	std::vector<std::string> getConnectedDevices() override;

   private:
	static constexpr uint32_t maxSpeedkHz = 50000;  /**< Maximum SWD speed */
	static constexpr size_t maxDevices = 10;        /**< Max devices to enumerate */
	spdlog::logger* logger;                         /**< Logger instance */
};
#endif