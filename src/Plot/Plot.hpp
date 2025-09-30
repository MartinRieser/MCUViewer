/**
 * @file Plot.hpp
 * @brief Core plot data structure for variable and trace visualization
 *
 * Defines the Plot class which represents a single plot window containing one or more
 * data series (variables). Supports multiple plot types (curve, bar, table, XY), domains
 * (analog/digital), markers, statistics selection, and various display formats.
 */

#ifndef _PLOT_HPP
#define _PLOT_HPP

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ScrollingBuffer.hpp"
#include "Variable.hpp"

/**
 * @class Plot
 * @brief Container for plotted variable data series
 *
 * A Plot manages multiple data series (each linked to a Variable) and provides:
 * - Time-series data storage via ScrollingBuffer
 * - Multiple plot types: curve (line), bar, table, XY scatter
 * - Analog and digital signal domains
 * - Interactive markers (X0, X1) for measurements
 * - Statistics selection regions
 * - Series visibility control
 * - Hover state tracking for UI interaction
 *
 * Each plot has a name (identifier) and optional alias (display name).
 * Series are stored in a map keyed by variable name.
 */
class Plot
{
   public:
	/** @brief Number format for displaying values */
	enum class displayFormat
	{
		DEC = 0,  ///< Decimal format
		HEX = 1,  ///< Hexadecimal format
		BIN = 2,  ///< Binary format
	};

	/** @brief Single data series within a plot */
	struct Series
	{
		Variable* var = nullptr;  ///< Associated variable
		displayFormat format = displayFormat::DEC;  ///< Display format
		std::unique_ptr<ScrollingBuffer<double>> buffer;  ///< Time-series data buffer
		bool visible = true;  ///< Visibility toggle

		/** @brief Add current variable value to series buffer */
		void addPointFromVar() { buffer->addPoint(var->getValue()); }
	};

	/** @brief Plot visualization type */
	enum class Type : uint8_t
	{
		CURVE = 0,  ///< Line/curve plot (continuous signals)
		BAR = 1,    ///< Bar chart (discrete values)
		TABLE = 2,  ///< Tabular data display
		XY = 3      ///< XY scatter plot (phase plots, parametric)
	};

	/** @brief Signal domain classification */
	enum class Domain : uint8_t
	{
		ANALOG = 0,   ///< Continuous analog signals
		DIGITAL = 1,  ///< Digital/binary signals
	};

	/** @brief Trace channel data type (for SWO trace viewer) */
	enum class TraceVarType : uint8_t
	{
		U8 = 0,   ///< Unsigned 8-bit
		I8 = 1,   ///< Signed 8-bit
		U16 = 2,  ///< Unsigned 16-bit
		I16 = 3,  ///< Signed 16-bit
		U32 = 4,  ///< Unsigned 32-bit
		I32 = 5,  ///< Signed 32-bit
		F32 = 6   ///< 32-bit float
	};

	/** @brief Draggable rectangle for statistics region selection */
	class DragRect
	{
	   public:
		bool getState() const { return state; }  ///< Get active state
		void setState(bool newState) { state = newState; }  ///< Set active state
		double getValueX0() const { return std::min(rect.x0, rect.x1); }  ///< Get left edge
		double getValueX1() const { return std::max(rect.x0, rect.x1); }  ///< Get right edge
		void setValueX0(double newX0) { rect.x0 = newX0; }  ///< Set left edge
		void setValueX1(double newX1) { rect.x1 = newX1; }  ///< Set right edge

	   private:
		bool state = false;  ///< True when rectangle is active
		struct Rect
		{
			double x0, x1, y0, y1;  ///< Rectangle coordinates
		} rect{};
	};

	/** @brief Interactive marker for measurements */
	class Marker
	{
	   public:
		Marker() : state(false), value(0.0) {}
		bool getState() const { return state; }  ///< Get visibility
		void setState(bool newState) { state = newState; }  ///< Set visibility
		double getValue() const { return value; }  ///< Get X position
		void setValue(double newValue) { value = newValue; }  ///< Set X position

	   private:
		bool state;  ///< True when marker is visible
		double value;  ///< X-axis position
	};

	Marker markerX0{};  ///< First measurement marker
	Marker markerX1{};  ///< Second measurement marker
	Marker trigger{};   ///< Trigger marker (for trace viewer)
	DragRect stats{};   ///< Statistics selection rectangle

	explicit Plot(const std::string& name);
	void setName(const std::string& newName);
	std::string getName() const;
	std::string& getNameVar();
	void setAlias(const std::string& newAlias);
	std::string getAlias() const;
	bool addSeries(Variable* var);
	std::shared_ptr<Plot::Series> getSeries(const std::string& name);
	std::map<std::string, std::shared_ptr<Plot::Series>>& getSeriesMap();
	ScrollingBuffer<double>* getXAxisSeries();
	bool removeSeries(const std::string& name);
	bool removeAllVariables();
	void renameSeries(const std::string& oldName, const std::string newName);
	std::vector<uint32_t> getVariableAddesses() const;
	std::vector<Variable::Type> getVariableTypes() const;
	bool addPoint(const std::string& varName, double value);
	void updateSeries();
	bool addTimePoint(double t);
	void erase();
	void setVisibility(bool state);
	bool getVisibility() const;
	bool& getVisibilityVar();

	void setType(Type newType);
	Type getType() const;

	/* TODO: Domain and TraceVarType should be in a derived class only */
	void setDomain(Domain newDomain);
	Domain getDomain() const;

	void setTraceVarType(TraceVarType newTraceVarType);
	TraceVarType getTraceVarType() const;

	void setIsHovered(bool isHovered);
	bool isHovered() const;

	Variable* getXAxisVariable();
	void setXAxisVariable(Variable* var);

	displayFormat getSeriesDisplayFormat(const std::string& name) const;
	void setSeriesDisplayFormat(const std::string& name, displayFormat format);
	std::string getSeriesValueString(const std::string& name, double value);

	int32_t statisticsSeries = 0;

   private:
	std::string name;
	std::string alias;
	std::map<std::string, std::shared_ptr<Series>> seriesMap;
	ScrollingBuffer<double> time;
	Series xAxisSeries;
	bool visibility = true;
	Type type = Type::CURVE;
	Domain domain = Domain::ANALOG;
	TraceVarType traceVarType = TraceVarType::F32;
	bool isHoveredOver = false;

	Marker mx0;
	Marker mx1;
};

#endif