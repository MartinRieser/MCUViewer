/**
 * @file GuiGroupEdit.hpp
 * @brief Plot group properties editor window for MCUViewer
 *
 * Provides a modal window for editing plot group properties,
 * primarily the group name with uniqueness validation.
 */

#pragma once

#include "GuiHelper.hpp"
#include "Plot.hpp"
#include "PlotGroupHandler.hpp"
#include "Popup.hpp"
#include "imgui.h"

/**
 * @class GroupEditWindow
 * @brief Modal window for editing plot group properties
 *
 * This window allows users to:
 * - Rename plot groups (with uniqueness validation)
 * - View group configuration
 *
 * @note Changes are applied immediately to PlotGroupHandler
 * @note Shows error popup for duplicate names
 */
class GroupEditWindow
{
   public:
	/**
	 * @brief Constructs group edit window
	 *
	 * @param plotGroupHandler Handler for plot group management
	 */
	GroupEditWindow(PlotGroupHandler* plotGroupHandler) : plotGroupHandler(plotGroupHandler)
	{
	}

	/**
	 * @brief Draws the group edit modal window
	 *
	 * Renders modal popup with group editing controls. Should be called
	 * every frame to handle window display and user input.
	 *
	 * @note Window automatically closes on "Done" button or Escape key
	 */
	void draw()
	{
		if (showGroupEditWindow)
			ImGui::OpenPopup("Group Edit");

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(700 * GuiHelper::contentScale, 500 * GuiHelper::contentScale));
		if (ImGui::BeginPopupModal("Group Edit", &showGroupEditWindow, 0))
		{
			drawGroupEditSettings();

			const float buttonHeight = 25.0f * GuiHelper::contentScale;
			ImGui::SetCursorPos(ImVec2(0, ImGui::GetWindowSize().y - buttonHeight / 2.0f - ImGui::GetFrameHeightWithSpacing()));

			if (ImGui::Button("Done", ImVec2(-1, buttonHeight)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
			{
				showGroupEditWindow = false;
				ImGui::CloseCurrentPopup();
			}

			popup.handle();
			ImGui::EndPopup();
		}
	}

	/**
	 * @brief Sets the group to be edited
	 *
	 * @param group Shared pointer to the plot group to edit
	 * @note Call before showing window to specify which group to edit
	 */
	void setGroupToEdit(std::shared_ptr<PlotGroup> group)
	{
		editedGroup = group;
	}

	/**
	 * @brief Sets the visibility state of the edit window
	 *
	 * @param state true to show window, false to hide
	 * @note Tracks state changes for keyboard focus management
	 */
	void setShowGroupEditWindowState(bool state)
	{
		if (showGroupEditWindow != state)
			stateChanged = true;
		showGroupEditWindow = state;
	}

	/**
	 * @brief Draws the group settings controls
	 *
	 * Renders input field for group name with uniqueness validation.
	 * Sets keyboard focus to name field when window first opens.
	 *
	 * @note Shows error popup for duplicate names
	 */
	void drawGroupEditSettings()
	{
		if (editedGroup == nullptr)
			return;

		std::string name = editedGroup->getName();

		ImGui::Dummy(ImVec2(-1, 5));
		GuiHelper::drawCenteredText("Group");
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
			if (!plotGroupHandler->checkIfGroupExists(name))
			{
				std::string oldName = editedGroup->getName();
				plotGroupHandler->renameGroup(oldName, name);
			}

			else
				popup.show("Error!", "Group already exists!", 1.5f);
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
	bool showGroupEditWindow = false;

	/** @brief Handler for plot group operations */
	PlotGroupHandler* plotGroupHandler;

	/** @brief Shared pointer to the group currently being edited */
	std::shared_ptr<PlotGroup> editedGroup = nullptr;

	/** @brief Popup for error messages (e.g., duplicate name) */
	Popup popup;

	/** @brief Flag indicating window state changed (for focus management) */
	bool stateChanged = false;
};
