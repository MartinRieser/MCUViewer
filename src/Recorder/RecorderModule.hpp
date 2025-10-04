#ifndef _RECORDERMODULE_HPP
#define _RECORDERMODULE_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "IDebugProbe.hpp"
#include "spdlog/spdlog.h"

/**
 * @brief States of the recorder module
 */
enum class RecorderState
{
	IDLE,           // Not configured
	CONFIGURED,     // Configured but not armed
	ARMED,          // Waiting for trigger, collecting pre-trigger samples
	TRIGGERED,      // Trigger fired, collecting post-trigger samples
	READY,          // Recording complete, data ready for download
	RECORDER_ERROR  // Error state
};

/**
 * @brief Trigger types supported by the recorder
 */
enum class TriggerType
{
	NONE = 0,       // No trigger, free-running
	EDGE = 1,       // Trigger when crossing threshold (rising/falling/both)
	WINDOW = 2,     // Trigger when entering/exiting a value range
	LOGIC = 3       // Boolean combination of conditions
};

/**
 * @brief Trigger mode
 */
enum class TriggerMode
{
	SINGLE_SHOT,    // Trigger once and stop
	AUTO_REARM,     // Automatically rearm after trigger
	FREE_RUNNING    // Continuous recording without trigger
};

/**
 * @brief Edge conditions for edge triggers
 *
 * RISING:  Triggers when value crosses threshold going from below to above
 * FALLING: Triggers when value crosses threshold going from above to below
 * BOTH:    Triggers on any threshold crossing (rising or falling)
 */
enum class EdgeCondition
{
	RISING = 0,
	FALLING = 1,
	BOTH = 2
};

/**
 * @brief Window conditions for window triggers
 */
enum class WindowCondition
{
	INSIDE = 0,
	OUTSIDE = 1
};

/**
 * @brief Configuration for a single trigger condition
 */
struct TriggerConfig
{
	TriggerType type = TriggerType::NONE;
	uint32_t varAddress = 0;          // Address of variable to monitor
	uint32_t condition = 0;           // Condition code (cast to EdgeCondition/WindowCondition)
	double value1 = 0.0;              // Primary threshold value
	double value2 = 0.0;              // Secondary threshold (for window triggers)
	double hysteresis = 0.0;          // Hysteresis for noise immunity
	uint32_t preTriggerSamples = 800; // Absolute number of samples before trigger point
};

/**
 * @brief Configuration for recorder module
 */
struct RecorderConfig
{
	uint32_t bufferSamples = 1000;              // Total buffer size in samples
	uint32_t sampleRateHz = 100;                // Sampling rate in Hz
	std::vector<uint32_t> addresses;            // Variable addresses to record
	std::vector<uint8_t> sizes;                 // Size of each variable (1, 2, 4, 8 bytes)
	bool useHardwareRecording = false;          // Use hardware recording if available
	bool useExternalSampling = false;           // Use external sampling (fed by ViewerDataHandler)
};

/**
 * @brief Single sample with timestamp and variable values
 */
struct RecorderSample
{
	double timestamp = 0.0;                     // Time in seconds
	std::map<uint32_t, double> values;          // Map of address -> value
};

/**
 * @brief Statistics about recorded data
 */
struct RecorderStats
{
	uint32_t totalSamples = 0;
	uint32_t preTriggerSamples = 0;
	uint32_t postTriggerSamples = 0;
	double triggerTimestamp = 0.0;
	double firstTimestamp = 0.0;
	double lastTimestamp = 0.0;
	uint32_t triggerIndex = 0;
};

/**
 * @brief Abstract interface for recorder backends
 *
 * This interface allows different debug probes (STLink, JLink, UART)
 * to implement recording in different ways (software polling vs hardware capture)
 */
class RecorderBackend
{
   public:
	virtual ~RecorderBackend() = default;

	/**
	 * @brief Read all configured variables in one operation
	 * @param addresses Vector of variable addresses
	 * @param sizes Vector of variable sizes (bytes)
	 * @param values Output vector of values (as double)
	 * @return true if read succeeded, false otherwise
	 */
	virtual bool readVariables(const std::vector<uint32_t>& addresses,
							   const std::vector<uint8_t>& sizes,
							   std::vector<double>& values) = 0;

	/**
	 * @brief Check if this backend supports hardware recording
	 * @return true if hardware recording is available
	 */
	virtual bool supportsHardwareRecording() const = 0;

	/**
	 * @brief Setup hardware recording (if supported)
	 * @param config Recorder configuration
	 * @param trigger Trigger configuration
	 * @return true if setup succeeded
	 */
	virtual bool setupHardwareRecording(const RecorderConfig& config,
										const TriggerConfig& trigger)
	{
		(void)config;
		(void)trigger;
		return false; // Default: not supported
	}

	/**
	 * @brief Arm hardware trigger (if supported)
	 * @param mode Trigger mode
	 * @return true if arm succeeded
	 */
	virtual bool armHardwareTrigger(TriggerMode mode)
	{
		(void)mode;
		return false; // Default: not supported
	}

	/**
	 * @brief Check if hardware trigger has fired
	 * @return true if triggered
	 */
	virtual bool isHardwareTriggered() const
	{
		return false; // Default: not supported
	}

	/**
	 * @brief Download hardware buffer data
	 * @param samples Output vector of samples
	 * @return true if download succeeded
	 */
	virtual bool downloadHardwareBuffer(std::vector<RecorderSample>& samples)
	{
		(void)samples;
		return false; // Default: not supported
	}

	/**
	 * @brief Get last error message
	 */
	virtual std::string getLastError() const = 0;
};

// Include TriggerEvaluator after type definitions to avoid circular dependency
#include "TriggerEvaluator.hpp"

/**
 * @brief Main recorder module class
 *
 * This class provides probe-agnostic recording functionality with trigger support.
 * It uses a circular buffer to store pre/post-trigger samples and can work with
 * any debug probe (STLink, JLink, UART) via the RecorderBackend interface.
 */
class RecorderModule
{
   public:
	RecorderModule(std::shared_ptr<RecorderBackend> backend, spdlog::logger* logger);
	~RecorderModule();

	/**
	 * @brief Configure the recorder
	 * @param config Recorder configuration
	 * @return true if configuration succeeded
	 */
	bool configure(const RecorderConfig& config);

	/**
	 * @brief Setup trigger conditions
	 * @param trigger Trigger configuration
	 * @return true if setup succeeded
	 */
	bool setupTrigger(const TriggerConfig& trigger);

	/**
	 * @brief Arm the recorder (start sampling and waiting for trigger)
	 * @param mode Trigger mode
	 * @return true if arm succeeded
	 */
	bool arm(TriggerMode mode);

	/**
	 * @brief Disarm the recorder (stop sampling)
	 */
	void disarm();

	/**
	 * @brief Force trigger immediately
	 */
	void forceTrigger();

	/**
	 * @brief Get current recorder state
	 */
	RecorderState getState() const;

	/**
	 * @brief Get recorded data
	 * @param startIndex Starting sample index (relative to trigger point, negative = before trigger)
	 * @param count Number of samples to retrieve
	 * @return Vector of samples
	 */
	std::vector<RecorderSample> getData(int32_t startIndex, uint32_t count) const;

	/**
	 * @brief Get all recorded data
	 * @return Vector of all samples
	 */
	std::vector<RecorderSample> getAllData() const;

	/**
	 * @brief Get recording statistics
	 */
	RecorderStats getStats() const;

	/**
	 * @brief Get current configuration
	 */
	RecorderConfig getConfig() const;

	/**
	 * @brief Get current trigger configuration
	 */
	TriggerConfig getTriggerConfig() const;

	/**
	 * @brief Reset the recorder to IDLE state
	 */
	void reset();

	/**
	 * @brief Feed a sample to the recorder (external sampling mode)
	 * @param timestamp Sample timestamp in seconds
	 * @param values Map of address -> value
	 * @return true if triggered, false otherwise
	 */
	bool feedSample(double timestamp, const std::unordered_map<uint32_t, double>& values);

	/**
	 * @brief Check if recorder is in external sampling mode (no internal thread)
	 */
	bool isExternalSamplingMode() const { return !recorderThread; }

   private:
	/**
	 * @brief Main recorder thread function
	 */
	void recorderThreadFunc();

	/**
	 * @brief Sample variables and check trigger condition
	 * @return true if trigger fired
	 */
	bool sampleAndCheckTrigger();


	/**
	 * @brief Calculate trigger index in circular buffer
	 */
	uint32_t calculateTriggerIndex() const;

	/**
	 * @brief Extract samples from circular buffer
	 */
	void extractSamples();

   private:
	std::shared_ptr<RecorderBackend> backend;
	spdlog::logger* logger;

	// Configuration
	RecorderConfig config;
	TriggerConfig triggerConfig;
	TriggerMode triggerMode = TriggerMode::SINGLE_SHOT;

	// State
	std::atomic<RecorderState> state{RecorderState::IDLE};
	std::atomic<bool> shouldStop{false};
	std::atomic<bool> forceTriggerFlag{false};

	// Circular buffer (pre-allocated)
	std::vector<RecorderSample> circularBuffer;
	std::atomic<uint32_t> bufferWriteIndex{0};
	std::atomic<uint32_t> samplesInBuffer{0};
	uint32_t triggerSampleIndex = 0;

	// Final extracted data
	std::vector<RecorderSample> capturedData;
	mutable std::mutex dataMutex;

	// Statistics
	RecorderStats stats;

	// External sampling mode tracking
	uint32_t postTriggerSamplesNeeded = 0;
	uint32_t postTriggerSamplesCollected = 0;

	// Threading
	std::unique_ptr<std::thread> recorderThread;

	// Trigger evaluator
	TriggerEvaluator triggerEvaluator;

	// Last error
	std::string lastError;
};

#endif // _RECORDERMODULE_HPP
