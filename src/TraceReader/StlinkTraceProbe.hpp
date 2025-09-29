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

class StlinkTraceProbe : public ITraceProbe
{
   public:
	explicit StlinkTraceProbe(spdlog::logger* logger);
	bool startTrace(const TraceProbeSettings& probeSettings, uint32_t coreFrequency, uint32_t tracePrescaler, uint32_t activeChannelMask, bool shouldReset) override;
	bool stopTrace() override;
	int32_t readTraceBuffer(uint8_t* buffer, uint32_t size) override;

	std::string getTargetName() override { return std::string(); }
	std::vector<std::string> getConnectedDevices() override;

   private:
	stlink_t* sl = nullptr;
	spdlog::logger* logger;
};
#endif