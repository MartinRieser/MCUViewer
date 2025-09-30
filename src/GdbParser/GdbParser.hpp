/**
 * @file GdbParser.hpp
 * @brief ELF file parser using GDB for extracting variable information
 *
 * This file implements a GDB-based parser that extracts variable names, addresses,
 * and types from ELF debug information. It uses GDB's scripting interface to query
 * symbol information and convert it into MCUViewer's variable format.
 */

#ifndef _GDBPARSER_HPP
#define _GDBPARSER_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ProcessHandler.hpp"
#include "Variable.hpp"
#include "VariableHandler.hpp"
#include "spdlog/spdlog.h"

/**
 * @class GdbParser
 * @brief Extracts variable information from ELF files using GDB
 *
 * GdbParser interfaces with GDB to extract debugging symbols from compiled ELF
 * binaries. It:
 * - Launches GDB as a subprocess and communicates via stdin/stdout
 * - Queries variable names, memory addresses, and data types
 * - Identifies trivial (primitive) types vs. complex structures
 * - Updates the VariableHandler with current address mappings
 * - Supports custom GDB command paths for cross-compilation scenarios
 *
 * The parser filters out non-trivial types (structs, arrays, pointers) and
 * focuses on directly-readable primitive types (integers, floats, bools).
 *
 * @note Thread-safe: Uses mutex for concurrent access protection
 */
class GdbParser
{
   public:
	/**
	 * @brief Parsed variable data from ELF file
	 */
	struct VariableData
	{
		uint32_t address;      ///< Memory address of the variable
		bool isTrivial = false; ///< True if variable is a primitive type (readable via debug probe)
	};

	/** @brief Construct parser with variable handler and logger
	 *  @param variableHandler Handler managing all application variables
	 *  @param logger Logger for diagnostic messages */
	GdbParser(VariableHandler* variableHandler, spdlog::logger* logger);

	/** @brief Verify GDB is available and functional
	 *  @return true if GDB executable found and responsive, false otherwise */
	bool validateGDB();

	/** @brief Update existing variable addresses from ELF without full reparse
	 *  @param elfPath Path to ELF file containing updated symbols
	 *  @return true on success, false on parse error */
	bool updateVariableMap(const std::string& elfPath);

	/** @brief Parse ELF file and extract all variables
	 *  @param elfPath Path to ELF file to parse
	 *  @return true on success, false on parse error */
	bool parse(const std::string& elfPath);

	/** @brief Get all parsed variable data
	 *  @return std::map<std::string, VariableData> Map of variable names to their data */
	std::map<std::string, VariableData> getParsedData();

	/** @brief Change GDB executable command (for cross-compilation)
	 *  @param command GDB command to use (e.g., "arm-none-eabi-gdb") */
	void changeCurrentGDBCommand(const std::string& command);

   private:
	/** @brief Parse a chunk of GDB output containing variable symbols */
	void parseVariableChunk(const std::string& chunk);

	/** @brief Determine if variable is trivial and extract its type */
	void checkVariableType(std::string& name);

	/** @brief Query GDB for variable's data type
	 *  @param name Variable name to check
	 *  @param output Pointer to store GDB's type response
	 *  @return Variable::Type The determined variable type */
	Variable::Type checkType(const std::string& name, std::string* output);

	/** @brief Query GDB for variable's memory address
	 *  @param name Variable name to check
	 *  @return std::optional<uint32_t> Address if found, nullopt otherwise */
	std::optional<uint32_t> checkAddress(const std::string& name);

   private:
	const char* defaultGDBCommand = "gdb";  ///< Default GDB command
	std::string currentGDBCommand = std::string(defaultGDBCommand);  ///< Active GDB command
	VariableHandler* variableHandler;  ///< Target for parsed variables
	spdlog::logger* logger;  ///< Logger for messages
	std::mutex mtx;  ///< Mutex for thread-safe access
	std::map<std::string, VariableData> parsedData;  ///< Cache of parsed variable data
	ProcessHandler process;  ///< GDB process communication handler

	/** @brief Map of C/C++ type names to MCUViewer variable types
	 *  Used to identify trivial (directly-readable) types and convert to internal representation */
	std::unordered_map<std::string, Variable::Type> isTrivial = {
		{"_Bool", Variable::Type::U8},
		{"bool", Variable::Type::U8},
		{"unsigned char", Variable::Type::U8},
		{"unsigned 8-bit", Variable::Type::U8},

		{"char", Variable::Type::I8},
		{"signed char", Variable::Type::I8},
		{"signed 8-bit", Variable::Type::I8},

		{"unsigned short", Variable::Type::U16},
		{"unsigned 16-bit", Variable::Type::U16},
		{"unsigned short int", Variable::Type::U16},
		{"short unsigned int", Variable::Type::U16},

		{"short", Variable::Type::I16},
		{"short int", Variable::Type::I16},
		{"signed short", Variable::Type::I16},
		{"signed 16-bit", Variable::Type::I16},
		{"signed short int", Variable::Type::I16},
		{"short signed int", Variable::Type::I16},

		{"unsigned int", Variable::Type::U32},
		{"unsigned long", Variable::Type::U32},
		{"unsigned 32-bit", Variable::Type::U32},
		{"unsigned long int", Variable::Type::U32},
		{"long unsigned int", Variable::Type::U32},

		{"int", Variable::Type::I32},
		{"long", Variable::Type::I32},
		{"long int", Variable::Type::I32},
		{"signed int", Variable::Type::I32},
		{"signed long", Variable::Type::I32},
		{"signed 32-bit", Variable::Type::I32},
		{"signed long int", Variable::Type::I32},
		{"long signed int", Variable::Type::I32},

		{"float", Variable::Type::F32},
	};
};
#endif