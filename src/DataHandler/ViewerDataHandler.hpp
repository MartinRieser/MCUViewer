/**
 * @file ViewerDataHandler.hpp
 * @brief Variable viewer data acquisition handler for MCUViewer
 *
 * This file implements the data handler for the Variable Viewer mode, which reads
 * variable values from microcontroller memory via a debug probe (STLink or JLink).
 * It manages periodic sampling, plot updates, and CSV logging of variable data.
 */

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "DataHandlerBase.hpp"
#include "IDebugProbe.hpp"
#include "MovingAverage.hpp"
#include "VariableHandler.hpp"

/**
 * @class ViewerDataHandler
 * @brief Manages variable data acquisition from debug probes
 *
 * ViewerDataHandler coordinates real-time reading of variable values from
 * microcontroller memory using debug interfaces (SWDIO/SWCLK). It:
 * - Manages periodic sampling at configurable frequencies (1 Hz to 1 MHz)
 * - Reads memory at variable addresses and converts raw bytes to typed values
 * - Updates plot buffers with new data points
 * - Handles CSV logging of acquired data
 * - Monitors actual sampling frequency and reports performance
 *
 * The handler runs in its own thread and synchronizes with the GUI thread
 * using mutexes. It supports dynamic ELF file reloading and variable address updates.
 *
 * @note Thread-safe: Data acquisition runs in a background thread
 */
class ViewerDataHandler : public DataHandlerBase
{
   public:
	static constexpr uint32_t minSamplinFrequencyHz = 1;        ///< Minimum sampling frequency (1 Hz)
	static constexpr uint32_t maxSamplinFrequencyHz = 1000000;  ///< Maximum sampling frequency (1 MHz)

	/**
	 * @brief Configuration settings for variable viewer acquisition
	 *
	 * Contains all user-configurable parameters for data acquisition behavior,
	 * including sampling rate, buffer sizes, ELF file handling, and logging options.
	 */
	typedef struct Settings
	{
		uint32_t sampleFrequencyHz = 100;              ///< Target sampling frequency in Hz
		uint32_t maxPoints = 10000;                    ///< Maximum data points to store per series
		uint32_t maxViewportPoints = 5000;             ///< Maximum points to display in viewport
		bool refreshAddressesOnElfChange = false;      ///< Auto-refresh variable addresses when ELF changes
		bool stopAcqusitionOnElfChange = false;        ///< Auto-stop acquisition when ELF file changes
		bool shouldLog = false;                        ///< Enable CSV logging of acquired data
		std::string logFilePath = "";                  ///< Directory path for CSV log files
		std::string gdbCommand = "gdb";                ///< GDB command for parsing ELF files
	} Settings;

	ViewerDataHandler(PlotGroupHandler* plotGroupHandler, VariableHandler* variableHandler, PlotHandler* plotHandler, PlotHandler* tracePlotHandler, std::atomic<bool>& done, std::mutex* mtx, spdlog::logger* logger);
	virtual ~ViewerDataHandler();

	/**
	 * @brief Get the last error message from the debug probe
	 * @return std::string Error description from the probe interface, or empty if no error
	 */
	std::string getLastReaderError() const;

	/**
	 * @brief Write a single data point to a variable's plot series
	 *
	 * This method is used for manual data injection into plots, typically for
	 * testing or custom data sources outside normal acquisition.
	 *
	 * @param var Variable to write data to
	 * @param value Numeric value to add to the variable's plot series
	 * @return true if the value was successfully written, false otherwise
	 */
	bool writeSeriesValue(Variable& var, double value);

	/**
	 * @brief Get current debug probe configuration settings
	 * @return IDebugProbe::DebugProbeSettings Current probe settings (device path, speed, mode, etc.)
	 */
	IDebugProbe::DebugProbeSettings getProbeSettings() const;

	/**
	 * @brief Set debug probe configuration
	 *
	 * Configures the debug probe parameters such as device selection, SWD speed,
	 * and target MCU settings. Changes take effect on next acquisition start.
	 *
	 * @param settings New probe configuration to apply
	 */
	void setProbeSettings(const IDebugProbe::DebugProbeSettings& settings);

	/**
	 * @brief Set the debug probe interface to use for acquisition
	 *
	 * Assigns a specific debug probe implementation (STLink or JLink) to use
	 * for reading variable data from the target MCU.
	 *
	 * @param probe Shared pointer to initialized debug probe instance
	 */
	void setDebugProbe(std::shared_ptr<IDebugProbe> probe);

	/**
	 * @brief Get current viewer settings
	 * @return Settings Current acquisition settings (sampling rate, buffer sizes, etc.)
	 */
	Settings getSettings() const;

	/**
	 * @brief Update viewer settings
	 *
	 * Applies new acquisition settings. Some settings (like maxPoints) take effect
	 * immediately, while others (like sampleFrequencyHz) apply on next acquisition cycle.
	 *
	 * @param newSettings Updated settings structure to apply
	 */
	void setSettings(const Settings& newSettings);

	/**
	 * @brief Get the actual measured sampling frequency
	 *
	 * Returns the real-time average sampling frequency based on actual timing
	 * measurements, which may differ from the target frequency due to probe
	 * communication delays or system load.
	 *
	 * @return double Measured average sampling frequency in Hz, or 0.0 if not sampling
	 */
	double getAverageSamplingFrequency() const
	{
		if (averageSamplingPeriod > 0.0)
			return 1.0 / averageSamplingPeriod;
		return 0.0;
	}

   private:
	using SampleListType = std::vector<std::pair<uint32_t, uint8_t>>;  ///< List of (address, size) pairs for bulk memory reads

	/**
	 * @brief Update plot buffers with newly acquired variable values
	 * @param timestamp Current acquisition timestamp in seconds
	 * @param values Map of memory addresses to their read values
	 */
	void updateVariables(double timestamp, const std::unordered_map<uint32_t, double>& values);

	/**
	 * @brief Main data acquisition loop (runs in separate thread)
	 *
	 * Continuously reads variable values from the debug probe at the configured
	 * sampling rate and updates plots until stopped.
	 */
	void dataHandler();

	/**
	 * @brief Initialize CSV file for logging with appropriate headers
	 *
	 * Creates a new CSV file in the configured log directory and writes
	 * column headers based on currently active variables.
	 */
	void prepareCSVFile();

	/**
	 * @brief Build optimized memory read list for bulk acquisition
	 *
	 * Creates a list of memory addresses and sizes to read in a single
	 * batch operation, improving acquisition performance.
	 */
	void createSampleList();

   private:
	static constexpr size_t maxVariablesOnSinglePlot = 100;  ///< Maximum variables that can be plotted simultaneously
	std::shared_ptr<IDebugProbe> debugProbe;           ///< Active debug probe interface (STLink or JLink)
	IDebugProbe::DebugProbeSettings probeSettings{};   ///< Current debug probe configuration
	MovingAverage samplingPeriodFilter{1000};          ///< Moving average filter for measuring actual sampling rate
	double averageSamplingPeriod = 0.0;                ///< Measured average time between samples in seconds
	Settings settings{};                                ///< Current acquisition settings
	std::unordered_map<std::string, double> csvEntry;  ///< Buffer for building CSV output rows

	SampleListType sampleList;  ///< Optimized list of memory locations to read each sample cycle
};