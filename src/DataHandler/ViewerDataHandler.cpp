#include "ViewerDataHandler.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "JlinkDebugProbe.hpp"
#include "StlinkDebugProbe.hpp"

ViewerDataHandler::ViewerDataHandler(PlotGroupHandler* plotGroupHandler, VariableHandler* variableHandler, PlotHandler* plotHandler, PlotHandler* tracePlotHandler, std::atomic<bool>& done, std::mutex* mtx, spdlog::logger* logger) : DataHandlerBase(plotGroupHandler, variableHandler, plotHandler, tracePlotHandler, done, mtx, logger)
{
	dataHandle = std::thread(&ViewerDataHandler::dataHandler, this);
}
ViewerDataHandler::~ViewerDataHandler()
{
	// Stop recorder thread first
	recorderThreadRunning = false;
	if (recorderThreadHandle && recorderThreadHandle->joinable())
		recorderThreadHandle->join();

	if (dataHandle.joinable())
		dataHandle.join();
}

bool ViewerDataHandler::writeSeriesValue(Variable& var, double value)
{
	std::lock_guard<std::mutex> lock(*mtx);
	uint32_t rawValue = var.getRawFromDouble(value);
	return debugProbe->writeMemory(var.getAddress(), (uint8_t*)&rawValue, var.getSize());
}

std::string ViewerDataHandler::getLastReaderError() const
{
	return debugProbe->getLastErrorMsg();
}

void ViewerDataHandler::setDebugProbe(std::shared_ptr<IDebugProbe> probe)
{
	debugProbe = probe;
}

void ViewerDataHandler::setRecorderModule(std::shared_ptr<RecorderModule> recorder)
{
	// Stop old recorder thread if running
	if (recorderThreadHandle && recorderThreadHandle->joinable())
	{
		recorderThreadRunning = false;
		recorderThreadHandle->join();
	}

	recorderModule = recorder;

	// Start new recorder thread if module is set
	if (recorderModule)
	{
		recorderThreadRunning = true;
		recorderThreadHandle = std::make_unique<std::thread>(&ViewerDataHandler::recorderHandler, this);
	}
}

std::shared_ptr<RecorderModule> ViewerDataHandler::getRecorderModule() const
{
	return recorderModule;
}

IDebugProbe::DebugProbeSettings ViewerDataHandler::getProbeSettings() const
{
	return probeSettings;
}

void ViewerDataHandler::setProbeSettings(const IDebugProbe::DebugProbeSettings& settings)
{
	probeSettings = settings;
}

ViewerDataHandler::Settings ViewerDataHandler::getSettings() const
{
	return settings;
}

void ViewerDataHandler::setSettings(const Settings& newSettings)
{
	settings = newSettings;
	plotHandler->setMaxPoints(settings.maxPoints);
}

void ViewerDataHandler::updateVariables(double timestamp, const std::unordered_map<uint32_t, double>& values)
{
	/* get raw values and put them into variables based on addresses */
	for (std::shared_ptr<Variable> var : *variableHandler)
	{
		uint32_t address = var->getAddress();
		if (values.contains(address))
			var->setRawValue(values.at(address));
	}

	for (std::shared_ptr<Variable> var : *variableHandler)
	{
		uint32_t address = var->getAddress();
		if (values.contains(address))
			csvEntry[var->getName()] = var->transformToDouble();
	}

	for (auto plot : *plotHandler)
	{
		std::lock_guard<std::mutex> lock(*mtx);
		/* thread-safe part */
		plot->updateSeries();
		plot->addTimePoint(timestamp);
	}

	if (settings.shouldLog)
		csvStreamer->writeLine(timestamp, csvEntry);
}

void ViewerDataHandler::dataHandler()
{
	std::chrono::time_point<std::chrono::steady_clock> start;
	uint32_t timer = 0;
	double lastT = 0.0;

	while (!done)
	{
		if (viewerState == State::RUN)
		{
			double period = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count();

			if (probeSettings.mode == IDebugProbe::Mode::HSS)
			{
				if (!debugProbe->isValid())
					setState(State::STOP);

				auto maybeEntry = debugProbe->readSingleEntry();

				if (!maybeEntry.has_value())
					continue;

				auto [timestamp, rawValues] = maybeEntry.value();

				updateVariables(timestamp, rawValues);

				/* filter sampling frequency */
				averageSamplingPeriod = samplingPeriodFilter.filter((period - lastT));
				lastT = period;
				timer++;
			}

			// Check if recorder is active and get required sample rate
			bool recorderActive = false;
			uint32_t recorderSampleRate = settings.sampleFrequencyHz;

			if (recorderModule)
			{
				RecorderState recState = recorderModule->getState();
				if (recState == RecorderState::ARMED || recState == RecorderState::TRIGGERED)
				{
					recorderActive = true;
					RecorderConfig recConfig = recorderModule->getConfig();
					recorderSampleRate = recConfig.sampleRateHz;
					createRecorderSampleList();  // Update recorder sample list
				}
			}

			// Choose which variables to sample based on mode
			const auto& activeSampleList = recorderActive ? recorderSampleList : sampleList;
			uint32_t activeSampleRate = recorderActive ? recorderSampleRate : settings.sampleFrequencyHz;

			// Sample at the active rate (viewer or recorder)
			if (period > ((1.0 / activeSampleRate) * timer))
				{
					std::unordered_map<uint32_t, double> rawValues;

					/* sample by address */
					for (auto& [address, size] : activeSampleList)
					{
						uint8_t buffer[8] = {0};
						if (debugProbe->readMemory(address, buffer, size))
						{
							// For recorder: convert to proper double based on type
							// For regular plots: keep as raw bits (Variable class does conversion)
							double value = 0.0;

							if (recorderActive)
							{
								// Convert to double based on size (same logic as StlinkRecorderBackend)
								switch (size)
								{
									case 1:
										value = static_cast<double>(*reinterpret_cast<uint8_t*>(buffer));
										break;
									case 2:
										value = static_cast<double>(*reinterpret_cast<uint16_t*>(buffer));
										break;
									case 4:
										value = static_cast<double>(*reinterpret_cast<float*>(buffer));
										break;
									case 8:
										value = *reinterpret_cast<double*>(buffer);
										break;
								}
							}
							else
							{
								// Regular plots: store raw bits as double (Variable class will interpret)
								uint32_t rawBits = 0;
								std::memcpy(&rawBits, buffer, std::min(size, static_cast<uint8_t>(4)));
								value = static_cast<double>(rawBits);
							}
							rawValues[address] = value;
						}
						else
							setState(State::STOP);
					}
					double timestamp = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count();

					// Feed to recorder if active
					if (recorderActive && recorderModule)
					{
						recorderModule->feedSample(timestamp, rawValues);
					}
					// Otherwise update regular plots
					else
					{
						updateVariables(timestamp, rawValues);
					}

					/* filter sampling frequency */
					averageSamplingPeriod = samplingPeriodFilter.filter((period - lastT));
					lastT = period;
					timer++;
				}
		}
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(20));

		if (stateChangeOrdered)
		{
			if (viewerState == State::RUN)
			{
				createSampleList();
				prepareCSVFile();

				if (debugProbe->startAcqusition(probeSettings, sampleList, settings.sampleFrequencyHz))
				{
					timer = 0;
					lastT = 0.0;
					start = std::chrono::steady_clock::now();
				}
				else
					viewerState = State::STOP;
			}
			else
			{
				debugProbe->stopAcqusition();
				if (settings.shouldLog)
					csvStreamer->finishLogging();
			}
			stateChangeOrdered = false;
		}
	}
}

void ViewerDataHandler::createRecorderSampleList()
{
	recorderSampleList.clear();

	if (!recorderModule)
		return;

	RecorderState state = recorderModule->getState();
	if (state != RecorderState::ARMED && state != RecorderState::TRIGGERED)
		return;

	RecorderConfig config = recorderModule->getConfig();
	for (size_t i = 0; i < config.addresses.size(); i++)
	{
		recorderSampleList.push_back({config.addresses[i], config.sizes[i]});
	}
}

void ViewerDataHandler::createSampleList()
{
	sampleList.clear();

	auto checkIfElementExists = [&](std::pair<uint32_t, uint8_t> newElement)
	{
		return std::find_if(sampleList.begin(), sampleList.end(), [&newElement](const std::pair<uint32_t, uint8_t>& element)
							{ return element == newElement; }) != sampleList.end();
	};

	for (auto& [name, plotElem] : *plotGroupHandler->getActiveGroup())
	{
		auto plot = plotElem.plot;

		if (!plotElem.visibility)
			continue;

		for (auto& [name, ser] : plot->getSeriesMap())
		{
			if (!ser->visible)
				continue;

			std::pair<uint32_t, uint8_t> newElement = {ser->var->getAddress(), ser->var->getSize()};

			if (!checkIfElementExists(newElement))
				sampleList.push_back(newElement);

			Variable* maybeXAxisVariable = plot->getXAxisVariable();
			if (plot->getType() == Plot::Type::XY && maybeXAxisVariable != nullptr)
			{
				newElement = {maybeXAxisVariable->getAddress(), maybeXAxisVariable->getSize()};
				if (!checkIfElementExists(newElement))
					sampleList.push_back(newElement);
			}
		}
	}

	/* additionally scan for eventual bases of fractional variables that should be sampled */
	for (auto variable : *variableHandler)
	{
		if (variable->getFractional().baseVariable != nullptr)
		{
			auto var = variable->getFractional().baseVariable;
			std::pair<uint32_t, uint8_t> newElement = {var->getAddress(), var->getSize()};

			if (!checkIfElementExists(newElement))
				sampleList.push_back(newElement);
		}
	}

	/* mark actively sampled varaibles */
	for (auto variable : *variableHandler)
	{
		variable->setIsCurrentlySampled(false);
		if (std::find(sampleList.begin(), sampleList.end(), std::pair<uint32_t, uint8_t>(variable->getAddress(), variable->getSize())) != sampleList.end())
			variable->setIsCurrentlySampled(true);
	}
}

void ViewerDataHandler::prepareCSVFile()
{
	if (!settings.shouldLog)
		return;

	std::vector<std::string> headerNames;

	for (auto& [name, plotElem] : *plotGroupHandler->getActiveGroup())
	{
		auto plot = plotElem.plot;

		if (!plotElem.visibility)
			continue;

		for (auto& [name, ser] : plot->getSeriesMap())
			headerNames.push_back(name);
	}
	csvStreamer->prepareFile(settings.logFilePath);
	csvStreamer->createHeader(headerNames);
}

void ViewerDataHandler::recorderHandler()
{
	if (logger)
		logger->info("ViewerDataHandler: Recorder thread started");

	while (recorderThreadRunning && !done)
	{
		if (!recorderModule)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		RecorderState state = recorderModule->getState();

		// Monitor recorder state transitions
		switch (state)
		{
			case RecorderState::IDLE:
			case RecorderState::CONFIGURED:
			case RecorderState::ARMED:
			case RecorderState::TRIGGERED:
				// Just wait - recorder is managing itself
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				break;

			case RecorderState::READY:
				// Recording complete - data is ready for GUI to retrieve
				// GUI will call recorderModule->getAllData() to get samples
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				break;

			case RecorderState::RECORDER_ERROR:
				// Error state - log it
				if (logger)
					logger->error("ViewerDataHandler: Recorder in ERROR state");
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				break;
		}
	}

	if (logger)
		logger->info("ViewerDataHandler: Recorder thread stopped");
}