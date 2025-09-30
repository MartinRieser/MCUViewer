/**
 * @file IFileHandler.hpp
 * @brief Interface for platform-specific file dialogs
 *
 * Defines abstract interface for native file open/save dialogs.
 * Implementations use platform-specific APIs (NFD for cross-platform support).
 */

#ifndef _FILEHANDLER_HPP
#define _FILEHANDLER_HPP

#include <string>
#include <utility>

/**
 * @class IFileHandler
 * @brief Abstract interface for file dialog operations
 *
 * Provides platform-independent file selection dialogs for:
 * - Opening files (ELF binaries, config files)
 * - Saving files (CSV exports, project configs)
 * - Selecting directories (log file destinations)
 */
class IFileHandler
{
   public:
	virtual ~IFileHandler() = default;

	/** @brief Initialize file dialog system */
	virtual bool init() = 0;

	/** @brief Cleanup file dialog system */
	virtual bool deinit() = 0;

	/** @brief Show open file dialog
	 *  @param filterFileNameFileExtension Pair of (description, extensions) e.g., ("ELF Files", "elf")
	 *  @return std::string Selected file path or empty string if cancelled */
	virtual std::string openFile(std::pair<std::string, std::string>&& filterFileNameFileExtension) = 0;

	/** @brief Show save file dialog
	 *  @param filterFileNameFileExtension Pair of (description, extensions)
	 *  @return std::string Selected file path or empty string if cancelled */
	virtual std::string saveFile(std::pair<std::string, std::string>&& filterFileNameFileExtension) = 0;

	/** @brief Show directory selection dialog
	 *  @param filterFileNameFileExtension Filter parameters (may be ignored on some platforms)
	 *  @return std::string Selected directory path or empty string if cancelled */
	virtual std::string openDirectory(std::pair<std::string, std::string>&& filterFileNameFileExtension) = 0;
};

#endif