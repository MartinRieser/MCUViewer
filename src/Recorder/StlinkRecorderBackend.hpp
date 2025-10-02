#ifndef _STLINKRECORDERBACKEND_HPP
#define _STLINKRECORDERBACKEND_HPP

#include <cstring>
#include <memory>
#include <vector>

#include "RecorderModule.hpp"
#include "StlinkDebugProbe.hpp"

/**
 * @brief Recorder backend implementation for STLink debug probe
 *
 * This backend provides software recording by polling STLink's readMemory()
 * method. Target sample rate: 100 Hz for multiple variables.
 */
class StlinkRecorderBackend : public RecorderBackend
{
   public:
	/**
	 * @brief Construct STLink recorder backend
	 * @param probe Shared pointer to STLink debug probe instance
	 */
	explicit StlinkRecorderBackend(std::shared_ptr<StlinkDebugProbe> probe)
		: probe_(probe)
	{
	}

	/**
	 * @brief Read all configured variables in one operation
	 * @param addresses Vector of variable addresses
	 * @param sizes Vector of variable sizes (bytes: 1, 2, 4, 8)
	 * @param values Output vector of values (as double)
	 * @return true if all reads succeeded, false otherwise
	 */
	bool readVariables(const std::vector<uint32_t>& addresses,
					   const std::vector<uint8_t>& sizes,
					   std::vector<double>& values) override
	{
		if (!probe_ || !probe_->isValid())
		{
			lastError_ = "STLink probe not connected";
			return false;
		}

		if (addresses.size() != sizes.size())
		{
			lastError_ = "Address and size vectors must have same length";
			return false;
		}

		values.clear();
		values.reserve(addresses.size());

		// Read each variable using probe's readMemory()
		for (size_t i = 0; i < addresses.size(); i++)
		{
			uint8_t buffer[8] = {0}; // Max 8 bytes for double
			uint8_t size = sizes[i];

			// Validate size
			if (size != 1 && size != 2 && size != 4 && size != 8)
			{
				lastError_ = "Invalid variable size: " + std::to_string(size) + " (must be 1, 2, 4, or 8)";
				return false;
			}

			// Read memory
			if (!probe_->readMemory(addresses[i], buffer, size))
			{
				lastError_ = "Failed to read address 0x" + toHex(addresses[i]) + ": " + probe_->getLastErrorMsg();
				return false;
			}

			// Convert bytes to double based on size
			double value = convertToDouble(buffer, size);
			values.push_back(value);
		}

		return true;
	}

	/**
	 * @brief Check if this backend supports hardware recording
	 * @return false - STLink only supports software recording
	 */
	bool supportsHardwareRecording() const override
	{
		return false;
	}

	/**
	 * @brief Get last error message
	 */
	std::string getLastError() const override
	{
		return lastError_;
	}

   private:
	/**
	 * @brief Convert raw bytes to double based on size
	 * @param buffer Raw byte buffer
	 * @param size Size in bytes (1, 2, 4, 8)
	 * @return Converted value as double
	 */
	double convertToDouble(const uint8_t* buffer, uint8_t size) const
	{
		switch (size)
		{
			case 1:
			{
				// Treat as unsigned 8-bit
				uint8_t val;
				std::memcpy(&val, buffer, 1);
				return static_cast<double>(val);
			}
			case 2:
			{
				// Treat as unsigned 16-bit
				uint16_t val;
				std::memcpy(&val, buffer, 2);
				return static_cast<double>(val);
			}
			case 4:
			{
				// Treat as 32-bit float
				float val;
				std::memcpy(&val, buffer, 4);
				return static_cast<double>(val);
			}
			case 8:
			{
				// Treat as 64-bit double
				double val;
				std::memcpy(&val, buffer, 8);
				return val;
			}
			default:
				return 0.0;
		}
	}

	/**
	 * @brief Convert uint32_t to hex string
	 */
	std::string toHex(uint32_t value) const
	{
		char buffer[16];
		snprintf(buffer, sizeof(buffer), "%08X", value);
		return std::string(buffer);
	}

	std::shared_ptr<StlinkDebugProbe> probe_;
	std::string lastError_;
};

#endif // _STLINKRECORDERBACKEND_HPP
