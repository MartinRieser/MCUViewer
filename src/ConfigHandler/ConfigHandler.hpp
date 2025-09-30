/**
 * @file ConfigHandler.hpp
 * @brief Project configuration file management for MCUViewer
 *
 * Handles reading and writing project configuration files (.mcuview format)
 * which store plots, variables, groups, and acquisition settings using INI format.
 */

#pragma once

#include <map>
#include <memory>
#include <string>

#include "PlotGroupHandler.hpp"
#include "TraceDataHandler.hpp"
#include "Variable.hpp"
#include "VariableHandler.hpp"
#include "ViewerDataHandler.hpp"
#include "ini.h"
#include "spdlog/spdlog.h"

/**
 * @class ConfigHandler
 * @brief Manages project configuration persistence in INI format
 *
 * This class handles:
 * - Loading project files (.mcuview) and populating all handlers
 * - Saving current application state to project files
 * - Detecting if unsaved changes exist
 * - Parsing and serializing complex data structures (plots, variables, groups)
 * - Managing project file versioning
 *
 * @note Uses mINI library for INI file I/O
 * @note Configuration includes variables, plots, plot groups, and settings
 */
class ConfigHandler
{
   public:
	/**
	 * @struct GlobalSettings
	 * @brief Global configuration settings
	 */
	typedef struct
	{
		uint32_t version = 1;  /**< Configuration file format version */
	} GlobalSettings;

	/**
	 * @brief Constructs configuration handler
	 *
	 * @param configFilePath Path to initial config file
	 * @param plotHandler Handler for variable plots
	 * @param tracePlotHandler Handler for trace plots
	 * @param plotGroupHandler Handler for plot groups
	 * @param variableHandler Handler for variables
	 * @param viewerDataHandler Handler for viewer data and settings
	 * @param traceDataHandler Handler for trace data and settings
	 * @param logger Logger for debug output
	 */
	ConfigHandler(const std::string& configFilePath, PlotHandler* plotHandler, PlotHandler* tracePlotHandler, PlotGroupHandler* plotGroupHandler, VariableHandler* variableHandler, ViewerDataHandler* viewerDataHandler, TraceDataHandler* traceDataHandler, spdlog::logger* logger);
	~ConfigHandler() = default;

	/** @brief Changes the active config file path */
	bool changeConfigFile(const std::string& newConfigFilePath);

	/** @brief Reads config file and populates all handlers */
	bool readConfigFile(std::string& elfPath);

	/** @brief Saves current state to config file */
	bool saveConfigFile(const std::string& elfPath, const std::string& newSavePath);

	/** @brief Checks if current state differs from saved config */
	bool isSavingRequired(const std::string& elfPath);

	/**
	 * @brief Template function to parse string values from INI to typed variables
	 *
	 * @tparam T Target type (int, float, double, bool, enum, etc.)
	 * @param value String value from INI file
	 * @param result Reference to variable to store parsed value
	 * @note Handles integers, floats, doubles, booleans, and enums
	 * @note Logs warning if parsing fails
	 */
	template <typename T>
	void parseValue(const std::string& value, T& result)
	{
		try
		{
			if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>)
				result = std::stoi(value);
			else if constexpr (std::is_enum_v<T>)
				result = static_cast<T>(std::stoi(value));
			else if constexpr (std::is_same_v<T, float>)
				result = std::stof(value);
			else if constexpr (std::is_same_v<T, double>)
				result = std::stod(value);
			else if constexpr (std::is_same_v<T, bool>)
				result = value == "true";
			else
				throw std::invalid_argument("Unsupported type");
		}
		catch (...)
		{
			logger->warn("stoi incorect argument: {}", value);
		}
	}

   private:
	/** @brief Loads variables section from config */
	void loadVariables();

	/** @brief Loads plots section from config */
	void loadPlots();

	/** @brief Loads trace plots section from config */
	void loadTracePlots();

	/** @brief Loads plot groups section from config */
	void loadPlotGroups();

	/** @brief Prepares INI structure for saving */
	mINI::INIStructure prepareSaveConfigFile(const std::string& elfPath);

   private:
	/** @brief Global settings (version, etc.) */
	GlobalSettings globalSettings;

	/** @brief Path to current config file */
	std::string configFilePath;

	/** @brief Handler for variable plots */
	PlotHandler* plotHandler;

	/** @brief Handler for trace plots */
	PlotHandler* tracePlotHandler;

	/** @brief Handler for plot groups */
	PlotGroupHandler* plotGroupHandler;

	/** @brief Handler for variables */
	VariableHandler* variableHandler;

	/** @brief Handler for viewer settings */
	ViewerDataHandler* viewerDataHandler;

	/** @brief Handler for trace settings */
	TraceDataHandler* traceDataHandler;

	/** @brief Logger instance */
	spdlog::logger* logger;

	/** @brief Mapping from string to plot display format enum */
	std::map<std::string, Plot::displayFormat> displayFormatMap{{"DEC", Plot::displayFormat::DEC}, {"HEX", Plot::displayFormat::HEX}, {"BIN", Plot::displayFormat::BIN}};

	/** @brief mINI file object */
	std::unique_ptr<mINI::INIFile> file;

	/** @brief mINI structure for parsed data */
	std::unique_ptr<mINI::INIStructure> ini;
};
