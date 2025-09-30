/**
 * @file Gui.hpp
 * @brief Main GUI controller for MCUViewer application
 *
 * This file implements the primary graphical user interface controller that manages
 * all UI windows, debug probe interfaces, menu systems, and user interactions for
 * both the Variable Viewer and Trace Viewer modes.
 */

#ifndef _GUI_HPP
#define _GUI_HPP

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>

#include "ConfigHandler.hpp"
#include "GuiPlotEdit.hpp"
#include "GuiPlotsTree.hpp"
#include "GuiVarTable.hpp"
#include "GuiVariablesEdit.hpp"
#include "IDebugProbe.hpp"
#include "IFileHandler.hpp"
#include "ImguiPlugins.hpp"
#ifdef JLINK_AVAILABLE
#include "JlinkDebugProbe.hpp"
#include "JlinkTraceProbe.hpp"
#endif
#include "Plot.hpp"
#include "PlotGroupHandler.hpp"
#include "Popup.hpp"
#include "TraceDataHandler.hpp"
#include "VariableHandler.hpp"
#include "ViewerDataHandler.hpp"
#include "imgui.h"
#include "implot.h"

/**
 * @class Gui
 * @brief Main GUI controller class that manages all UI components and user interactions
 *
 * The Gui class is the central controller for the MCUViewer application's user interface.
 * It manages:
 * - Main menu bar and window creation
 * - Debug probe selection and configuration (STLink, JLink)
 * - Trace probe selection and configuration
 * - Variable viewer and trace viewer modes
 * - Plot rendering and interaction
 * - Project file management (open, save, new)
 * - Keyboard shortcuts
 * - Modal dialogs and popups
 * - Data acquisition settings
 *
 * @note This class uses ImGui for all UI rendering and manages the main application loop on macOS
 */
class Gui
{
   public:
	/**
	 * @brief Constructs the main GUI controller
	 *
	 * @param plotHandler Handler for variable plot management
	 * @param variableHandler Handler for variable management
	 * @param configHandler Handler for configuration file I/O
	 * @param plotGroupHandler Handler for plot group organization
	 * @param fileHandler Interface for file dialog operations
	 * @param tracePlotHandler Handler for SWO trace plot management
	 * @param viewerDataHandler Data handler for variable viewer mode
	 * @param traceDataHandler Data handler for trace viewer mode
	 * @param done Reference to atomic boolean flag for application shutdown
	 * @param mtx Mutex for thread synchronization
	 * @param logger Logger instance for debug output
	 * @param projectPath Reference to current project file path
	 */
	Gui(PlotHandler* plotHandler, VariableHandler* variableHandler, ConfigHandler* configHandler, PlotGroupHandler* plotGroupHandler, IFileHandler* fileHandler, PlotHandler* tracePlotHandler, ViewerDataHandler* viewerDataHandler, TraceDataHandler* traceDataHandler, std::atomic<bool>& done, std::mutex* mtx, spdlog::logger* logger, std::string& projectPath);

	/**
	 * @brief Destructor - cleans up GUI resources and joins main thread
	 */
	~Gui();

#ifdef __APPLE__
	/**
	 * @brief Runs the main GUI event loop on macOS
	 *
	 * @note macOS requires the main thread to handle UI events, so this method
	 *       runs the entire ImGui render loop on the main thread
	 */
	void runMainLoop();
#endif

   private:
	/** @brief Flag to show ImGui demo window for development */
	static constexpr bool showDemoWindow = false;

	/** @brief Maps data handler states to display strings */
	const std::map<DataHandlerBase::State, std::string> viewerStateMap{{DataHandlerBase::State::RUN, "RUNNING"}, {DataHandlerBase::State::STOP, "STOPPED"}};

	/** @brief Main GUI thread handle (used on non-macOS platforms) */
	std::thread threadHandle;

	/** @brief Handler for variable viewer plots */
	PlotHandler* plotHandler;

	/** @brief Handler for variable management and storage */
	VariableHandler* variableHandler;

	/** @brief Handler for project configuration */
	ConfigHandler* configHandler;

	/** @brief Handler for plot grouping and organization */
	PlotGroupHandler* plotGroupHandler;

	/** @brief Path to current project configuration file */
	std::string projectConfigPath;

	/** @brief Path to current ELF debug file */
	std::string projectElfPath;

	/** @brief Flag to show acquisition settings modal window */
	bool showAcqusitionSettingsWindow = false;

	/** @brief Flag to show about dialog */
	bool showAboutWindow = false;

	/** @brief Flag to show preferences window */
	bool showPreferencesWindow = false;

	/** @brief Flag to show variable selection window */
	bool showSelectVariablesWindow = false;

	/** @brief Interface for file open/save dialogs */
	IFileHandler* fileHandler;

	/** @brief Handler for SWO trace plots */
	PlotHandler* tracePlotHandler;

	/** @brief Data handler for variable viewer mode */
	ViewerDataHandler* viewerDataHandler;

	/** @brief Data handler for trace viewer mode */
	TraceDataHandler* traceDataHandler;

	/** @brief STLink debug probe instance */
	std::shared_ptr<IDebugProbe> stlinkProbe;
#ifdef JLINK_AVAILABLE
	/** @brief JLink debug probe instance (if available) */
	std::shared_ptr<IDebugProbe> jlinkProbe;
#endif
	/** @brief Currently selected debug probe */
	std::shared_ptr<IDebugProbe> debugProbeDevice;

	/** @brief List of detected debug probe device names */
	std::vector<std::string> devicesList{};

	/** @brief String displayed when no probes are detected */
	const std::string noDevices = "No debug probes found!";

	/** @brief STLink trace probe instance */
	std::shared_ptr<ITraceProbe> stlinkTraceProbe;
#ifdef JLINK_AVAILABLE
	/** @brief JLink trace probe instance (if available) */
	std::shared_ptr<ITraceProbe> jlinkTraceProbe;
#endif
	/** @brief Currently selected trace probe */
	std::shared_ptr<ITraceProbe> traceProbeDevice;

	/** @brief Reference to application done flag for shutdown coordination */
	std::atomic<bool>& done;

	/** @brief Popup instance for general notifications */
	Popup popup;

	/** @brief Popup instance for acquisition error messages */
	Popup acqusitionErrorPopup;

	/**
	 * @enum ActiveViewType
	 * @brief Defines which viewer mode is currently active
	 */
	enum class ActiveViewType : uint8_t
	{
		VarViewer = 0,    /**< Variable viewer mode (memory reading) */
		TraceViewer = 1,  /**< Trace viewer mode (SWO trace output) */
	};

	/** @brief Currently active viewer mode */
	ActiveViewType activeView = ActiveViewType::VarViewer;

	/** @brief Mutex for thread-safe access to shared data */
	std::mutex* mtx;

	/** @brief Logger for debug and info messages */
	spdlog::logger* logger;

	/** @brief Window for editing plot properties */
	std::shared_ptr<PlotEditWindow> plotEditWindow;

	/** @brief Window displaying variable table */
	std::shared_ptr<VariableTableWindow> variableTable;

	/** @brief Tree view of plots and groups */
	std::shared_ptr<PlotsTree> plotsTree;

#ifdef __APPLE__
	/** @brief Path to project file passed from external source (macOS file associations) */
	std::string externalProjectPath;
#endif

   private:
	/**
	 * @brief Main GUI rendering thread function
	 *
	 * @param externalPath Path to project file if opened externally (e.g., file association)
	 * @note This method runs in a separate thread on non-macOS platforms
	 */
	void mainThread(std::string externalPath);

	/**
	 * @brief Renders the main menu bar
	 *
	 * Draws the top menu with File, Options, and Help menus containing
	 * project operations, settings, and documentation links
	 */
	void drawMenu();

	/**
	 * @brief Draws the start/stop button for data acquisition
	 *
	 * @param activeDataHandler Pointer to currently active data handler (viewer or trace)
	 * @note Button appearance changes based on acquisition state
	 */
	void drawStartButton(DataHandlerBase* activeDataHandler);

	/**
	 * @brief Draws debug probe selection dropdown
	 *
	 * Displays combo box with available debug probes (STLink, JLink)
	 * and handles probe selection and connection
	 */
	void drawDebugProbes();

	/**
	 * @brief Draws trace probe selection dropdown
	 *
	 * Displays combo box with available trace probes for SWO capture
	 */
	void drawTraceProbes();

	/**
	 * @brief Draws button to update variable addresses from ELF file
	 *
	 * Shows button that triggers GDB parser to refresh variable addresses
	 * when ELF file has changed
	 */
	void drawUpdateAddressesFromElf();

	/**
	 * @brief Draws acquisition settings modal window
	 *
	 * @param type Type of viewer (VarViewer or TraceViewer) to configure
	 * @note Modal window for configuring sampling rate, probe settings, etc.
	 */
	void drawAcqusitionSettingsWindow(ActiveViewType type);

	/**
	 * @brief Draws variable viewer specific acquisition settings
	 *
	 * Renders settings for memory reading, sampling period, max points, etc.
	 */
	void acqusitionSettingsViewer();

	/**
	 * @brief Draws logging settings UI (template for both viewer types)
	 *
	 * @tparam Settings Settings structure type (ViewerDataHandler::Settings or TraceDataHandler::Settings)
	 * @param handler Plot handler for the active mode
	 * @param settings Settings structure to modify
	 */
	template <typename Settings>
	void drawLoggingSettings(PlotHandler* handler, Settings& settings);

	/**
	 * @brief Draws GDB command settings UI
	 *
	 * @param settings Viewer settings structure containing GDB configuration
	 */
	void drawGdbSettings(ViewerDataHandler::Settings& settings);

	/**
	 * @brief Draws the About window with version and license info
	 */
	void drawAboutWindow();

	/**
	 * @brief Draws the Preferences window for global application settings
	 */
	void drawPreferencesWindow();

	/**
	 * @brief Draws trace viewer specific acquisition settings
	 *
	 * Renders settings for SWO trace capture, baud rate, trace channels, etc.
	 */
	void acqusitionSettingsTrace();

	/**
	 * @brief Draws all visible plots in the active plot group
	 *
	 * Iterates through active plot group and renders each visible plot
	 */
	void drawPlots();

	/**
	 * @brief Draws a curve-type plot
	 *
	 * @param plot Shared pointer to plot to render
	 * @note Renders time-series line plots with ImPlot
	 */
	void drawPlotCurve(std::shared_ptr<Plot> plot);

	/**
	 * @brief Draws a bar-type plot
	 *
	 * @param plot Shared pointer to plot to render
	 * @note Renders bar chart visualization
	 */
	void drawPlotBar(std::shared_ptr<Plot> plot);

	/**
	 * @brief Draws a table-type plot
	 *
	 * @param plot Shared pointer to plot to render
	 * @note Renders tabular data view with ImGui tables
	 */
	void drawPlotTable(std::shared_ptr<Plot> plot);

	/**
	 * @brief Draws an XY scatter plot
	 *
	 * @param plot Shared pointer to plot to render
	 * @note Renders XY correlation plot with custom X-axis variable
	 */
	void drawPlotXY(std::shared_ptr<Plot> plot);

	/**
	 * @brief Handles interactive marker placement and dragging on plots
	 *
	 * @param id Unique identifier for the marker
	 * @param marker Reference to marker state object
	 * @param plotLimits Current plot axis limits
	 * @param activeCallback Callback invoked when marker is being dragged
	 */
	void handleMarkers(uint32_t id, Plot::Marker& marker, ImPlotRect plotLimits, std::function<void()> activeCallback);

	/**
	 * @brief Handles interactive rectangular selection on plots
	 *
	 * @param id Unique identifier for the drag rectangle
	 * @param dragRect Reference to drag rectangle state
	 * @param plotLimits Current plot axis limits
	 */
	void handleDragRect(uint32_t id, Plot::DragRect& dragRect, ImPlotRect plotLimits);

	/**
	 * @brief Handles drag-and-drop of variables onto plot windows
	 *
	 * @param plot Plot that accepts the dropped variables
	 */
	void dragAndDropPlot(std::shared_ptr<Plot> plot);

	/**
	 * @brief Displays a modal question box with Yes/No/Cancel buttons
	 *
	 * @param id Unique ID for the popup
	 * @param question Question text to display
	 * @param onYes Callback for Yes button
	 * @param onNo Callback for No button
	 * @param onCancel Callback for Cancel button
	 */
	void showQuestionBox(const char* id, const char* question, std::function<void()> onYes, std::function<void()> onNo, std::function<void()> onCancel);

	/**
	 * @brief Asks user if unsaved project should be saved before exiting
	 *
	 * @param shouldOpenPopup Flag to trigger popup display
	 */
	void askShouldSaveOnExit(bool shouldOpenPopup);

	/**
	 * @brief Asks user if unsaved project should be saved before creating new project
	 *
	 * @param shouldOpenPopup Flag to trigger popup display
	 */
	void askShouldSaveOnNew(bool shouldOpenPopup);

	/**
	 * @brief Saves current project to existing file
	 *
	 * @return true if save succeeded, false otherwise
	 */
	bool saveProject();

	/**
	 * @brief Opens save dialog and saves project to new file
	 *
	 * @return true if save succeeded, false otherwise
	 */
	bool saveProjectAs();

	/**
	 * @brief Shows popup asking user about changing plot data format
	 *
	 * @param text Popup message text
	 * @param plt Plot to modify
	 * @param name Name identifier for the popup
	 */
	void showChangeFormatPopup(const char* text, Plot& plt, const std::string& name);

	/**
	 * @brief Opens file dialog to select ELF debug file
	 *
	 * @return true if file selected and valid, false otherwise
	 */
	bool openElfFile();

	/**
	 * @brief Opens directory chooser for log file location
	 *
	 * @param logDirectory Reference to string that receives selected directory path
	 * @return true if directory selected, false otherwise
	 */
	bool openLogDirectory(std::string& logDirectory);

	/**
	 * @brief Converts project-relative path to absolute filesystem path
	 *
	 * @param projectRelativePath Relative path stored in project file
	 * @return Absolute path resolved relative to project config location
	 */
	std::string convertProjectPathToAbsolute(const std::string& projectRelativePath);

	/**
	 * @brief Checks and handles keyboard shortcuts
	 *
	 * Processes keyboard shortcuts like Ctrl+S for save, Ctrl+O for open, etc.
	 */
	void checkShortcuts();

	/**
	 * @brief Checks if ELF file has been modified since last load
	 *
	 * @return true if ELF file timestamp changed, false otherwise
	 */
	bool checkElfFileChanged();

	/**
	 * @brief Opens a project configuration file
	 *
	 * @param externalPath Optional path to project file (for external file associations)
	 * @return true if project loaded successfully, false otherwise
	 */
	bool openProject(std::string externalPath = "");

	/**
	 * @brief Draws SWO trace viewer settings panel
	 *
	 * Renders settings specific to trace viewer mode
	 */
	void drawSettingsSwo();

	/**
	 * @brief Draws status indicators for SWO trace viewer
	 *
	 * Shows trace buffer status, dropped packets, etc.
	 */
	void drawIndicatorsSwo();

	/**
	 * @brief Draws all SWO trace plots
	 *
	 * Renders trace channel plots in trace viewer mode
	 */
	void drawPlotsSwo();

	/**
	 * @brief Draws a single SWO trace curve plot
	 *
	 * @param plot Pointer to trace plot
	 * @param time Reference to time buffer for X-axis
	 * @param seriesMap Map of series data for the plot
	 * @param first Flag indicating if this is the first plot (for linked axes)
	 */
	void drawPlotCurveSwo(Plot* plot, ScrollingBuffer<double>& time, std::map<std::string, std::shared_ptr<Plot::Series>>& seriesMap, bool first);

	/**
	 * @brief Draws the SWO trace plot tree view
	 *
	 * Displays hierarchical view of trace plots and channels
	 */
	void drawPlotsTreeSwo();

	/**
	 * @brief Opens a URL in the system default web browser
	 *
	 * @param url URL string to open
	 * @return true if browser launched successfully, false otherwise
	 */
	bool openWebsite(const char* url);
};

#endif
