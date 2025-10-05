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

class StlinkDebugProbe : public IDebugProbe
{
   public:
	struct ProbeInfo
	{
		uint32_t stlinkVersion = 0;  // 2 or 3
		uint32_t jtagVersion = 0;
		uint32_t maxSwdSpeedKHz = 4000;  // Default to V2 max (4 MHz)
		uint32_t maxTraceFreqHz = 2000000;  // Default to V2 max (2 MHz)
		std::string versionString = "";
	};

	StlinkDebugProbe(spdlog::logger* logger);
	bool startAcqusition(const DebugProbeSettings& probeSettings, std::vector<std::pair<uint32_t, uint8_t>>& addressSizeVector, uint32_t samplingFreqency) override;
	bool stopAcqusition() override;
	bool isValid() const override;
	std::string getTargetName() override { return std::string(); }

	std::optional<IDebugProbe::varEntryType> readSingleEntry() override;
	bool readMemory(uint32_t address, uint8_t* buf, uint32_t size) override;
	bool writeMemory(uint32_t address, uint8_t* buf, uint32_t size) override;

	std::string getLastErrorMsg() const override;
	std::vector<std::string> getConnectedDevices() override;

	ProbeInfo getProbeInfo() const;

   private:
	void detectProbeCapabilities();

	stlink_t* sl = nullptr;
	spdlog::logger* logger;
	ProbeInfo probeInfo;
};

#endif