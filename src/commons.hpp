/**
 * @file commons.hpp
 * @brief Common utility functions and platform detection macros
 *
 * Provides platform-independent utilities and macros used throughout MCUViewer.
 * Includes string manipulation helpers and Unix/Linux platform detection.
 */

#ifndef COMMONS_HPP
#define COMMONS_HPP

#include <algorithm>
#include <string>

#if defined(unix) || defined(__unix__) || defined(__unix)
#define _UNIX  ///< Unix/Linux platform detection macro
#endif

/**
 * @brief Convert string to lowercase
 * @param str Input string to convert
 * @return std::string Lowercase version of input
 * @note Used for case-insensitive search/comparison operations
 */
std::string toLower(std::string str);

#endif