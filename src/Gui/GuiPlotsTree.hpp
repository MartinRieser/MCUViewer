#pragma once

#include <optional>
#include <string>

#include "GuiGroupEdit.hpp"
#include "GuiHelper.hpp"
#include "GuiPlotEdit.hpp"
#include "GuiStatisticsWindow.hpp"
#include "IFileHandler.hpp"
#include "Plot.hpp"
#include "PlotGroupHandler.hpp"
#include "ViewerDataHandler.hpp"

class Gui;  // Forward declaration

class PlotsTree
{
   public:
	PlotsTree(ViewerDataHandler* viewerDataHandler, PlotHandler* plotHandler, PlotGroupHandler* plotGroupHandler, VariableHandler* variableHandler, std::shared_ptr<PlotEditWindow> plotEditWindow, IFileHandler* fileHandler, spdlog::logger* logger, Gui* gui) : viewerDataHandler(viewerDataHandler), plotHandler(plotHandler), plotGroupHandler(plotGroupHandler), variableHandler(variableHandler), plotEditWindow(plotEditWindow), fileHandler(fileHandler), logger(logger), gui(gui)
	{
		groupEditWindow = std::make_unique<GroupEditWindow>(plotGroupHandler);
	}
	void draw();

	void drawAddPlotButton()
	{
		if (ImGui::Button("Add plot", ImVec2(-1, 25 * GuiHelper::contentScale)))
			addNewPlot();

		if (ImGui::Button("Add group", ImVec2(-1, 25 * GuiHelper::contentScale)))
			addNewGroup();
	}

	void addNewPlot();
	void addNewGroup();
	void drawExportPlotToCSVButton(std::shared_ptr<Plot> plt);

   private:
	void drawMenuGroupPopup(const std::string& name, std::function<void()> onNewGroup, std::function<void()> onNewPlot, std::function<void(const std::string&)> onDelete, std::function<void(const std::string&)> onProperties);
	void drawMenuPlotPopup(const std::string& name, std::function<void()> onNewPlot, std::function<void(const std::string&)> onDelete, std::function<void(const std::string&)> onProperties);

   private:
	ViewerDataHandler* viewerDataHandler;
	PlotHandler* plotHandler;
	PlotGroupHandler* plotGroupHandler;
	VariableHandler* variableHandler;
	std::shared_ptr<PlotEditWindow> plotEditWindow;

	std::unique_ptr<GroupEditWindow> groupEditWindow;

	StatisticsWindow statisticsWindow;
	IFileHandler* fileHandler;
	spdlog::logger* logger;
	Gui* gui;
};