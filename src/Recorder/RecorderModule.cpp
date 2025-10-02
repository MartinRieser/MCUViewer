#include "RecorderModule.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

RecorderModule::RecorderModule(std::shared_ptr<RecorderBackend> backend, spdlog::logger* logger)
	: backend(backend), logger(logger)
{
	if (logger)
		logger->info("RecorderModule created");
}

RecorderModule::~RecorderModule()
{
	disarm();
	if (recorderThread && recorderThread->joinable())
		recorderThread->join();

	if (logger)
		logger->info("RecorderModule destroyed");
}

bool RecorderModule::configure(const RecorderConfig& newConfig)
{
	std::lock_guard<std::mutex> lock(dataMutex);

	if (state != RecorderState::IDLE && state != RecorderState::CONFIGURED)
	{
		lastError = "Cannot configure while armed or recording";
		if (logger)
			logger->error("RecorderModule::configure: {}", lastError);
		return false;
	}

	// Validate configuration
	if (newConfig.bufferSamples == 0)
	{
		lastError = "Buffer size must be > 0";
		if (logger)
			logger->error("RecorderModule::configure: {}", lastError);
		return false;
	}

	if (newConfig.sampleRateHz == 0)
	{
		lastError = "Sample rate must be > 0";
		if (logger)
			logger->error("RecorderModule::configure: {}", lastError);
		return false;
	}

	if (newConfig.addresses.size() != newConfig.sizes.size())
	{
		lastError = "Address and size vectors must have same length";
		if (logger)
			logger->error("RecorderModule::configure: {}", lastError);
		return false;
	}

	if (newConfig.addresses.empty())
	{
		lastError = "At least one variable must be configured";
		if (logger)
			logger->error("RecorderModule::configure: {}", lastError);
		return false;
	}

	// Store configuration
	config = newConfig;

	// Pre-allocate circular buffer
	circularBuffer.resize(config.bufferSamples);
	for (auto& sample : circularBuffer)
	{
		sample.timestamp = 0.0;
		sample.values.clear();
	}

	bufferWriteIndex = 0;
	samplesInBuffer = 0;

	state = RecorderState::CONFIGURED;

	if (logger)
		logger->info("RecorderModule configured: {} samples @ {} Hz, {} variables",
					 config.bufferSamples, config.sampleRateHz, config.addresses.size());

	return true;
}

bool RecorderModule::setupTrigger(const TriggerConfig& trigger)
{
	std::lock_guard<std::mutex> lock(dataMutex);

	if (state == RecorderState::ARMED || state == RecorderState::TRIGGERED)
	{
		lastError = "Cannot setup trigger while armed or triggered";
		if (logger)
			logger->error("RecorderModule::setupTrigger: {}", lastError);
		return false;
	}

	// Validate trigger configuration
	if (trigger.preTriggerPercent > 99)
	{
		lastError = "Pre-trigger percentage must be 0-99";
		if (logger)
			logger->error("RecorderModule::setupTrigger: {}", lastError);
		return false;
	}

	// Verify trigger variable is in configured addresses
	if (trigger.type != TriggerType::NONE)
	{
		bool found = false;
		for (const auto& addr : config.addresses)
		{
			if (addr == trigger.varAddress)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			lastError = "Trigger variable address not in configured addresses";
			if (logger)
				logger->error("RecorderModule::setupTrigger: {}", lastError);
			return false;
		}
	}

	triggerConfig = trigger;

	if (logger)
		logger->info("Trigger configured: type={}, address=0x{:08X}, value1={}",
					 static_cast<int>(trigger.type), trigger.varAddress, trigger.value1);

	return true;
}

bool RecorderModule::arm(TriggerMode mode)
{
	std::lock_guard<std::mutex> lock(dataMutex);

	if (state != RecorderState::CONFIGURED && state != RecorderState::READY)
	{
		lastError = "Recorder must be configured before arming";
		if (logger)
			logger->error("RecorderModule::arm: {}", lastError);
		return false;
	}

	// Reset state
	bufferWriteIndex = 0;
	samplesInBuffer = 0;
	capturedData.clear();
	lastValue = 0.0;
	lastState = false;
	forceTriggerFlag = false;

	// Reset statistics
	stats = RecorderStats{};

	triggerMode = mode;

	// Check if hardware recording is available and requested
	if (config.useHardwareRecording && backend->supportsHardwareRecording())
	{
		if (logger)
			logger->info("Using hardware recording mode");

		if (!backend->setupHardwareRecording(config, triggerConfig))
		{
			lastError = "Failed to setup hardware recording: " + backend->getLastError();
			if (logger)
				logger->error("RecorderModule::arm: {}", lastError);
			return false;
		}

		if (!backend->armHardwareTrigger(mode))
		{
			lastError = "Failed to arm hardware trigger: " + backend->getLastError();
			if (logger)
				logger->error("RecorderModule::arm: {}", lastError);
			return false;
		}

		state = RecorderState::ARMED;
		return true;
	}

	// Software recording mode - start thread
	if (logger)
		logger->info("Using software recording mode");

	shouldStop = false;
	state = RecorderState::ARMED;

	recorderThread = std::make_unique<std::thread>(&RecorderModule::recorderThreadFunc, this);

	if (logger)
		logger->info("Recorder armed in mode: {}", static_cast<int>(mode));

	return true;
}

void RecorderModule::disarm()
{
	if (logger)
		logger->info("Disarming recorder");

	shouldStop = true;

	if (recorderThread && recorderThread->joinable())
		recorderThread->join();

	if (state != RecorderState::ERROR)
		state = RecorderState::CONFIGURED;
}

void RecorderModule::forceTrigger()
{
	if (state == RecorderState::ARMED)
	{
		if (logger)
			logger->info("Force trigger requested");
		forceTriggerFlag = true;
	}
}

RecorderState RecorderModule::getState() const
{
	return state.load();
}

std::vector<RecorderSample> RecorderModule::getData(int32_t startIndex, uint32_t count) const
{
	std::lock_guard<std::mutex> lock(dataMutex);

	if (capturedData.empty())
		return {};

	// Calculate actual start index
	int32_t triggerIdx = static_cast<int32_t>(stats.triggerIndex);
	int32_t actualStart = triggerIdx + startIndex;

	if (actualStart < 0)
		actualStart = 0;
	if (actualStart >= static_cast<int32_t>(capturedData.size()))
		return {};

	// Calculate actual count
	uint32_t remaining = capturedData.size() - actualStart;
	uint32_t actualCount = std::min(count, remaining);

	std::vector<RecorderSample> result;
	result.reserve(actualCount);

	for (uint32_t i = 0; i < actualCount; i++)
		result.push_back(capturedData[actualStart + i]);

	return result;
}

std::vector<RecorderSample> RecorderModule::getAllData() const
{
	std::lock_guard<std::mutex> lock(dataMutex);
	return capturedData;
}

RecorderStats RecorderModule::getStats() const
{
	std::lock_guard<std::mutex> lock(dataMutex);
	return stats;
}

RecorderConfig RecorderModule::getConfig() const
{
	std::lock_guard<std::mutex> lock(dataMutex);
	return config;
}

TriggerConfig RecorderModule::getTriggerConfig() const
{
	std::lock_guard<std::mutex> lock(dataMutex);
	return triggerConfig;
}

void RecorderModule::reset()
{
	disarm();

	std::lock_guard<std::mutex> lock(dataMutex);

	state = RecorderState::IDLE;
	circularBuffer.clear();
	capturedData.clear();
	bufferWriteIndex = 0;
	samplesInBuffer = 0;
	stats = RecorderStats{};
	lastError.clear();

	if (logger)
		logger->info("Recorder reset to IDLE");
}

void RecorderModule::recorderThreadFunc()
{
	if (logger)
		logger->info("Recorder thread started");

	uint32_t sampleCount = 0;

	// Calculate sleep time between samples
	auto samplePeriod = std::chrono::microseconds(1000000 / config.sampleRateHz);

	while (!shouldStop && state != RecorderState::ERROR)
	{
		auto loopStart = std::chrono::steady_clock::now();

		// Sample and check trigger
		bool triggered = sampleAndCheckTrigger();

		if (triggered)
		{
			// Trigger fired!
			if (logger)
				logger->info("Trigger fired at sample {}", sampleCount);

			state = RecorderState::TRIGGERED;

			// Calculate how many post-trigger samples we need
			uint32_t preTriggerSamples = (config.bufferSamples * triggerConfig.preTriggerPercent) / 100;
			uint32_t postTriggerSamples = config.bufferSamples - preTriggerSamples;

			// Continue sampling for post-trigger samples
			for (uint32_t i = 0; i < postTriggerSamples && !shouldStop; i++)
			{
				auto postLoopStart = std::chrono::steady_clock::now();
				sampleAndCheckTrigger(); // Just sample, ignore trigger
				sampleCount++;

				// Maintain sample rate
				auto elapsed = std::chrono::steady_clock::now() - postLoopStart;
				if (elapsed < samplePeriod)
					std::this_thread::sleep_for(samplePeriod - elapsed);
			}

			// Extract samples from circular buffer
			extractSamples();

			state = RecorderState::READY;

			if (logger)
				logger->info("Recording complete: {} samples captured", capturedData.size());

			// Check trigger mode
			if (triggerMode == TriggerMode::SINGLE_SHOT)
			{
				break; // Stop thread
			}
			else if (triggerMode == TriggerMode::AUTO_REARM)
			{
				// Reset for next trigger
				bufferWriteIndex = 0;
				samplesInBuffer = 0;
				lastValue = 0.0;
				lastState = false;
				forceTriggerFlag = false;
				state = RecorderState::ARMED;

				if (logger)
					logger->info("Auto-rearmed for next trigger");
			}
		}

		sampleCount++;

		// Maintain sample rate
		auto elapsed = std::chrono::steady_clock::now() - loopStart;
		if (elapsed < samplePeriod)
			std::this_thread::sleep_for(samplePeriod - elapsed);
	}

	if (logger)
		logger->info("Recorder thread stopped");
}

bool RecorderModule::sampleAndCheckTrigger()
{
	// Read all variables
	std::vector<double> values;
	if (!backend->readVariables(config.addresses, config.sizes, values))
	{
		lastError = "Failed to read variables: " + backend->getLastError();
		if (logger)
			logger->error("RecorderModule::sampleAndCheckTrigger: {}", lastError);
		state = RecorderState::ERROR;
		return false;
	}

	// Create sample
	RecorderSample sample;
	auto now = std::chrono::steady_clock::now();
	sample.timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();

	for (size_t i = 0; i < config.addresses.size(); i++)
		sample.values[config.addresses[i]] = values[i];

	// Store in circular buffer
	uint32_t writeIdx = bufferWriteIndex.load();
	circularBuffer[writeIdx] = sample;
	bufferWriteIndex = (writeIdx + 1) % config.bufferSamples;

	if (samplesInBuffer < config.bufferSamples)
		samplesInBuffer++;

	// Check trigger
	if (forceTriggerFlag.load())
	{
		triggerSampleIndex = writeIdx;
		forceTriggerFlag = false;
		return true;
	}

	if (triggerConfig.type != TriggerType::NONE)
	{
		if (evaluateTrigger(sample))
		{
			triggerSampleIndex = writeIdx;
			return true;
		}
	}

	return false;
}

bool RecorderModule::evaluateTrigger(const RecorderSample& sample)
{
	// Get trigger variable value
	auto it = sample.values.find(triggerConfig.varAddress);
	if (it == sample.values.end())
		return false;

	double value = it->second;

	switch (triggerConfig.type)
	{
		case TriggerType::NONE:
			return false;

		case TriggerType::EDGE:
		{
			EdgeCondition condition = static_cast<EdgeCondition>(triggerConfig.condition);
			double threshold = triggerConfig.value1;

			bool currentState = (value >= threshold);

			bool triggered = false;
			if (condition == EdgeCondition::RISING && !lastState && currentState)
				triggered = true;
			else if (condition == EdgeCondition::FALLING && lastState && !currentState)
				triggered = true;
			else if (condition == EdgeCondition::BOTH && lastState != currentState)
				triggered = true;

			lastState = currentState;
			lastValue = value;

			return triggered;
		}

		case TriggerType::LEVEL:
		{
			LevelCondition condition = static_cast<LevelCondition>(triggerConfig.condition);
			double threshold = triggerConfig.value1;

			if (condition == LevelCondition::ABOVE)
				return value > threshold;
			else
				return value < threshold;
		}

		case TriggerType::WINDOW:
		{
			WindowCondition condition = static_cast<WindowCondition>(triggerConfig.condition);
			double lower = std::min(triggerConfig.value1, triggerConfig.value2);
			double upper = std::max(triggerConfig.value1, triggerConfig.value2);

			bool inside = (value >= lower && value <= upper);

			if (condition == WindowCondition::INSIDE)
				return inside;
			else
				return !inside;
		}

		case TriggerType::LOGIC:
			// TODO: Implement logic trigger (boolean combinations)
			if (logger)
				logger->warn("LOGIC trigger not yet implemented");
			return false;

		default:
			return false;
	}
}

void RecorderModule::extractSamples()
{
	std::lock_guard<std::mutex> lock(dataMutex);

	capturedData.clear();

	uint32_t totalSamples = samplesInBuffer.load();
	if (totalSamples == 0)
		return;

	// Calculate where to start reading from circular buffer
	uint32_t currentWriteIdx = bufferWriteIndex.load();
	uint32_t startIdx;

	if (totalSamples < config.bufferSamples)
	{
		// Buffer not full yet, start from beginning
		startIdx = 0;
	}
	else
	{
		// Buffer is full, start from oldest sample
		startIdx = currentWriteIdx;
	}

	// Extract all samples in order
	capturedData.reserve(totalSamples);
	for (uint32_t i = 0; i < totalSamples; i++)
	{
		uint32_t idx = (startIdx + i) % config.bufferSamples;
		capturedData.push_back(circularBuffer[idx]);
	}

	// Calculate statistics
	stats.totalSamples = totalSamples;

	if (!capturedData.empty())
	{
		stats.firstTimestamp = capturedData.front().timestamp;
		stats.lastTimestamp = capturedData.back().timestamp;

		// Find trigger index in extracted data
		// The trigger sample is at triggerSampleIndex in circular buffer
		// Map it to position in capturedData
		if (totalSamples < config.bufferSamples)
		{
			stats.triggerIndex = triggerSampleIndex;
		}
		else
		{
			// Buffer wrapped around
			if (triggerSampleIndex >= startIdx)
				stats.triggerIndex = triggerSampleIndex - startIdx;
			else
				stats.triggerIndex = config.bufferSamples - startIdx + triggerSampleIndex;
		}

		stats.preTriggerSamples = stats.triggerIndex;
		stats.postTriggerSamples = totalSamples - stats.triggerIndex - 1;

		if (stats.triggerIndex < capturedData.size())
			stats.triggerTimestamp = capturedData[stats.triggerIndex].timestamp;
	}

	if (logger)
	{
		logger->info("Extracted {} samples: pre-trigger={}, post-trigger={}",
					 stats.totalSamples, stats.preTriggerSamples, stats.postTriggerSamples);
	}
}
