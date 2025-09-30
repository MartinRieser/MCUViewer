/**
 * @file NFDFileHandler.hpp
 * @brief Native File Dialog implementation of file handler interface
 *
 * Implements IFileHandler using the NFD (Native File Dialog) library,
 * which provides cross-platform native file dialogs on Windows, macOS, and Linux.
 */

#ifndef _NFDFILEHANDLER_HPP
#define _NFDFILEHANDLER_HPP

#include <string>
#include <utility>

#include "IFileHandler.hpp"

/**
 * @class NFDFileHandler
 * @brief Cross-platform file dialog handler using NFD library
 *
 * Provides native-looking file dialogs on all supported platforms
 * by wrapping the NFD (Native File Dialog) library API.
 */
class NFDFileHandler : public IFileHandler
{
   public:
	bool init() override;
	bool deinit() override;
	std::string openFile(std::pair<std::string, std::string>&& filterFileNameFileExtension) override;
	std::string saveFile(std::pair<std::string, std::string>&& filterFileNameFileExtension) override;
	std::string openDirectory(std::pair<std::string, std::string>&& filterFileNameFileExtension) override;

   private:
	/** @brief Dialog operation type */
	enum class handleType
	{
		SAVE,    ///< Save file dialog
		OPEN,    ///< Open file dialog
		OPENDIR  ///< Directory picker dialog
	};

	/** @brief Common handler for all dialog types
	 *  @param type Type of dialog to show
	 *  @param filterFileNameFileExtension File filter specification
	 *  @return std::string Selected path or empty if cancelled */
	std::string handleFile(handleType type, std::pair<std::string, std::string>& filterFileNameFileExtension);
};

#endif