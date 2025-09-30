/**
 * @file DataHandlerBase.hpp
 * @brief Base class for data acquisition handlers in MCUViewer
 *
 * This file provides the abstract base class for both variable viewer and trace viewer
 * data handlers. It manages the common state and provides thread-safe state transitions
 * for starting/stopping data acquisition.
 */

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "CSVStreamer.hpp"
#include "PlotGroupHandler.hpp"
#include "PlotHandler.hpp"
#include "VariableHandler.hpp"
#include "spdlog/spdlog.h"

/**
 * @class DataHandlerBase
 * @brief Abstract base class for managing data acquisition and streaming
 *
 * DataHandlerBase provides the common infrastructure for both ViewerDataHandler
 * (debug probe variable reading) and TraceDataHandler (SWO trace processing).
 * It manages state transitions, CSV logging, and coordinates between plot handlers
 * and variable handlers.
 *
 * The class runs data acquisition in a separate thread and provides thread-safe
 * state management for starting and stopping acquisition.
 */
class DataHandlerBase
{
   public:
	/**
	 * @brief Acquisition state enumeration
	 *
	 * Defines whether data acquisition is currently running or stopped.
	 */
	enum class State
	{
		STOP = 0,  ///< Data acquisition is stopped
		RUN = 1,   ///< Data acquisition is running
	};

	/**
	 * @brief Construct a new DataHandlerBase object
	 *
	 * @param plotGroupHandler Manages plot groups for organizing related plots together
	 * @param variableHandler Manages all variables that can be sampled/traced
	 * @param plotHandler Handles variable viewer plots (time-domain variable plots)
	 * @param tracePlotHandler Handles trace viewer plots (SWO trace channel plots)
	 * @param done Reference to application-wide shutdown flag for coordinating thread termination
	 * @param mtx Mutex for synchronizing access to shared plot/variable data between threads
	 * @param logger Logger instance for recording debug and error messages
	 */
	DataHandlerBase(PlotGroupHandler* plotGroupHandler, VariableHandler* variableHandler, PlotHandler* plotHandler, PlotHandler* tracePlotHandler, std::atomic<bool>& done, std::mutex* mtx, spdlog::logger* logger) : plotGroupHandler(plotGroupHandler), variableHandler(variableHandler), plotHandler(plotHandler), tracePlotHandler(tracePlotHandler), done(done), mtx(mtx), logger(logger)
	{
		csvStreamer = std::make_unique<CSVStreamer>(logger);
	}
	virtual ~DataHandlerBase() = default;

	/**
	 * @brief Get the last error message from the data reader
	 *
	 * This pure virtual method must be implemented by derived classes to provide
	 * error information specific to their data source (debug probe or trace probe).
	 *
	 * @return std::string Error message describing the last failure, or empty if no error
	 */
	virtual std::string getLastReaderError() const = 0;

	/**
	 * @brief Set the acquisition state (start or stop data acquisition)
	 *
	 * This method provides thread-safe state transitions. Setting to RUN starts
	 * data acquisition, while STOP halts it. The state change is asynchronous -
	 * the actual state transition occurs in the data handler thread.
	 *
	 * @param state Target state (State::RUN to start acquisition, State::STOP to stop)
	 * @note This method returns immediately; the state change is processed asynchronously
	 */
	void setState(State state)
	{
		if (state == viewerState)
			return;

		viewerState = state;
		stateChangeOrdered = true;
	}

	/**
	 * @brief Get the current acquisition state
	 *
	 * This method waits for any pending state changes to complete before returning
	 * the current state, ensuring consistency.
	 *
	 * @return State Current acquisition state (State::RUN or State::STOP)
	 * @note Busy-waits while a state change is in progress (potential for optimization)
	 */
	State getState() const
	{
		/* TODO possible deadlock */
		while (stateChangeOrdered);
		return viewerState;
	}

   protected:
	PlotGroupHandler* plotGroupHandler;    ///< Manages plot group organization and active group selection
	VariableHandler* variableHandler;       ///< Manages variable definitions and their properties
	PlotHandler* plotHandler;               ///< Manages variable viewer plots (time-series data)
	PlotHandler* tracePlotHandler;          ///< Manages trace viewer plots (SWO channel data)
	std::atomic<bool>& done;                ///< Application shutdown flag shared across all threads
	std::atomic<State> viewerState = State::STOP;  ///< Current acquisition state (atomic for thread safety)
	std::mutex* mtx;                        ///< Mutex for protecting shared plot/variable access
	std::thread dataHandle;                 ///< Thread running the data acquisition loop
	std::atomic<bool> stateChangeOrdered = false;  ///< Flag indicating a state change is pending
	spdlog::logger* logger;                 ///< Logger for diagnostic messages

	std::unique_ptr<CSVStreamer> csvStreamer;  ///< CSV file writer for logging acquired data to disk
};