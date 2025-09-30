/**
 * @file GuiPlotEdit.hpp
 * @brief Plot properties editor window for MCUViewer
 *
 * Provides a modal window for editing plot properties including name, type,
 * and X-axis variable selection for XY plots.
 */

#ifndef _GUI_PLOTEDIT_HPP
#define _GUI_PLOTEDIT_HPP

#include "GuiHelper.hpp"
#include "GuiSelectVariable.hpp"
#include "Plot.hpp"
#include "PlotGroupHandler.hpp"
#include "Popup.hpp"
#include "imgui.h"

/**
 * @class PlotEditWindow
 * @brief Modal window for editing plot properties
 *
 * This window allows users to:
 * - Rename plots (with uniqueness validation)
 * - Change plot type (curve, bar, table, XY)
 * - Select X-axis variable for XY plots
 * - View and modify plot configuration
 *
 * @note Changes are applied immediately and synchronized with PlotGroupHandler
 */
class PlotEditWindow
{
   public:
	/**
	 * @brief Constructs plot edit window
	 *
	 * @param plotHandler Handler for plot management
	 * @param plotGroupHandler Handler for plot group management
	 * @param variableHandler Handler for variable management
	 */
	PlotEditWindow(PlotHandler* plotHandler, PlotGroupHandler* plotGroupHandler, VariableHandler* variableHandler) : plotHandler(plotHandler), plotGroupHandler(plotGroupHandler), variableHandler(variableHandler)
	{
		selectVariableWindow = std::make_unique<SelectVariableWindow>(variableHandler, &selection, 1);
	}

	/**
	 * @brief Draws the plot edit modal window
	 *
	 * Renders the modal popup with plot editing controls. Should be called
	 * every frame to handle window display and user input.
	 *
	 * @note Window automatically closes on "Done" button or Escape key
	 * @note Uses ImGui modal popup pattern
	 */
	void draw()
	{
		if (showPlotEditWindow)
			ImGui::OpenPopup("Plot Edit");

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(700 * GuiHelper::contentScale, 500 * GuiHelper::contentScale));
		if (ImGui::BeginPopupModal("Plot Edit", &showPlotEditWindow, 0))
		{
			drawPlotEditSettings();

			const float buttonHeight = 25.0f * GuiHelper::contentScale;
			ImGui::SetCursorPos(ImVec2(0, ImGui::GetWindowSize().y - buttonHeight / 2.0f - ImGui::GetFrameHeightWithSpacing()));

			if (ImGui::Button("Done", ImVec2(-1, buttonHeight)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				showPlotEditWindow = false;
				ImGui::CloseCurrentPopup();
			}

			popup.handle();
			selectVariableWindow->draw();
			ImGui::EndPopup();
		}
	}

	/**
	 * @brief Sets the plot to be edited
	 *
	 * @param plot Shared pointer to the plot whose properties should be edited
	 * @note Call this before showing the window to specify which plot to edit
	 */
	void setPlotToEdit(std::shared_ptr<Plot> plot)
	{
		editedPlot = plot;
	}

	/**
	 * @brief Sets the visibility state of the edit window
	 *
	 * @param state true to show window, false to hide
	 * @note Tracks state changes to set keyboard focus when window opens
	 */
	void setShowPlotEditWindowState(bool state)
	{
		if (showPlotEditWindow != state)
			stateChanged = true;
		showPlotEditWindow = state;
	}

	/**
	 * @brief Draws the plot settings controls
	 *
	 * Renders input fields for:
	 * - Plot name (with uniqueness validation)
	 * - Plot type selection (curve/bar/table/XY)
	 * - X-axis variable selection (for XY plots)
	 *
	 * @note Sets keyboard focus to name field when window first opens
	 * @note Shows error popup if attempting to use duplicate name
	 */
	void drawPlotEditSettings()
	{
		if (editedPlot == nullptr)
			return;

		std::string name = editedPlot->getName();

		ImGui::Dummy(ImVec2(-1, 5));
		GuiHelper::drawCenteredText("Plot");
		ImGui::Separator();

		GuiHelper::drawTextAlignedToSize("name:", alignment);
		ImGui::SameLine();

		if (stateChanged)
		{
			ImGui::SetKeyboardFocusHere(0);
			stateChanged = false;
		}

		ImGui::InputText("##name", &name, ImGuiInputTextFlags_None, NULL, NULL);

		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			if (!plotHandler->checkIfPlotExists(name))
			{
				std::string oldName = editedPlot->getName();
				plotHandler->renamePlot(oldName, name);
				plotGroupHandler->renamePlotInAllGroups(oldName, name);
			}
			else
				popup.show("Error!", "Plot already exists!", 1.5f);
		}

		const char* plotTypes[] = {"curve", "bar", "table", "XY"};
		int32_t typeCombo = (int32_t)editedPlot->getType();
		GuiHelper::drawTextAlignedToSize("type:", alignment);
		ImGui::SameLine();
		if (ImGui::Combo("##combo", &typeCombo, plotTypes, IM_ARRAYSIZE(plotTypes)))
			editedPlot->setType((Plot::Type)typeCombo);

		if (editedPlot->getType() == Plot::Type::XY)
		{
			GuiHelper::drawTextAlignedToSize("X-axis variable:", alignment);
			ImGui::SameLine();

			std::string selectedVariable = "";

			if (selection.empty())
				selectedVariable = editedPlot->getXAxisVariable() ? editedPlot->getXAxisVariable()->getName() : "";
			else
				selectedVariable = *selection.begin();

			ImGui::InputText("##", &selectedVariable, 0, NULL, NULL);
			if (variableHandler->contains(selectedVariable))
				editedPlot->setXAxisVariable(variableHandler->getVariable(selectedVariable).get());
			ImGui::SameLine();
			if (ImGui::Button("select...", ImVec2(65 * GuiHelper::contentScale, 19 * GuiHelper::contentScale)))
				selectVariableWindow->setShowState(true);
		}
	}

   private:
	/**
	 * @brief Text alignment width for form labels
	 *
	 * Number of characters to align label text to for uniform field layout
	 */
	static constexpr size_t alignment = 18;

	/** @brief Flag indicating if window should be displayed */
	bool showPlotEditWindow = false;

	/** @brief Flag indicating window visibility state changed (for focus management) */
	bool stateChanged = false;

	/** @brief Shared pointer to the plot currently being edited */
	std::shared_ptr<Plot> editedPlot = nullptr;

	/** @brief Handler for plot operations */
	PlotHandler* plotHandler;

	/** @brief Handler for plot group operations */
	PlotGroupHandler* plotGroupHandler;

	/** @brief Handler for variable operations */
	VariableHandler* variableHandler;

	/** @brief Popup for error messages (e.g., duplicate name) */
	Popup popup;

	/** @brief Set containing selected variable name(s) for X-axis */
	std::set<std::string> selection;

	/** @brief Variable selection window for choosing X-axis variable */
	std::unique_ptr<SelectVariableWindow> selectVariableWindow;
};

#endif