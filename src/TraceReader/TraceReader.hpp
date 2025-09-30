/**
 * @file TraceReader.hpp
 * @brief High-level SWO trace data decoder and manager.
 *
 * TraceReader coordinates trace probe hardware, decodes raw ITM packets into
 * channel data with timestamps, and provides thread-safe buffered access to
 * trace streams. It handles the complete ITM protocol state machine including
 * synchronization, timestamps, and multi-byte payloads.
 */

#ifndef _ITRACEREADER_HPP
#define _ITRACEREADER_HPP

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ITraceProbe.hpp"
#include "RingBufferBlocking.hpp"
#include "spdlog/spdlog.h"

/**
 * @class TraceReader
 * @brief SWO trace packet decoder and data manager.
 *
 * Manages the complete trace capture pipeline:
 * - Coordinates trace probe hardware (STLink/JLink)
 * - Runs background thread to continuously read trace data
 * - Decodes ITM packet protocol (source packets, timestamps, sync)
 * - Buffers decoded data in thread-safe ring buffer
 * - Provides error detection and diagnostics
 *
 * ITM Protocol Overview:
 * - Source packets: 1-4 byte payloads on channels 0-31
 * - Local timestamps: Relative time markers
 * - Global timestamps: Absolute time synchronization
 * - Protocol error detection
 *
 * Usage example:
 * @code
 * TraceReader reader(logger);
 * ITraceProbe::TraceProbeSettings settings;
 * std::array<bool, 32> channels = {};
 * channels[0] = true; // Enable channel 0
 *
 * reader.setCoreClockFrequency(168000000);
 * reader.setTraceFrequency(16800000);
 *
 * if (reader.startAcqusition(settings, channels)) {
 *     double timestamp;
 *     std::array<uint32_t, 10> data;
 *     while (reader.readTrace(timestamp, data)) {
 *         // Process trace data
 *     }
 * }
 * @endcode
 */
class TraceReader
{
   public:
	struct TraceIndicators
	{
		uint32_t framesTotal;
		uint32_t errorFramesTotal;
		uint32_t errorFramesInView;
		uint32_t delayedTimestamp1;
		uint32_t delayedTimestamp2;
		uint32_t delayedTimestamp3;
		uint32_t delayedTimestamp3InView;
		uint32_t sleepCycles;
	};

	TraceReader(spdlog::logger* logger);

	bool startAcqusition(const ITraceProbe::TraceProbeSettings& probeSettings, const std::array<bool, 32>& activeChannels);
	bool stopAcqusition();
	bool isValid() const;

	bool readTrace(double& timestamp, std::array<uint32_t, 10>& trace);

	std::string getLastErrorMsg() const;

	void setCoreClockFrequency(uint32_t frequencyHz);
	uint32_t getCoreClockFrequency() const;
	void setTraceFrequency(uint32_t frequencyHz);
	uint32_t getTraceFrequency() const;
	void setTraceShouldReset(bool shouldReset);
	void setTraceTimeout(uint32_t timeout);

	std::vector<std::string> getConnectedDevices() const;
	void changeDevice(std::shared_ptr<ITraceProbe> newTraceProbe);
	std::string getTargetName();

	TraceIndicators getTraceIndicators() const;

   private:
	typedef enum
	{
		TRACE_STATE_UNKNOWN,
		TRACE_STATE_IDLE,
		TRACE_STATE_TARGET_SOURCE_1B,
		TRACE_STATE_TARGET_SOURCE_2B,
		TRACE_STATE_TARGET_SOURCE_3B,
		TRACE_STATE_TARGET_SOURCE_4B,
		TRACE_STATE_TARGET_TIMESTAMP_HEADER,
		TRACE_STATE_TARGET_TIMESTAMP_CONT,
		TRACE_STATE_TARGET_TIMESTAMP_END,
		TRACE_STATE_SKIP_FRAME,
	} TraceState;

	TraceState state = TRACE_STATE_IDLE;
	TraceIndicators traceIndicators{};

	static constexpr uint32_t channels = 10;
	static constexpr uint32_t size = 10 * 2048;
	uint8_t buffer[size]{};

	uint32_t currentValue[channels]{};
	uint8_t currentChannel[channels]{};

	uint8_t awaitingTimestamp = 0;
	uint32_t timestamp = 0;
	uint8_t sourceFrameSize = 0;

	std::vector<uint8_t> timestampVec;

	uint32_t coreFrequency = 160000;
	uint32_t tracePrescaler = 10;
	uint32_t traceTimeout = 2;
	bool shouldReset = false;

	std::atomic<bool> isRunning{false};
	std::string lastErrorMsg = "";
	std::array<uint32_t, channels> previousEntry{};
	RingBufferBlocking<std::pair<std::array<uint32_t, channels>, double>, 2000> traceTable;
	std::thread readerHandle;

	std::shared_ptr<ITraceProbe> TraceProbe;
	spdlog::logger* logger;
	mutable std::mutex mtx;

	TraceState updateTraceIdle(uint8_t c);
	TraceState updateTrace(uint8_t c);
	void timestampEnd(bool headerData);
	void readerThread();
};

#endif