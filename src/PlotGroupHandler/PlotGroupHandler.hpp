/**
 * @file PlotGroupHandler.hpp
 * @brief Plot group management system for MCUViewer
 *
 * Provides hierarchical organization of plots into groups with individual
 * visibility control. Allows multiple views of the same plots.
 */

#pragma once

#include <algorithm>
#include <map>
#include <string>

#include "Plot.hpp"

/**
 * @class PlotGroup
 * @brief Represents a named group of plots with visibility control
 *
 * A PlotGroup organizes plots into a named collection where each plot
 * can be individually shown or hidden. Multiple groups can reference
 * the same plot objects, allowing different views of the data.
 *
 * Features:
 * - Add/remove plots to/from group
 * - Toggle plot visibility within group
 * - Rename plots within group
 * - Count visible plots
 * - Iterate over group members
 *
 * @note Groups do not own plots; they hold shared pointers
 */
class PlotGroup
{
   public:
	/**
	 * @struct PlotEntry
	 * @brief Entry in plot group combining plot reference and visibility
	 */
	struct PlotEntry
	{
		bool visibility = true;              /**< Visibility flag for this plot in this group */
		std::shared_ptr<Plot> plot;          /**< Shared pointer to plot object */
	};

	/**
	 * @brief Constructs a new plot group
	 *
	 * @param name Unique name for the group
	 */
	PlotGroup(const std::string& name) : name(name)
	{
	}

	/**
	 * @brief Adds a plot to this group
	 *
	 * @param plot Shared pointer to plot to add
	 * @param visibility Initial visibility state (default: true)
	 * @note If plot already exists in group, it is replaced
	 */
	void addPlot(std::shared_ptr<Plot> plot, bool visibility = true)
	{
		group[plot->getName()] = {visibility, plot};
	}

	/**
	 * @brief Removes a plot from this group
	 *
	 * @param name Name of plot to remove
	 * @note Does not delete the plot object, only removes from group
	 */
	void removePlot(const std::string& name)
	{
		group.erase(name);
	}

	/**
	 * @brief Sets visibility of a plot in this group
	 *
	 * @param name Name of plot
	 * @param visible New visibility state
	 */
	void setVisibility(const std::string& name, bool visible)
	{
		group.at(name).visibility = visible;
	}

	/**
	 * @brief Gets visibility of a plot in this group
	 *
	 * @param name Name of plot
	 * @return Current visibility state
	 */
	bool getVisibility(const std::string& name) const
	{
		return group.at(name).visibility;
	}

	/**
	 * @brief Sets the name of this group
	 *
	 * @param name New name for the group
	 */
	void setName(const std::string& name)
	{
		this->name = name;
	}

	/**
	 * @brief Gets the name of this group
	 *
	 * @return Group name string
	 */
	std::string getName() const
	{
		return name;
	}

	/**
	 * @brief Renames a plot within this group
	 *
	 * @param oldName Current plot name
	 * @param newName New plot name
	 * @return true if rename succeeded, false if plot not in group
	 * @note Updates the key in the internal map
	 */
	bool renamePlot(const std::string& oldName, const std::string& newName)
	{
		if (!group.contains(oldName))
			return false;

		auto groupElement = group.extract(oldName);
		groupElement.key() = newName;
		group.insert(std::move(groupElement));
		return true;
	}

	/**
	 * @brief Returns const iterator to first plot in group
	 * @return Const iterator to beginning
	 */
	std::map<std::string, PlotEntry>::const_iterator begin() const
	{
		return group.cbegin();
	}

	/**
	 * @brief Returns const iterator past last plot in group
	 * @return Const iterator to end
	 */
	std::map<std::string, PlotEntry>::const_iterator end() const
	{
		return group.cend();
	}

	/**
	 * @brief Counts how many plots in group are currently visible
	 *
	 * @return Number of visible plots
	 */
	uint32_t getVisiblePlotsCount() const
	{
		return std::count_if(group.begin(), group.end(), [](const auto& pair)
							 { return pair.second.visibility; });
	}

   private:
	/** @brief Name of this plot group */
	std::string name;

	/** @brief Map of plot entries keyed by plot name */
	std::map<std::string, PlotEntry> group;
};

/**
 * @class PlotGroupHandler
 * @brief Manages collection of PlotGroup objects
 *
 * Provides centralized management of all plot groups with support for:
 * - Creating and deleting groups
 * - Renaming groups
 * - Setting active group for GUI display
 * - Propagating plot renames across all groups
 * - Iterating over all groups
 *
 * @note Ensures at least one group always exists
 * @note Tracks which group is currently active for GUI
 */
class PlotGroupHandler
{
   public:
	/**
	 * @brief Creates and adds a new group
	 *
	 * @param name Unique name for the new group
	 * @return Shared pointer to created group
	 */
	std::shared_ptr<PlotGroup> addGroup(const std::string& name)
	{
		groupMap[name] = std::make_shared<PlotGroup>(name);
		return groupMap[name];
	}

	/** @brief Renames a group */
	void renameGroup(const std::string& oldName, const std::string& newName)
	{
		auto group = groupMap.extract(oldName);
		group.key() = newName;
		groupMap.insert(std::move(group));
		groupMap[newName]->setName(newName);
	}

	/** @brief Removes a group (ensures at least one group remains) */
	void removeGroup(const std::string& name)
	{
		groupMap.erase(name);

		if (groupMap.size() == 0)
			addGroup("new group0");

		activeGroup = groupMap.begin()->first;
	}

	/** @brief Removes all groups from the handler */
	void removeAllGroups()
	{
		groupMap.clear();
	}

	/** @brief Renames a plot in all groups that contain it */
	bool renamePlotInAllGroups(const std::string& oldName, const std::string& newName)
	{
		for (auto& [name, group] : groupMap)
		{
			group->renamePlot(oldName, newName);
		}
		return true;
	}

	/** @brief Gets total number of groups */
	size_t getGroupCount()
	{
		return groupMap.size();
	}

	/** @brief Retrieves a group by name */
	std::shared_ptr<PlotGroup> getGroup(const std::string& name)
	{
		return groupMap.at(name);
	}

	/** @brief Returns const iterator to first group */
	std::map<std::string, std::shared_ptr<PlotGroup>>::const_iterator begin() const
	{
		return groupMap.cbegin();
	}

	/** @brief Returns const iterator past last group */
	std::map<std::string, std::shared_ptr<PlotGroup>>::const_iterator end() const
	{
		return groupMap.cend();
	}

	/** @brief Sets which group is currently active in GUI */
	void setActiveGroup(const std::string& name)
	{
		activeGroup = name;
	}

	/** @brief Gets the currently active group */
	std::shared_ptr<PlotGroup> getActiveGroup()
	{
		if (!groupMap.contains(activeGroup))
			activeGroup = groupMap.begin()->first;
		return groupMap.at(activeGroup);
	}

	/** @brief Checks if a group with given name exists */
	bool checkIfGroupExists(const std::string& name) const
	{
		return groupMap.find(name) != groupMap.end();
	}

   private:
	/** @brief Name of currently active group */
	std::string activeGroup = "";

	/** @brief Map of all groups keyed by name */
	std::map<std::string, std::shared_ptr<PlotGroup>> groupMap;
};