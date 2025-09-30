/**
 * @file TraceDataHandler.hpp
 * @brief SWO trace data acquisition handler for MCUViewer
 *
 * This file implements the data handler for the Trace Viewer mode, which captures
 * and processes SWO (Serial Wire Output) trace data from microcontroller debug interfaces.
 * It manages ITM channel decoding, plot updates, triggering, and CSV logging of trace data.
 */

#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "DataHandlerBase.hpp"
#include "Plot.hpp"
#include "StlinkTraceProbe.hpp"
#include "TraceReader.hpp"
#include "spdlog/spdlog.h"

/**
 * @class TraceDataHandler
 * @brief Manages SWO trace data acquisition and processing
 *
 * TraceDataHandler coordinates real-time capture and decoding of SWO trace data
 * from microcontroller ITM (Instrumentation Trace Macrocell) channels. It:
 * - Captures raw SWO data from trace probes (STLink or JLink)
 * - Decodes ITM packets into channel data
 * - Supports up to 10 ITM channels for parallel data streams
 * - Implements triggering on channel values for oscilloscope-like behavior
 * - Tracks frame errors and timing issues for quality monitoring
 * - Updates trace plots in real-time
 * - Handles CSV logging of trace data
 *
 * The handler is optimized for high-speed trace capture and processes data
 * in a separate thread to maintain real-time performance.
 *
 * @note Thread-safe: Trace acquisition runs in a background thread
 */
class TraceDataHandler : public DataHandlerBase
{
   public:
	/**
	 * @brief Configuration settings for trace viewer acquisition
	 *
	 * Contains all user-configurable parameters for SWO trace capture,
	 * including timing configuration, triggering, buffer sizes, and logging.
	 */
	typedef struct
	{
		uint32_t coreFrequency = 160000;           ///< MCU core frequency in kHz for trace timing calculations
		uint32_t tracePrescaler = 10;              ///< SWO prescaler dividing core clock to trace clock
		uint32_t maxPoints = 10000;                ///< Maximum data points to store per trace channel
		uint32_t maxViewportPointsPercent = 10;    ///< Percentage of maxPoints to display in viewport
		int32_t triggerChannel = -1;               ///< ITM channel to trigger on (-1 for no trigger)
		double triggerLevel = 0.9;                 ///< Trigger threshold value (normalized 0.0-1.0)
		bool shouldReset = false;                  ///< Reset trace buffers on acquisition start
		uint32_t timeout = 2;                      ///< Timeout in seconds for trace data reception
		bool shouldLog = false;                    ///< Enable CSV logging of trace data
		std::string logFilePath = "";              ///< Directory path for CSV log files
	} Settings;

	TraceDataHandler(PlotGroupHandler* plotGroupHandler, VariableHandler* variableHandler, PlotHandler* plotHandler, PlotHandler* tracePlotHandler, std::atomic<bool>& done, std::mutex* mtx, spdlog::logger* logger);
	~TraceDataHandler();

	/** @brief Get trace quality indicators (frame errors, buffer utilization, etc.)
	 *  @return TraceReader::TraceIndicators Structure containing trace statistics */
	TraceReader::TraceIndicators getTraceIndicators() const;

	/** @brief Get timestamps of all frame errors for visualization
	 *  @return std::vector<double> Vector of timestamps (in seconds) where frame errors occurred */
	std::vector<double> getErrorTimestamps();

	/** @brief Get timestamps of all delayed frames for visualization
	 *  @return std::vector<double> Vector of timestamps (in seconds) where frames were delayed */
	std::vector<double> getDelayed3Timestamps();

	/** @brief Get the last error message from the trace probe
	 *  @return std::string Error description from the probe interface, or empty if no error */
	std::string getLastReaderError() const;

	/** @brief Set which ITM channel to use for triggering
	 *  @param triggerChannel Channel number (0-9) or -1 to disable triggering */
	void setTriggerChannel(int32_t triggerChannel);

	/** @brief Get the currently configured trigger channel
	 *  @return int32_t Active trigger channel number or -1 if triggering disabled */
	int32_t getTriggerChannel() const;

	/** @brief Set the trace probe interface to use for capture
	 *  @param probe Shared pointer to initialized trace probe instance (STLink or JLink) */
	void setDebugProbe(std::shared_ptr<ITraceProbe> probe);

	/** @brief Get current trace probe configuration settings
	 *  @return ITraceProbe::TraceProbeSettings Current probe settings */
	ITraceProbe::TraceProbeSettings getProbeSettings() const;

	/** @brief Set trace probe configuration (SWO speed, buffer sizes, etc.)
	 *  @param settings New probe configuration to apply */
	void setProbeSettings(const ITraceProbe::TraceProbeSettings& settings);

	/** @brief Get current trace viewer settings
	 *  @return Settings Current acquisition settings */
	Settings getSettings() const;

	/** @brief Update trace viewer settings
	 *  @param settings Updated settings structure to apply */
	void setSettings(const Settings& settings);

	/* TODO refactor so these are not here: (ideally using traceVeriableHandler)*/
	/** @brief Convert raw trace value to double based on plot type configuration
	 *  @param plot Plot containing type/format information
	 *  @param value Raw ITM channel value
	 *  @return double Converted value for plotting */
	double getDoubleValue(const Plot& plot, uint32_t value);

	/** @brief Initialize trace plots for all ITM channels */
	void initPlots();

	std::map<std::string, std::shared_ptr<Variable>> traceVars;  ///< Variables representing trace channels

   private:
	/** @brief Main trace acquisition loop (runs in separate thread) */
	void dataHandler();

	/** @brief Initialize CSV file for trace logging with channel headers */
	void prepareCSVFile();

   private:
	/**
	 * @class MarkerTimestamps
	 * @brief Tracks timestamps of trace quality events for visualization
	 *
	 * Maintains a sliding window of timestamps where trace errors or delays
	 * occurred, allowing them to be displayed as markers on trace plots.
	 */
	class MarkerTimestamps
	{
	   public:
		/** @brief Clear all stored timestamps */
		void reset()
		{
			timestamps.clear();
			previousErrors = 0;
		}

		/** @brief Update marker list with new error/delay occurrences
		 *  @param time Current timestamp in seconds
		 *  @param oldestTimestamp Oldest timestamp still in viewport (for cleanup)
		 *  @param totalErrors Cumulative error count (marks added when count increases) */
		void handle(double time, double oldestTimestamp, uint32_t totalErrors)
		{
			if (previousErrors != totalErrors)
				timestamps.push_back(time);

			while (timestamps.size() && timestamps.front() < oldestTimestamp)
				timestamps.pop_front();
			previousErrors = totalErrors;
		}

		/** @brief Get number of markers currently stored
		 *  @return size_t Count of marker timestamps */
		size_t size() const
		{
			return timestamps.size();
		}

		/** @brief Get all marker timestamps as a vector for plotting
		 *  @return std::vector<double> All timestamp markers */
		auto getVector()
		{
			return std::vector<double>(timestamps.begin(), timestamps.end());
		}

	   private:
		std::deque<double> timestamps;  ///< Queue of marker timestamps within viewport
		uint32_t previousErrors;        ///< Last known error count for detecting changes
	};

	Settings settings{};                        ///< Current trace acquisition settings
	std::unique_ptr<TraceReader> traceReader;   ///< Trace data decoder and processor

	MarkerTimestamps errorFrames{};    ///< Timestamps where frame errors occurred
	MarkerTimestamps delayed3Frames{}; ///< Timestamps where frames were delayed

	std::string lastErrorMsg{};  ///< Last error message from trace probe

	ITraceProbe::TraceProbeSettings probeSettings;  ///< Current trace probe configuration

	bool traceTriggered = false;  ///< True after trigger condition has been met
	static constexpr uint32_t channels = 10;  ///< Number of ITM channels supported (0-9)
	static constexpr size_t maxAllowedViewportErrors = 100;  ///< Max error markers to display

	std::unordered_map<std::string, double> csvEntry;  ///< Buffer for building CSV output rows
};
