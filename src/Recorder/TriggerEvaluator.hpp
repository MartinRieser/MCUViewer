#ifndef _TRIGGEREVALUATOR_HPP
#define _TRIGGEREVALUATOR_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

// Note: TriggerConfig and trigger type enums are defined in RecorderModule.hpp
// This header is included from RecorderModule.hpp after those definitions

/**
 * @brief High-performance trigger condition evaluator
 *
 * This class evaluates trigger conditions on incoming samples with
 * optimized performance (<1μs per evaluation). Supports edge, level,
 * window, and logic triggers with hysteresis for noise immunity.
 */
class TriggerEvaluator
{
   public:
	TriggerEvaluator() : lastValue_(0.0), lastState_(false), configured_(false) {}

	/**
	 * @brief Setup trigger configuration
	 * @param config Trigger configuration
	 */
	void setup(const TriggerConfig& config)
	{
		config_ = config;
		configured_ = true;
		reset();
	}

	/**
	 * @brief Reset evaluator state (for re-arming)
	 */
	void reset()
	{
		lastValue_ = 0.0;
		lastState_ = false;
		logicStates_.clear();
	}

	/**
	 * @brief Evaluate trigger condition on sample
	 * @param values Map of address -> value
	 * @return true if trigger condition met
	 */
	bool evaluate(const std::map<uint32_t, double>& values)
	{
		if (!configured_)
			return false;

		// Get trigger variable value
		auto it = values.find(config_.varAddress);
		if (it == values.end())
			return false;

		double value = it->second;

		switch (config_.type)
		{
			case TriggerType::NONE:
				return false;

			case TriggerType::EDGE:
				return evaluateEdge(value);

			case TriggerType::LEVEL:
				return evaluateLevel(value);

			case TriggerType::WINDOW:
				return evaluateWindow(value);

			case TriggerType::LOGIC:
				return evaluateLogic(values);

			default:
				return false;
		}
	}

	/**
	 * @brief Get current configuration
	 */
	const TriggerConfig& getConfig() const
	{
		return config_;
	}

	/**
	 * @brief Check if evaluator is configured
	 */
	bool isConfigured() const
	{
		return configured_;
	}

   private:
	/**
	 * @brief Evaluate edge trigger
	 * @param value Current value
	 * @return true if edge detected
	 */
	bool evaluateEdge(double value)
	{
		EdgeCondition condition = static_cast<EdgeCondition>(config_.condition);
		double threshold = config_.value1;
		double hyst = config_.hysteresis;

		// Apply hysteresis to threshold
		double upperThreshold = threshold + hyst / 2.0;
		double lowerThreshold = threshold - hyst / 2.0;

		// Determine current state with hysteresis
		bool currentState;
		if (value >= upperThreshold)
			currentState = true;
		else if (value <= lowerThreshold)
			currentState = false;
		else
			currentState = lastState_; // Stay in current state (hysteresis)

		// Detect edge
		bool triggered = false;

		switch (condition)
		{
			case EdgeCondition::RISING:
				if (!lastState_ && currentState)
					triggered = true;
				break;

			case EdgeCondition::FALLING:
				if (lastState_ && !currentState)
					triggered = true;
				break;

			case EdgeCondition::BOTH:
				if (lastState_ != currentState)
					triggered = true;
				break;
		}

		// Update state
		lastState_ = currentState;
		lastValue_ = value;

		return triggered;
	}

	/**
	 * @brief Evaluate level trigger
	 * @param value Current value
	 * @return true if level condition met
	 */
	bool evaluateLevel(double value)
	{
		LevelCondition condition = static_cast<LevelCondition>(config_.condition);
		double threshold = config_.value1;
		double hyst = config_.hysteresis;

		// Apply hysteresis
		if (hyst > 0.0)
		{
			if (condition == LevelCondition::ABOVE)
			{
				// Trigger when crossing threshold + hysteresis/2
				double triggerThreshold = threshold + hyst / 2.0;
				bool triggered = (value > triggerThreshold);

				// Once triggered, maintain until dropping below threshold - hysteresis/2
				if (triggered || lastState_)
				{
					double releaseThreshold = threshold - hyst / 2.0;
					lastState_ = (value > releaseThreshold);
					return triggered && !lastState_; // Return true only on initial trigger
				}

				return false;
			}
			else // LevelCondition::BELOW
			{
				// Trigger when crossing threshold - hysteresis/2
				double triggerThreshold = threshold - hyst / 2.0;
				bool triggered = (value < triggerThreshold);

				// Once triggered, maintain until rising above threshold + hysteresis/2
				if (triggered || lastState_)
				{
					double releaseThreshold = threshold + hyst / 2.0;
					lastState_ = (value < releaseThreshold);
					return triggered && !lastState_; // Return true only on initial trigger
				}

				return false;
			}
		}
		else
		{
			// No hysteresis - simple comparison
			if (condition == LevelCondition::ABOVE)
				return value > threshold;
			else
				return value < threshold;
		}
	}

	/**
	 * @brief Evaluate window trigger
	 * @param value Current value
	 * @return true if window condition met
	 */
	bool evaluateWindow(double value)
	{
		WindowCondition condition = static_cast<WindowCondition>(config_.condition);
		double lower = std::min(config_.value1, config_.value2);
		double upper = std::max(config_.value1, config_.value2);
		double hyst = config_.hysteresis;

		// Apply hysteresis by expanding window
		lower -= hyst / 2.0;
		upper += hyst / 2.0;

		bool inside = (value >= lower && value <= upper);

		// Detect edge transitions with hysteresis
		bool triggered = false;

		if (condition == WindowCondition::INSIDE)
		{
			// Trigger on entering window
			if (!lastState_ && inside)
				triggered = true;
		}
		else // WindowCondition::OUTSIDE
		{
			// Trigger on exiting window
			if (lastState_ && !inside)
				triggered = true;
		}

		lastState_ = inside;
		lastValue_ = value;

		return triggered;
	}

	/**
	 * @brief Evaluate logic trigger (boolean combinations)
	 * @param values All variable values
	 * @return true if logic condition met
	 *
	 * Logic trigger format (stored in config):
	 * - value1: First condition address
	 * - value2: Second condition address (if needed)
	 * - condition: Logic operation
	 *   - 0: AND
	 *   - 1: OR
	 *   - 2: XOR
	 *   - 3: NAND
	 *   - 4: NOR
	 *
	 * Note: This is a simplified implementation. For complex boolean
	 * expressions, consider implementing a full expression parser.
	 */
	bool evaluateLogic(const std::map<uint32_t, double>& values)
	{
		// Get first condition value
		auto it1 = values.find(static_cast<uint32_t>(config_.value1));
		if (it1 == values.end())
			return false;

		bool cond1 = (it1->second != 0.0); // Non-zero = true

		// Get second condition value (if two-operand logic)
		if (config_.condition <= 4) // AND, OR, XOR, NAND, NOR
		{
			auto it2 = values.find(static_cast<uint32_t>(config_.value2));
			if (it2 == values.end())
				return false;

			bool cond2 = (it2->second != 0.0);

			// Evaluate logic operation
			bool result = false;
			switch (config_.condition)
			{
				case 0: // AND
					result = cond1 && cond2;
					break;
				case 1: // OR
					result = cond1 || cond2;
					break;
				case 2: // XOR
					result = cond1 != cond2;
					break;
				case 3: // NAND
					result = !(cond1 && cond2);
					break;
				case 4: // NOR
					result = !(cond1 || cond2);
					break;
				default:
					return false;
			}

			// Detect edge (transition from false to true)
			bool triggered = (!lastState_ && result);
			lastState_ = result;
			return triggered;
		}
		else // Single-operand logic (NOT)
		{
			bool result = !cond1;
			bool triggered = (!lastState_ && result);
			lastState_ = result;
			return triggered;
		}
	}

	TriggerConfig config_;
	double lastValue_;
	bool lastState_;
	bool configured_;

	// For complex logic triggers with multiple states
	std::map<uint32_t, bool> logicStates_;
};

#endif // _TRIGGEREVALUATOR_HPP
