/**
 * @file MovingAverage.hpp
 * @brief Moving average filter for signal smoothing
 *
 * Implements a simple moving average (SMA) filter using a circular buffer.
 * Used primarily for measuring actual sampling frequency by smoothing period measurements.
 */

#ifndef MOVINGAVG_HPP
#define MOVINGAVG_HPP

#include <cstdlib>

/**
 * @class MovingAverage
 * @brief Simple moving average filter
 *
 * Computes the running average of the last N samples efficiently using
 * a circular buffer and running sum (O(1) per sample).
 */
class MovingAverage
{
   public:
	/** @brief Construct filter
	 *  @param samples_ Number of samples in moving window (max 1000) */
	MovingAverage(const size_t samples_);

	/** @brief Process new sample through filter
	 *  @param sampleIn New input sample
	 *  @return double Filtered output (average of last N samples) */
	double filter(double sampleIn);

   private:
	static constexpr size_t maxSamples = 1000;  ///< Maximum window size
	size_t samples = 0;         ///< Configured window size
	double buffer[maxSamples] = {0};  ///< Circular sample buffer
	size_t bufferIter = 0;      ///< Current buffer position
	double bufferSum = 0;       ///< Running sum for efficiency
};

#endif