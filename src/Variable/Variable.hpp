/**
 * @file Variable.hpp
 * @brief Variable representation for MCU memory monitoring.
 *
 * This file defines the Variable class which represents a single variable
 * being monitored from MCU memory. It handles type information, value storage,
 * memory addressing, display formatting, and advanced features like fractional
 * number representation and bit masking.
 */

#ifndef __VARIABLE_HPP
#define __VARIABLE_HPP

#include <cstdint>
#include <functional>
#include <string>

/**
 * @class Variable
 * @brief Represents a variable read from MCU memory with type, address, and display properties.
 *
 * Variables are the core data entities in MCUViewer. Each variable corresponds to a memory
 * location in the target MCU and tracks:
 * - Memory address and size
 * - Data type (integers, floats, fractional numbers)
 * - Current and raw values
 * - Display properties (color, format)
 * - Bit manipulation (mask, shift)
 * - ELF file synchronization state
 *
 * Usage example:
 * @code
 * Variable speedVar("motorSpeed");
 * speedVar.setType(Variable::Type::U32);
 * speedVar.setAddress(0x20000100);
 * speedVar.setColor(1.0f, 0.0f, 0.0f, 1.0f); // Red
 *
 * // Read from MCU and update
 * uint32_t rawValue = readFromMCU(speedVar.getAddress(), speedVar.getSize());
 * speedVar.setRawValue(rawValue);
 * double speed = speedVar.getValue();
 * @endcode
 */
class Variable
{
   public:
	/**
	 * @enum Type
	 * @brief Data types supported for variable representation.
	 */
	enum class Type
	{
		UNKNOWN = 0, /**< Unknown or uninitialized type */
		U8 = 1,      /**< Unsigned 8-bit integer (0-255) */
		I8 = 2,      /**< Signed 8-bit integer (-128 to 127) */
		U16 = 3,     /**< Unsigned 16-bit integer (0-65535) */
		I16 = 4,     /**< Signed 16-bit integer (-32768 to 32767) */
		U32 = 5,     /**< Unsigned 32-bit integer (0-4294967295) */
		I32 = 6,     /**< Signed 32-bit integer (-2147483648 to 2147483647) */
		F32 = 7      /**< 32-bit IEEE 754 floating point */
	};

	/**
	 * @enum HighLevelType
	 * @brief Advanced type representations for special number formats.
	 */
	enum class HighLevelType
	{
		NONE = 0,         /**< Standard type representation */
		SIGNEDFRAC = 1,   /**< Signed fixed-point fractional number (Q format) */
		UNSIGNEDFRAC = 2, /**< Unsigned fixed-point fractional number (UQ format) */
	};

	/**
	 * @struct Color
	 * @brief RGBA color for variable visualization in plots.
	 */
	struct Color
	{
		float r; /**< Red component (0.0-1.0) */
		float g; /**< Green component (0.0-1.0) */
		float b; /**< Blue component (0.0-1.0) */
		float a; /**< Alpha/opacity component (0.0-1.0) */
	};

	/**
	 * @struct Fractional
	 * @brief Configuration for fixed-point fractional number representation.
	 *
	 * Fixed-point numbers represent fractional values using integers by defining
	 * how many bits represent the fractional part. Common in embedded systems
	 * for efficient non-floating-point math.
	 *
	 * Example: Q15 format uses 1 sign bit, 16 integer bits, 15 fractional bits
	 */
	struct Fractional
	{
		uint32_t fractionalBits = 15;  /**< Number of bits for fractional part (typical: 8-16) */
		double base = 1.0;             /**< Base scaling factor for the fractional value */
		Variable* baseVariable = nullptr; /**< Optional variable to use as dynamic base value */
	};

	/**
	 * @brief Construct a variable with a name.
	 *
	 * Creates a variable with default type (UNKNOWN) and address.
	 *
	 * @param name Variable name (e.g., "motorSpeed", "sensorTemp")
	 */
	explicit Variable(std::string name);

	/**
	 * @brief Construct a variable with name, type, and initial value.
	 *
	 * @param name Variable name
	 * @param type Data type
	 * @param value Initial value
	 */
	Variable(std::string name, Type type, double value);

	/**
	 * @brief Set the variable's data type.
	 *
	 * Determines how raw memory bytes are interpreted (e.g., U32, F32).
	 *
	 * @param type Data type enumeration value
	 */
	void setType(Type type);

	/**
	 * @brief Get the variable's data type.
	 *
	 * @return Current data type
	 */
	Type getType() const;

	/**
	 * @brief Get human-readable type string.
	 *
	 * @return Type as string (e.g., "u32", "f32", "i16")
	 */
	std::string getTypeStr() const;

	/**
	 * @brief Set raw value from MCU memory read.
	 *
	 * Updates the variable's raw value (as read from memory) and automatically
	 * converts to double representation based on type. Applies mask/shift if configured.
	 *
	 * @param value Raw 32-bit value from memory
	 */
	void setRawValue(uint32_t value);

	/**
	 * @brief Set value directly as double.
	 *
	 * @param val New value
	 */
	void setValue(double val);

	/**
	 * @brief Get current value as double.
	 *
	 * Returns the interpreted value after type conversion, fractional scaling,
	 * and any transformations.
	 *
	 * @return Current value
	 */
	double getValue() const;

	/**
	 * @brief Set memory address where variable resides in MCU.
	 *
	 * @param addr 32-bit memory address (e.g., 0x20000000)
	 */
	void setAddress(uint32_t addr);

	/**
	 * @brief Get memory address.
	 *
	 * @return Memory address
	 */
	uint32_t getAddress() const;

	/**
	 * @brief Get variable name.
	 *
	 * @return Variable name string
	 */
	std::string getName();

	/**
	 * @brief Rename the variable.
	 *
	 * @param newName New name for the variable
	 */
	void rename(const std::string& newName);

	/**
	 * @brief Set plot display color using RGBA components.
	 *
	 * @param r Red (0.0-1.0)
	 * @param g Green (0.0-1.0)
	 * @param b Blue (0.0-1.0)
	 * @param a Alpha/opacity (0.0-1.0)
	 */
	void setColor(float r, float g, float b, float a);

	/**
	 * @brief Set color using 32-bit packed format.
	 *
	 * @param AaBbGgRr Color in AABBGGRR format (ImGui style)
	 */
	void setColor(uint32_t AaBbGgRr);

	/**
	 * @brief Get color structure.
	 *
	 * @return Reference to Color struct
	 */
	Color& getColor();

	/**
	 * @brief Get color as 32-bit packed value.
	 *
	 * @return Color in AABBGGRR format
	 */
	uint32_t getColorU32() const;

	/**
	 * @brief Check if variable was found in ELF file.
	 *
	 * @return true if found in symbol table during ELF parsing
	 */
	bool getIsFound() const;

	/**
	 * @brief Set whether variable was found in ELF file.
	 *
	 * Used by GdbParser to mark variables that exist in the compiled binary.
	 *
	 * @param found true if found in ELF
	 */
	void setIsFound(bool found);

	/**
	 * @brief Get size in bytes based on type.
	 *
	 * @return Size (1 for U8/I8, 2 for U16/I16, 4 for U32/I32/F32)
	 */
	uint8_t getSize();

	/**
	 * @brief Check if address should update from ELF file.
	 *
	 * @return true if variable address should sync with ELF on reload
	 */
	bool getShouldUpdateFromElf() const;

	/**
	 * @brief Set whether to update address from ELF file.
	 *
	 * When enabled, variable address is refreshed from ELF symbol table
	 * each time the ELF file is reloaded (useful for development).
	 *
	 * @param shouldUpdateFromElf true to enable ELF synchronization
	 */
	void setShouldUpdateFromElf(bool shouldUpdateFromElf);

	/**
	 * @brief Check if tracked name differs from display name.
	 *
	 * @return true if internal tracked name != display name
	 */
	bool getIsTrackedNameDifferent() const;

	/**
	 * @brief Set whether tracked name is different.
	 *
	 * @param isDifferent true if names differ
	 */
	void setIsTrackedNameDifferent(bool isDifferent);

	/**
	 * @brief Get internal tracked name.
	 *
	 * The tracked name is used for ELF symbol lookup, while the display
	 * name can be customized by the user.
	 *
	 * @return Tracked name string
	 */
	std::string getTrackedName() const;

	/**
	 * @brief Set internal tracked name for ELF lookup.
	 *
	 * @param trackedName Name as it appears in ELF symbol table
	 */
	void setTrackedName(const std::string& trackedName);

	/**
	 * @brief Set bit shift for extracting bit fields.
	 *
	 * Used to extract specific bits from the raw value.
	 * value = (rawValue & mask) >> shift
	 *
	 * @param shift Number of bits to right-shift (0-31)
	 */
	void setShift(uint32_t shift);

	/**
	 * @brief Get bit shift value.
	 *
	 * @return Current shift value
	 */
	uint32_t getShift() const;

	/**
	 * @brief Set bit mask for extracting bit fields.
	 *
	 * Applied before shifting to isolate specific bits.
	 * value = (rawValue & mask) >> shift
	 *
	 * @param mask Bit mask (default 0xFFFFFFFF for full value)
	 */
	void setMask(uint32_t mask);

	/**
	 * @brief Get bit mask value.
	 *
	 * @return Current mask value
	 */
	uint32_t getMask() const;

	/**
	 * @brief Set high-level type for special representations.
	 *
	 * @param type High-level type (fractional formats, etc.)
	 */
	void setHighLevelType(HighLevelType type);

	/**
	 * @brief Get high-level type.
	 *
	 * @return Current high-level type
	 */
	HighLevelType getHighLevelType() const;

	/**
	 * @brief Configure fractional number representation.
	 *
	 * Sets parameters for interpreting the value as a fixed-point number.
	 *
	 * @param fractional Fractional configuration struct
	 */
	void setFractional(Fractional fractional);

	/**
	 * @brief Get fractional configuration.
	 *
	 * @return Current fractional settings
	 */
	Variable::Fractional getFractional() const;

	/**
	 * @brief Check if variable uses fractional representation.
	 *
	 * @return true if high-level type is SIGNEDFRAC or UNSIGNEDFRAC
	 */
	bool isFractional() const;

	/**
	 * @brief Convert double value to raw representation.
	 *
	 * Performs inverse transformation from double to raw integer value
	 * based on type and fractional settings.
	 *
	 * @param value Double value to convert
	 * @return Raw 32-bit representation
	 */
	uint32_t getRawFromDouble(double value);

	/**
	 * @brief Transform raw value to double.
	 *
	 * Converts raw value to double considering type, fractional settings,
	 * mask, and shift.
	 *
	 * @return Transformed double value
	 */
	double transformToDouble();

	/**
	 * @brief Set whether variable is currently being sampled.
	 *
	 * Tracks if variable is actively monitored in current acquisition session.
	 *
	 * @param isCurrentlySampled true if being sampled
	 */
	void setIsCurrentlySampled(bool isCurrentlySampled);

	/**
	 * @brief Check if variable is currently being sampled.
	 *
	 * @return true if in active sampling session
	 */
	bool getIsCurrentlySampled() const;

   public:
	static const char* types[8];        /**< String names for Type enum */
	static const char* highLevelTypes[3]; /**< String names for HighLevelType enum */

   private:
	std::string name = "";                      /**< Display name shown in UI */
	std::string trackedName = "";               /**< Internal name for ELF symbol lookup */
	Type type = Type::UNKNOWN;                  /**< Variable data type */
	HighLevelType highLevelType = HighLevelType::NONE; /**< Advanced type representation */

	double value = 0.0;                         /**< Current value as double precision */
	uint32_t rawValue = 0;                      /**< Raw value from memory (before transformation) */

	uint32_t address = 0x20000000;              /**< Memory address in MCU (default to RAM start) */
	Fractional fractional{};                    /**< Fractional number configuration */

	uint32_t shift = 0;                         /**< Bit shift for bit field extraction */
	uint32_t mask = 0xffffffff;                 /**< Bit mask for bit field extraction */

	Color color{};                              /**< Plot display color (RGBA) */
	bool isFound = false;                       /**< True if found in ELF symbol table */
	bool shouldUpdateFromElf = true;            /**< True to sync address from ELF on reload */
	bool isTrackedNameDifferent = false;        /**< True if tracked name differs from display name */
	bool isCurrentlySampled = false;            /**< True if actively being sampled */
};

#endif