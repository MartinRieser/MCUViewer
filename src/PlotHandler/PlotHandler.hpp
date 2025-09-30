/**
 * @file PlotHandler.hpp
 * @brief Plot management and storage handler for MCUViewer
 *
 * Manages the collection of plots in the application, providing CRUD operations
 * and iteration support. Maintains a map of all plot objects and their data.
 */

#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <thread>

#include "Plot.hpp"
#include "ScrollingBuffer.hpp"

/**
 * @class PlotHandler
 * @brief Manages collection of Plot objects with CRUD operations
 *
 * This class provides:
 * - Plot creation, deletion, and renaming
 * - Plot lookup by name
 * - Plot data management (clearing, max points)
 * - Iteration over all plots
 * - Statistics on plot count and visibility
 *
 * @note All plots are identified by unique names
 * @note Provides custom iterator for range-based loops
 */
class PlotHandler
{
   public:
	/**
	 * @brief Creates and adds a new plot to the handler
	 *
	 * @param name Unique name for the new plot
	 * @return Shared pointer to the created plot
	 * @note Name must be unique; no validation performed by this method
	 */
	std::shared_ptr<Plot> addPlot(const std::string& name);

	/**
	 * @brief Removes a plot from the handler
	 *
	 * @param name Name of plot to remove
	 * @return true if plot was found and removed, false otherwise
	 */
	bool removePlot(const std::string& name);

	/**
	 * @brief Renames an existing plot
	 *
	 * @param oldName Current name of the plot
	 * @param newName New name for the plot
	 * @return true if rename succeeded, false if plot not found
	 * @note Updates the plot object's internal name as well
	 */
	bool renamePlot(const std::string& oldName, const std::string& newName);

	/**
	 * @brief Removes all plots from the handler
	 *
	 * @return true (always succeeds)
	 */
	bool removeAllPlots();

	/**
	 * @brief Retrieves a plot by name
	 *
	 * @param name Name of plot to retrieve
	 * @return Shared pointer to plot, or nullptr if not found
	 */
	std::shared_ptr<Plot> getPlot(std::string name);

	/**
	 * @brief Clears data from all plots while keeping plot structures
	 *
	 * @return true if successful
	 * @note Preserves plot configuration, only clears time-series data
	 */
	bool eraseAllPlotData();

	/**
	 * @brief Gets count of visible plots across all groups
	 *
	 * @return Number of plots currently set as visible
	 */
	uint32_t getVisiblePlotsCount() const;

	/**
	 * @brief Gets total number of plots
	 *
	 * @return Total count of plots in handler
	 */
	uint32_t getPlotsCount() const;

	/**
	 * @brief Checks if a plot with given name exists
	 *
	 * @param name Plot name to check
	 * @return true if plot exists, false otherwise
	 */
	bool checkIfPlotExists(const std::string& name) const;

	/**
	 * @brief Sets maximum number of data points for all plots
	 *
	 * @param maxPoints Maximum data points to retain per plot
	 * @note Older points are discarded when limit is reached (circular buffer)
	 */
	void setMaxPoints(uint32_t maxPoints);

	/**
	 * @class iterator
	 * @brief Custom iterator for iterating over Plot objects
	 *
	 * Allows range-based for loops over plot handler:
	 * @code
	 * for (std::shared_ptr<Plot> plot : plotHandler) { ... }
	 * @endcode
	 *
	 * @note Iterates over plot objects, not map entries
	 */
	class iterator
	{
	   public:
		/** @brief Standard iterator category tag */
		using iterator_category = std::forward_iterator_tag;

		/**
		 * @brief Constructs iterator from map iterator
		 * @param iter Map iterator to wrap
		 */
		explicit iterator(std::map<std::string, std::shared_ptr<Plot>>::iterator iter);

		/** @brief Pre-increment operator */
		iterator& operator++();

		/** @brief Post-increment operator */
		iterator operator++(int);

		/**
		 * @brief Equality comparison operator
		 * @param other Iterator to compare with
		 * @return true if iterators point to same element
		 */
		bool operator==(const iterator& other) const;

		/**
		 * @brief Inequality comparison operator
		 * @param other Iterator to compare with
		 * @return true if iterators point to different elements
		 */
		bool operator!=(const iterator& other) const;

		/**
		 * @brief Dereference operator
		 * @return Shared pointer to current Plot object
		 */
		std::shared_ptr<Plot> operator*();

	   private:
		/** @brief Underlying map iterator */
		std::map<std::string, std::shared_ptr<Plot>>::iterator m_iter;
	};

	/**
	 * @brief Returns iterator to first plot
	 * @return Iterator to beginning of plot collection
	 */
	iterator begin();

	/**
	 * @brief Returns iterator past last plot
	 * @return Iterator to end of plot collection
	 */
	iterator end();

   protected:
	/** @brief Map storing all plots keyed by name */
	std::map<std::string, std::shared_ptr<Plot>> plotsMap;
};
