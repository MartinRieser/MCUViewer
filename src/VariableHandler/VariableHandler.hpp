/**
 * @file VariableHandler.hpp
 * @brief Variable management and storage handler for MCUViewer
 *
 * Manages the collection of variables that can be sampled from target memory,
 * providing CRUD operations, iteration, and rename notifications.
 */

#pragma once

#include <map>
#include <memory>
#include <random>

#include "Variable.hpp"

/**
 * @class VariableHandler
 * @brief Manages collection of Variable objects with CRUD operations
 *
 * This class provides:
 * - Variable creation, deletion, and renaming
 * - Variable lookup by name
 * - Iteration over all variables
 * - Rename callbacks for synchronization with other components
 *
 * @note All variables are identified by unique names
 * @note Provides custom iterator for range-based loops
 * @note Thread-safe if used with external synchronization
 */
class VariableHandler
{
   public:
	/** @brief Type alias for variable storage map */
	using VariableMap = std::map<std::string, std::shared_ptr<Variable>>;

   public:
	/** @brief Adds an existing variable to the handler */
	void addVariable(std::shared_ptr<Variable> var);

	/** @brief Retrieves a variable by name */
	std::shared_ptr<Variable> getVariable(const std::string& name);

	/** @brief Removes all variables from the handler */
	void clear();

	/** @brief Checks if handler contains no variables */
	bool isEmpty();

	/** @brief Removes a specific variable by name */
	void erase(const std::string& nameToDelete);

	/** @brief Checks if a variable with given name exists */
	bool contains(const std::string& name);

	/** @brief Creates and adds a new variable with given name */
	void addNewVariable(std::string newName);

	/** @brief Renames an existing variable and invokes rename callback */
	void renameVariable(const std::string& currentName, const std::string& newName);

	/** @brief Custom iterator for iterating over Variable objects */
	class iterator
	{
	   public:
		using iterator_category = std::forward_iterator_tag;
		explicit iterator(std::map<std::string, std::shared_ptr<Variable>>::iterator iter);
		iterator& operator++();
		iterator operator++(int);
		bool operator==(const iterator& other) const;
		bool operator!=(const iterator& other) const;
		std::shared_ptr<Variable> operator*();

	   private:
		std::map<std::string, std::shared_ptr<Variable>>::iterator m_iter;
	};

	/** @brief Returns iterator to first variable */
	iterator begin();

	/** @brief Returns iterator past last variable */
	iterator end();

   public:
	/**
	 * @brief Callback invoked when variable is renamed
	 *
	 * Signature: void(const std::string& oldName, const std::string& newName)
	 * Used to notify other components (e.g., plots) of variable name changes
	 */
	std::function<void(const std::string&, const std::string&)> renameCallback;

   private:
	/** @brief Map storing all variables keyed by name */
	VariableMap variableMap;
};