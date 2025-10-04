#ifndef _GUIRECORDERCONTROL_HPP
#define _GUIRECORDERCONTROL_HPP

#include <memory>
#include <vector>

#include "RecorderModule.hpp"
#include "VariableHandler.hpp"
#include "imgui.h"

class RecorderControlWindow
{
   public:
	void draw(std::shared_ptr<RecorderModule> recorder, VariableHandler* variableHandler)
	{
		if (!ImGui::Begin("Recorder Control"))
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

		// Get current state
		RecorderState state = recorder->getState();
		RecorderConfig config = recorder->getConfig();
		TriggerConfig trigger = recorder->getTriggerConfig();

		// Draw status indicator
		drawStatus(state);

		ImGui::Separator();

		// Configuration section (disabled when armed/triggered)
		bool canConfigure = (state == RecorderState::IDLE || state == RecorderState::CONFIGURED);

		if (!canConfigure)
			ImGui::BeginDisabled();

		ImGui::Text("Configuration");

		// Buffer size
		ImGui::Text("Buffer Size:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150);
		int bufferSamples = static_cast<int>(config.bufferSamples);
		if (ImGui::InputInt("##buffersize", &bufferSamples, 100, 1000))
		{
			config.bufferSamples = std::max(100, std::min(100000, bufferSamples));
			recorder->configure(config);
		}

		// Sample rate
		ImGui::Text("Sample Rate:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150);
		int sampleRate = static_cast<int>(config.sampleRateHz);
		if (ImGui::InputInt("##samplerate", &sampleRate, 10, 100))
		{
			config.sampleRateHz = std::max(1, std::min(10000, sampleRate));
			recorder->configure(config);
		}
		ImGui::SameLine();
		ImGui::Text("Hz");

		// Pre-trigger samples (absolute count)
		ImGui::Text("Pre-Trigger:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150);
		int preTrigger = static_cast<int>(trigger.preTriggerSamples);
		if (ImGui::InputInt("##pretrigger", &preTrigger, 10, 100))
		{
			trigger.preTriggerSamples = std::max(0, std::min(static_cast<int>(config.bufferSamples), preTrigger));
			recorder->setupTrigger(trigger);
		}
		ImGui::SameLine();
		ImGui::Text("samples");

		ImGui::Separator();

		// Variable selection
		ImGui::Text("Variables to Record:");

		if (ImGui::BeginListBox("##variables", ImVec2(-1, 150)))
		{
			for (auto var : *variableHandler)
			{
				uint32_t address = var->getAddress();
				bool isSelected = std::find(config.addresses.begin(), config.addresses.end(), address) != config.addresses.end();

				if (ImGui::Selectable(var->getName().c_str(), isSelected))
				{
					if (isSelected)
					{
						// Remove variable
						auto it = std::find(config.addresses.begin(), config.addresses.end(), address);
						if (it != config.addresses.end())
						{
							size_t index = it - config.addresses.begin();
							config.addresses.erase(config.addresses.begin() + index);
							config.sizes.erase(config.sizes.begin() + index);
						}
					}
					else
					{
						// Add variable
						config.addresses.push_back(address);
						config.sizes.push_back(var->getSize());
					}
					recorder->configure(config);
				}
			}
			ImGui::EndListBox();
		}

		ImGui::Separator();

		// Trigger configuration
		ImGui::Text("Trigger Configuration");

		const char* triggerTypes[] = {"None (Free-Running)", "Edge", "Window", "Logic"};
		int triggerType = static_cast<int>(trigger.type);
		if (ImGui::Combo("Trigger Type", &triggerType, triggerTypes, 4))
		{
			trigger.type = static_cast<TriggerType>(triggerType);
			recorder->setupTrigger(trigger);
		}

		if (trigger.type != TriggerType::NONE)
		{
			// Variable selection for trigger
			ImGui::Text("Trigger Variable:");
			if (ImGui::BeginCombo("##trigvar", getTriggerVariableName(variableHandler, trigger.varAddress).c_str()))
			{
				for (auto var : *variableHandler)
				{
					bool isSelected = (var->getAddress() == trigger.varAddress);
					if (ImGui::Selectable(var->getName().c_str(), isSelected))
					{
						trigger.varAddress = var->getAddress();
						recorder->setupTrigger(trigger);
					}
				}
				ImGui::EndCombo();
			}

			// Trigger-type-specific parameters
			if (trigger.type == TriggerType::EDGE)
			{
				const char* edgeTypes[] = {"Rising Edge", "Falling Edge", "Both Edges"};
				int edgeType = static_cast<int>(trigger.condition);
				if (ImGui::Combo("Edge Type", &edgeType, edgeTypes, 3))
				{
					trigger.condition = edgeType;
					recorder->setupTrigger(trigger);
				}

				ImGui::Text("Threshold:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150);
				if (ImGui::InputDouble("##threshold", &trigger.value1))
					recorder->setupTrigger(trigger);
			}
			else if (trigger.type == TriggerType::WINDOW)
			{
				const char* windowTypes[] = {"Inside", "Outside"};
				int windowType = static_cast<int>(trigger.condition);
				if (ImGui::Combo("Condition", &windowType, windowTypes, 2))
				{
					trigger.condition = windowType;
					recorder->setupTrigger(trigger);
				}

				ImGui::Text("Lower Bound:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150);
				if (ImGui::InputDouble("##lower", &trigger.value1))
					recorder->setupTrigger(trigger);

				ImGui::Text("Upper Bound:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150);
				if (ImGui::InputDouble("##upper", &trigger.value2))
					recorder->setupTrigger(trigger);
			}

			// Hysteresis (for all trigger types)
			ImGui::Text("Hysteresis:");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(150);
			if (ImGui::InputDouble("##hysteresis", &trigger.hysteresis))
				recorder->setupTrigger(trigger);
		}

		if (!canConfigure)
			ImGui::EndDisabled();

		ImGui::Separator();

		// Control buttons
		drawControlButtons(recorder, state);

		ImGui::End();
	}

   private:
	void drawStatus(RecorderState state)
	{
		ImGui::Text("Status:");
		ImGui::SameLine();

		switch (state)
		{
			case RecorderState::IDLE:
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "IDLE");
				break;
			case RecorderState::CONFIGURED:
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "CONFIGURED");
				break;
			case RecorderState::ARMED:
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "ARMED - Waiting for Trigger");
				break;
			case RecorderState::TRIGGERED:
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "TRIGGERED - Recording");
				break;
			case RecorderState::READY:
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "READY - Data Available");
				break;
			case RecorderState::RECORDER_ERROR:
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ERROR");
				break;
		}
	}

	void drawControlButtons(std::shared_ptr<RecorderModule> recorder, RecorderState state)
	{
		if (state == RecorderState::IDLE || state == RecorderState::CONFIGURED || state == RecorderState::READY)
		{
			if (ImGui::Button("Arm Trigger", ImVec2(120, 30)))
			{
				recorder->arm(TriggerMode::SINGLE_SHOT);
			}
			ImGui::SameLine();
			if (ImGui::Button("Auto-Rearm", ImVec2(120, 30)))
			{
				recorder->arm(TriggerMode::AUTO_REARM);
			}
		}
		else if (state == RecorderState::ARMED || state == RecorderState::TRIGGERED)
		{
			if (ImGui::Button("Force Trigger", ImVec2(120, 30)))
			{
				recorder->forceTrigger();
			}
			ImGui::SameLine();
			if (ImGui::Button("Disarm", ImVec2(120, 30)))
			{
				recorder->disarm();
			}
		}

		if (state == RecorderState::READY)
		{
			ImGui::SameLine();
			if (ImGui::Button("Reset", ImVec2(120, 30)))
			{
				recorder->reset();
			}
		}

		// Statistics display
		if (state == RecorderState::READY)
		{
			ImGui::Separator();
			ImGui::Text("Recording Statistics:");

			RecorderStats stats = recorder->getStats();
			ImGui::Text("Total Samples:       %u", stats.totalSamples);
			ImGui::Text("Pre-Trigger Samples: %u", stats.preTriggerSamples);
			ImGui::Text("Post-Trigger Samples:%u", stats.postTriggerSamples);
			ImGui::Text("Trigger Index:       %u", stats.triggerIndex);
			ImGui::Text("Trigger Time:        %.3f s", stats.triggerTimestamp);
			ImGui::Text("Duration:            %.3f s", stats.lastTimestamp - stats.firstTimestamp);
		}
	}

	std::string getTriggerVariableName(VariableHandler* variableHandler, uint32_t address)
	{
		for (auto var : *variableHandler)
		{
			if (var->getAddress() == address)
				return var->getName();
		}
		return "Select Variable";
	}
};

#endif // _GUIRECORDERCONTROL_HPP
