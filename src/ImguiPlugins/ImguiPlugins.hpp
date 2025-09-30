/**
 * @file ImguiPlugins.hpp
 * @brief Custom ImGui widget extensions for MCUViewer
 *
 * Provides convenience wrappers and custom widgets that extend ImGui
 * functionality with std::string support and additional UI patterns.
 */

#ifndef _IMGUI_PLUGINS_HPP
#define _IMGUI_PLUGINS_HPP
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

/**
 * @namespace ImGui
 * @brief Extended ImGui namespace with custom widget functions
 *
 * These extensions integrate seamlessly with standard ImGui functions
 * and follow the same naming conventions and patterns.
 */
namespace ImGui
{
/**
 * @brief InputText widget with std::string support
 *
 * Wrapper around ImGui::InputText that works directly with std::string
 * instead of char buffers, handling memory management automatically.
 *
 * @param label Widget label (with optional ## for hidden labels)
 * @param str Pointer to std::string to edit
 * @param flags ImGui input text flags (e.g., ImGuiInputTextFlags_EnterReturnsTrue)
 * @param callback Optional callback for text input events
 * @param user_data Optional user data passed to callback
 * @return true if text was modified, false otherwise
 */
bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data);

/**
 * @brief Selectable widget that becomes an editable input when selected
 *
 * Combines ImGui::Selectable and ImGui::InputText behavior - displays as
 * selectable item, becomes editable text field when selected.
 *
 * @param str_id Unique string ID for the widget
 * @param selected Current selection state
 * @param flags ImGui selectable flags
 * @param buf Character buffer for text editing
 * @param buf_size Size of the character buffer
 * @return true if selection state changed, false otherwise
 * @note Useful for inline-editable list items
 */
bool SelectableInput(const char* str_id, bool selected, ImGuiSelectableFlags flags, char* buf, size_t buf_size);

/**
 * @brief Displays a help marker with tooltip on hover
 *
 * Shows a "(?)marker that displays descriptive text in a tooltip when hovered.
 * Commonly used next to settings and input fields for documentation.
 *
 * @param desc Description text to show in tooltip
 * @note Displays as grayed-out text with hover tooltip
 */
void HelpMarker(const char* desc);

/**
 * @brief Combo box widget with std::vector<std::string> support
 *
 * Wrapper around ImGui::Combo that accepts a vector of strings instead
 * of C-style string arrays.
 *
 * @param label Widget label
 * @param current_item Pointer to currently selected item index
 * @param items Vector of string items to display in dropdown
 * @param height_in_items Optional height in items (use -1 for default)
 * @return true if selection changed, false otherwise
 */
bool Combo(const char* label, int* current_item, const std::vector<std::string>& items, int height_in_items = -1);
}  // namespace ImGui
#endif