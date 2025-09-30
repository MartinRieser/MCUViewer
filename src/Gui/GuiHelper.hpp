/**
 * @file GuiHelper.hpp
 * @brief GUI utility functions and helper namespace for MCUViewer
 *
 * Provides common GUI utility functions for text formatting, color definitions,
 * input handling, and dialog operations used throughout the GUI components.
 */

#ifndef _GUI_HELPER_HPP
#define _GUI_HELPER_HPP

#include <cstdint>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "ImguiPlugins.hpp"
#include "imgui.h"

/**
 * @namespace GuiHelper
 * @brief Namespace containing GUI utility functions and shared UI resources
 *
 * This namespace provides:
 * - Common color definitions for UI elements
 * - Text formatting and alignment functions
 * - Number conversion utilities (hex/decimal)
 * - Dialog boxes and popups
 * - Input field helpers
 */
namespace GuiHelper
{
/**
 * @brief Global content scale factor for DPI-aware rendering
 *
 * All GUI sizes should be multiplied by this scale to support high-DPI displays.
 * Default value is 1.0f for standard displays.
 */
static float contentScale = 1.0f;

/** @brief Standard white color for UI elements */
static ImVec4 white = (ImVec4)ImColor::HSV(0.0f, 0.0f, 1.0f);

/** @brief Green color for success/OK states */
static ImVec4 green = (ImVec4)ImColor::HSV(0.365f, 0.94f, 0.37f);

/** @brief Light green color variant */
static ImVec4 greenLight = (ImVec4)ImColor::HSV(0.365f, 0.94f, 0.57f);

/** @brief Dimmed light green color variant */
static ImVec4 greenLightDim = (ImVec4)ImColor::HSV(0.365f, 0.94f, 0.47f);

/** @brief Red color for errors/warnings */
static ImVec4 red = (ImVec4)ImColor::HSV(0.0f, 0.95f, 0.72f);

/** @brief Light red color variant */
static ImVec4 redLight = (ImVec4)ImColor::HSV(0.0f, 0.95f, 0.92f);

/** @brief Dimmed light red color variant */
static ImVec4 redLightDim = (ImVec4)ImColor::HSV(0.0f, 0.95f, 0.82f);

/** @brief Orange color for warnings/attention states */
static ImVec4 orange = (ImVec4)ImColor::HSV(0.116f, 0.97f, 0.72f);

/** @brief Light orange color variant */
static ImVec4 orangeLight = (ImVec4)ImColor::HSV(0.116f, 0.97f, 0.92f);

/** @brief Dimmed light orange color variant */
static ImVec4 orangeLightDim = (ImVec4)ImColor::HSV(0.116f, 0.97f, 0.82f);

/**
 * @brief Converts a 32-bit unsigned integer to hexadecimal string
 *
 * @param var Integer value to convert
 * @return Hexadecimal string representation (uppercase, without "0x" prefix)
 */
std::string intToHexString(uint32_t var);

/**
 * @brief Draws text centered horizontally in the current window
 *
 * @param text Text string to display (passed as rvalue reference)
 * @note Uses ImGui::CalcTextSize to compute centering offset
 */
void drawCenteredText(std::string&& text);

/**
 * @brief Draws text aligned to a specific character width
 *
 * Used to align labels in front of input fields to create uniform column layout.
 *
 * @param text Text string to display
 * @param alignTo Minimum character width to align to (text is padded with spaces)
 * @note Useful for creating aligned form layouts
 */
void drawTextAlignedToSize(std::string&& text, size_t alignTo);

/**
 * @brief Parses hexadecimal string to unsigned 32-bit integer
 *
 * @param hexStr Hexadecimal string (with or without "0x" prefix)
 * @return Parsed decimal value
 * @note Handles both "0x" prefixed and raw hex strings
 */
uint32_t hexStringToDecimal(const std::string& hexStr);

/**
 * @brief Converts project-relative path to absolute filesystem path
 *
 * @param relativePath Pointer to relative path string from project config
 * @param projectConfigPath Pointer to project configuration file path
 * @return Absolute path resolved relative to project config directory
 */
std::string convertProjectPathToAbsolute(const std::string* relativePath, std::string* projectConfigPath);

/**
 * @brief Shows a delete confirmation popup for an item
 *
 * @param text Button label text
 * @param name Name of item to delete (shown in confirmation)
 * @return Optional string containing item name if delete confirmed, std::nullopt otherwise
 * @note Uses ImGui context menu pattern
 */
std::optional<std::string> showDeletePopup(const char* text, const std::string& name);

/**
 * @brief Displays modal question dialog with Yes/No/Cancel buttons
 *
 * @param id Unique identifier for the popup window
 * @param question Question text to display to user
 * @param onYes Callback function invoked when Yes is clicked
 * @param onNo Callback function invoked when No is clicked
 * @param onCancel Callback function invoked when Cancel is clicked
 */
void showQuestionBox(const char* id, const char* question, std::function<void()> onYes, std::function<void()> onNo, std::function<void()> onCancel);

/**
 * @brief Template function to draw an input text field for numeric values
 *
 * Converts the variable to string, shows an input field, and calls callback on changes.
 *
 * @tparam T Numeric type that can be converted to/from string
 * @param id Unique ImGui ID for the input field
 * @param variable Current value of the variable
 * @param valueChanged Callback invoked with new string value when field is edited
 * @note Triggers on Enter key or when clicking outside the field
 */
template <typename T>
void drawInputText(const char* id, T variable, std::function<void(std::string)> valueChanged)
{
	std::string str = std::to_string(variable);
	if (ImGui::InputText(id, &str, ImGuiInputTextFlags_EnterReturnsTrue, NULL, NULL) || ImGui::IsMouseClicked(0))
		if (valueChanged)
			valueChanged(str);
}

/**
 * @brief Template function to draw a description label followed by a formatted number
 *
 * Displays a label and numeric value on the same line with optional unit and threshold coloring.
 *
 * @tparam T Numeric type to display
 * @param description Label text to show before the number
 * @param number Numeric value to display
 * @param unit Optional unit string (e.g., "ms", "Hz") appended after number
 * @param decimalPlaces Number of decimal places for floating point display
 * @param threshold Optional threshold value - if exceeded, uses thresholdExceededColor
 * @param thresholdExceededColor Color to use for description text when threshold exceeded
 * @note Useful for displaying statistics and measurements with warning colors
 */
template <typename T>
void drawDescriptionWithNumber(const char* description, T number, std::string unit = "", size_t decimalPlaces = 5, float threshold = std::nan(""), ImVec4 thresholdExceededColor = {0.0f, 0.0f, 0.0f, 1.0f})
{
	if (threshold != std::nan("") && number > threshold)
		ImGui::TextColored(thresholdExceededColor, "%s", description);
	else
		ImGui::Text("%s", description);
	ImGui::SameLine();
	std::ostringstream formattedNum;
	formattedNum << std::fixed << std::setprecision(decimalPlaces) << number;
	ImGui::Text("%s", (formattedNum.str() + unit).c_str());
}

/**
 * @brief Template function to convert string to numeric value
 *
 * @tparam T Target numeric type
 * @param str String to parse
 * @return Parsed numeric value of type T
 * @note Uses std::istringstream for conversion
 */
template <typename T>
T convertStringToNumber(std::string& str)
{
	T result;
	std::istringstream ss(str);
	ss >> result;
	return result;
}

}  // namespace GuiHelper

#endif