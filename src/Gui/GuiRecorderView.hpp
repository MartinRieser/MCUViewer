#ifndef _GUIRECORDERVIEW_HPP
#define _GUIRECORDERVIEW_HPP

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "RecorderModule.hpp"
#include "VariableHandler.hpp"
#include "imgui.h"
#include "implot.h"

class RecorderViewWindow
{
   public:
	RecorderViewWindow() : cursor1Enabled(false), cursor2Enabled(false), cursor1Time(0.0), cursor2Time(0.0)
	{
	}

	void draw(std::shared_ptr<RecorderModule> recorder, VariableHandler* variableHandler)
	{
		if (!ImGui::Begin("Recorder View"))
		{
			ImGui::End();
			return;
		}

		if (!recorder)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No recorder module set");
			ImGui::End();
			return;
		}

		RecorderState state = recorder->getState();

		if (state != RecorderState::READY)
		{
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No recording available. Arm trigger and wait for capture.");
			ImGui::End();
			return;
		}

		// Get recorded data
		std::vector<RecorderSample> data = recorder->getAllData();
		RecorderStats stats = recorder->getStats();

		if (data.empty())
		{
			ImGui::Text("No data captured");
			ImGui::End();
			return;
		}

		// Extract variable addresses
		std::vector<uint32_t> addresses;
		if (!data.empty() && !data[0].values.empty())
		{
			for (const auto& [addr, value] : data[0].values)
				addresses.push_back(addr);
		}

		// Variable visibility toggles
		ImGui::Text("Visible Variables:");
		ImGui::SameLine();

		for (uint32_t addr : addresses)
		{
			bool visible = (visibleVariables.find(addr) != visibleVariables.end());
			std::string varName = getVariableName(variableHandler, addr);

			if (ImGui::Checkbox(varName.c_str(), &visible))
			{
				if (visible)
					visibleVariables.insert(addr);
				else
					visibleVariables.erase(addr);
			}
			ImGui::SameLine();
		}

		ImGui::NewLine();

		// Cursor controls
		ImGui::Checkbox("Cursor 1", &cursor1Enabled);
		ImGui::SameLine();
		ImGui::Checkbox("Cursor 2", &cursor2Enabled);

		if (cursor1Enabled && cursor2Enabled)
		{
			ImGui::SameLine();
			ImGui::Text("| ΔT = %.6f s", std::abs(cursor2Time - cursor1Time));
		}

		// Export button
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 120);
		if (ImGui::Button("Export CSV", ImVec2(110, 0)))
			exportToCSV(data, stats, variableHandler);

		ImGui::Separator();

		// Calculate time axis (relative to trigger)
		std::vector<double> timeAxis;
		timeAxis.reserve(data.size());

		double triggerTime = stats.triggerTimestamp;
		for (const auto& sample : data)
			timeAxis.push_back(sample.timestamp - triggerTime);

		// Plot waveforms
		if (ImPlot::BeginPlot("##RecorderPlot", ImVec2(-1, -1), ImPlotFlags_NoChild))
		{
			// Setup axes
			ImPlot::SetupAxis(ImAxis_X1, "Time (s, relative to trigger)", ImPlotAxisFlags_None);
			ImPlot::SetupAxis(ImAxis_Y1, "Value", ImPlotAxisFlags_None);

			// Auto-fit on first display
			if (firstDraw)
			{
				ImPlot::SetupAxisLimits(ImAxis_X1, timeAxis.front(), timeAxis.back(), ImPlotCond_Once);
				firstDraw = false;
			}

			// Draw pre/post-trigger region shading
			drawTriggerRegions(timeAxis, stats);

			// Draw trigger marker
			drawTriggerMarker();

			// Plot each variable
			for (uint32_t addr : addresses)
			{
				if (visibleVariables.find(addr) == visibleVariables.end())
					continue;

				std::string varName = getVariableName(variableHandler, addr);

				// Extract values for this variable
				std::vector<double> values;
				values.reserve(data.size());
				for (const auto& sample : data)
				{
					auto it = sample.values.find(addr);
					if (it != sample.values.end())
						values.push_back(it->second);
					else
						values.push_back(0.0);
				}

				// Get variable color
				ImVec4 color = getVariableColor(variableHandler, addr);
				ImPlot::SetNextLineStyle(color);

				// Plot line
				if (!values.empty())
					ImPlot::PlotLine(varName.c_str(), timeAxis.data(), values.data(), timeAxis.size());
			}

			// Draw cursors
			if (cursor1Enabled)
			{
				if (ImPlot::DragLineX(1, &cursor1Time, ImVec4(1, 1, 0, 1), 1, ImPlotDragToolFlags_None))
				{
					// Cursor dragged
				}
			}

			if (cursor2Enabled)
			{
				if (ImPlot::DragLineX(2, &cursor2Time, ImVec4(0, 1, 1, 1), 1, ImPlotDragToolFlags_None))
				{
					// Cursor dragged
				}
			}

			ImPlot::EndPlot();
		}

		ImGui::End();
	}

   private:
	void drawTriggerRegions(const std::vector<double>& timeAxis, const RecorderStats& stats)
	{
		if (timeAxis.empty())
			return;

		// Pre-trigger region (shaded green)
		double preStart = timeAxis.front();
		double preEnd = 0.0; // Trigger is at t=0
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.15f);
		ImPlot::PlotShaded("Pre-Trigger", &preStart, &preEnd, 1);
		ImPlot::PopStyleVar();

		// Post-trigger region (shaded blue)
		double postStart = 0.0;
		double postEnd = timeAxis.back();
		ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.15f);
		ImPlot::PlotShaded("Post-Trigger", &postStart, &postEnd, 1);
		ImPlot::PopStyleVar();
	}

	void drawTriggerMarker()
	{
		// Vertical line at t=0 (trigger point)
		double triggerTime = 0.0;
		ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2.0f);
		ImPlot::PlotInfLines("Trigger", &triggerTime, 1);
		ImPlot::PopStyleVar();
		ImPlot::PopStyleColor();
	}

	std::string getVariableName(VariableHandler* variableHandler, uint32_t address)
	{
		for (auto var : *variableHandler)
		{
			if (var->getAddress() == address)
				return var->getName();
		}
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "0x%08X", address);
		return std::string(buffer);
	}

	ImVec4 getVariableColor(VariableHandler* variableHandler, uint32_t address)
	{
		for (auto var : *variableHandler)
		{
			if (var->getAddress() == address)
			{
				const auto& color = var->getColor();
				return ImVec4(color.r, color.g, color.b, color.a);
			}
		}
		return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White default
	}

	void exportToCSV(const std::vector<RecorderSample>& data, const RecorderStats& stats, VariableHandler* variableHandler)
	{
		// Generate filename with timestamp
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		char filename[128];
		strftime(filename, sizeof(filename), "recorder_%Y%m%d_%H%M%S.csv", localtime(&time));

		std::ofstream file(filename);
		if (!file.is_open())
		{
			// TODO: Show error popup
			return;
		}

		// Get all addresses
		std::vector<uint32_t> addresses;
		if (!data.empty() && !data[0].values.empty())
		{
			for (const auto& [addr, value] : data[0].values)
				addresses.push_back(addr);
		}

		// Write header
		file << "Time (s),Time Relative to Trigger (s)";
		for (uint32_t addr : addresses)
		{
			std::string varName = getVariableName(variableHandler, addr);
			file << "," << varName;
		}
		file << "\n";

		// Write data
		double triggerTime = stats.triggerTimestamp;
		for (const auto& sample : data)
		{
			file << sample.timestamp << "," << (sample.timestamp - triggerTime);
			for (uint32_t addr : addresses)
			{
				auto it = sample.values.find(addr);
				if (it != sample.values.end())
					file << "," << it->second;
				else
					file << ",";
			}
			file << "\n";
		}

		file.close();

		// Store filename for display
		lastExportFilename = filename;
	}

   private:
	std::set<uint32_t> visibleVariables; // Addresses of visible variables
	bool cursor1Enabled;
	bool cursor2Enabled;
	double cursor1Time;
	double cursor2Time;
	bool firstDraw = true;
	std::string lastExportFilename;
};

#endif // _GUIRECORDERVIEW_HPP
