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

   private:
	stlink_t* sl = nullptr;
	spdlog::logger* logger;
};

#endif