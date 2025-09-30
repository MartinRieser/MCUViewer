/**
 * @file Popup.hpp
 * @brief Timed popup notification system for MCUViewer
 *
 * Provides auto-dismissing modal popup notifications for displaying
 * temporary messages, errors, and warnings to the user.
 */

#ifndef POPUP_HPP_
#define POPUP_HPP_

#include <chrono>
#include <string>

#include "imgui.h"

/**
 * @class Popup
 * @brief Auto-dismissing modal popup notification window
 *
 * This class creates temporary notification popups that automatically
 * close after a specified duration. Used throughout the GUI for:
 * - Error messages
 * - Warning notifications
 * - Success confirmations
 * - Status updates
 *
 * @note Uses ImGui modal popup pattern with automatic timing
 * @note Multiple popups can be active if using different instances
 */
class Popup
{
   public:
	/**
	 * @brief Triggers the popup to display
	 *
	 * @param title Title text displayed in popup header
	 * @param msg Message text displayed in popup body
	 * @param showTime Duration in seconds to display popup before auto-closing
	 * @note If popup is already running, this call is ignored
	 */
	void show(const char* title, const char* msg, float showTime)
	{
		this->title = title;
		this->msg = msg;
		this->showTime = showTime;
		if (running)
			return;
		start = std::chrono::high_resolution_clock::now();
		running = true;
	}

	/**
	 * @brief Handles popup rendering and timing
	 *
	 * Must be called every frame to render the popup and handle auto-dismissal.
	 * Checks elapsed time and closes popup when duration expires.
	 *
	 * @note Uses ImGui::BeginPopupModal for rendering
	 * @note Popup auto-centers on screen
	 */
	void handle()
	{
		auto popupTimer = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

		if (running)
			ImGui::OpenPopup(title.c_str());

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("%s", msg.c_str());

			if (popupTimer.count() / 1000.0f >= showTime)
			{
				ImGui::CloseCurrentPopup();
				running = false;
			}

			ImGui::EndPopup();
		}
	}

   private:
	/** @brief Start time of popup display for duration tracking */
	std::chrono::time_point<std::chrono::high_resolution_clock> start;

	/** @brief Title text displayed in popup header */
	std::string title;

	/** @brief Message text displayed in popup body */
	std::string msg;

	/** @brief Duration in seconds to display popup */
	float showTime = 0;

	/** @brief Flag indicating if popup is currently active */
	bool running = false;
};

#endif