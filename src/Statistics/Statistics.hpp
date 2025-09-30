/**
 * @file Statistics.hpp
 * @brief Statistical analysis functions for plot data in MCUViewer
 *
 * Provides statistical calculations for both analog and digital signal analysis
 * including min/max, mean, standard deviation, and frequency analysis.
 */

#ifndef STATISTICS_HPP_
#define STATISTICS_HPP_

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "Plot.hpp"
#include "ScrollingBuffer.hpp"

#pragma once
#ifndef TEST_FRIENDS_STATISTICS
#define TEST_FRIENDS_STATISTICS
#endif

/**
 * @class Statistics
 * @brief Static utility class for statistical analysis of plot data
 *
 * Provides two main analysis modes:
 * - Analog signal statistics (min, max, mean, stddev)
 * - Digital signal statistics (pulse width, frequency analysis)
 *
 * All methods are static and operate on time-series data within a specified range.
 *
 * @note Uses marker-based range selection for analysis
 * @note Designed for real-time signal analysis on circular buffers
 */
class Statistics
{
   public:
	/**
	 * @struct AnalogResults
	 * @brief Results structure for analog signal statistics
	 */
	struct AnalogResults
	{
		double min;     /**< Minimum value in range */
		double max;     /**< Maximum value in range */
		double mean;    /**< Average value in range */
		double stddev;  /**< Standard deviation in range */
	};

	/**
	 * @struct DigitalResults
	 * @brief Results structure for digital signal statistics
	 */
	struct DigitalResults
	{
		double Lmin;  /**< Minimum low pulse width */
		double Lmax;  /**< Maximum low pulse width */
		double Hmin;  /**< Minimum high pulse width */
		double Hmax;  /**< Maximum high pulse width */
		double fmin;  /**< Minimum frequency */
		double fmax;  /**< Maximum frequency */
	};

	/**
	 * @brief Calculates digital signal statistics within time range
	 *
	 * Analyzes digital waveform for pulse widths and frequencies.
	 *
	 * @param ser Plot series data to analyze
	 * @param time Time buffer for X-axis
	 * @param start Start time of analysis range
	 * @param end End time of analysis range
	 * @param results Output structure for results
	 * @note Detects rising/falling edges and measures pulse characteristics
	 */
	static void calculateResults(Plot::Series* ser, ScrollingBuffer<double>* time, double start, double end, DigitalResults& results)
	{
		auto data = ser->buffer->getLinearData(time->getIndexFromvalue(start) + 1, time->getIndexFromvalue(end) + 1);
		std::vector<double> timeData = time->getLinearData(time->getIndexFromvalue(start) + 1, time->getIndexFromvalue(end) + 1);
		std::vector<double> Lvec, Hvec;

		if (!convertDigitalSeriesToVectors(timeData, data, Lvec, Hvec))
			return;

		results.Lmin = findmin(Lvec);
		results.Lmax = findmax(Lvec);
		results.Hmin = findmin(Hvec);
		results.Hmax = findmax(Hvec);

		auto shorter = Lvec.size() < Hvec.size() ? Lvec.size() : Hvec.size();

		std::vector<double> T;
		std::vector<double> f;

		for (size_t i = 0; i < shorter; i++)
		{
			auto sum = Lvec[i] + Hvec[i];
			T.push_back(sum);
			f.push_back(1.0 / sum);
		}

		results.fmin = findmin(f);
		results.fmax = findmax(f);
	}

	/**
	 * @brief Calculates analog signal statistics within time range
	 *
	 * Computes basic statistical measures for analog signals.
	 *
	 * @param ser Plot series data to analyze
	 * @param time Time buffer for X-axis
	 * @param start Start time of analysis range
	 * @param end End time of analysis range
	 * @param results Output structure for results
	 * @note Accounts for sample-and-hold behavior (+1 offset)
	 */
	static void calculateResults(Plot::Series* ser, ScrollingBuffer<double>* time, double start, double end, AnalogResults& results)
	{
		/* + 1 is to account for the way sample is "held" for the entire duration of sample period */
		auto data = ser->buffer->getLinearData(time->getIndexFromvalue(start) + 1, time->getIndexFromvalue(end) + 1);
		results.min = findmin(data);
		results.max = findmax(data);
		results.mean = mean(data);
		results.stddev = stddev(data);
	}

   private:
	TEST_FRIENDS_STATISTICS

	/** @brief Finds minimum value in vector */
	static double findmin(std::vector<double> data)
	{
		if (data.empty())
			return 0.0;
		return *std::min_element(data.begin(), data.end());
	}

	/** @brief Finds maximum value in vector */
	static double findmax(std::vector<double> data)
	{
		if (data.empty())
			return 0.0;
		return *std::max_element(data.begin(), data.end());
	}

	/** @brief Calculates mean (average) of vector */
	static double mean(std::vector<double> data)
	{
		if (data.empty())
			return 0.0;
		return std::accumulate(data.begin(), data.end(), 0.0) / static_cast<double>(data.size());
	}

	/** @brief Calculates standard deviation of vector */
	static double stddev(std::vector<double> data)
	{
		if (data.empty())
			return 0.0;

		double m = mean(data);

		double variance = 0.0;
		for (const double& value : data)
			variance += (value - m) * (value - m);

		variance /= static_cast<double>(data.size());

		return std::sqrt(variance);
	}

	/**
	 * @brief Converts digital signal into low/high pulse width vectors
	 *
	 * Analyzes digital waveform to extract pulse width information.
	 *
	 * @param time Time stamps for each sample
	 * @param data Signal values (0 or 1)
	 * @param Lvec Output vector of low pulse widths
	 * @param Hvec Output vector of high pulse widths
	 * @return true if successful, false if insufficient data
	 * @note Finds first and last signal changes to avoid partial pulses
	 */
	static bool convertDigitalSeriesToVectors(std::vector<double> time, std::vector<double> data, std::vector<double>& Lvec, std::vector<double>& Hvec)
	{
		/* find the first and last signal change */
		size_t start = 0;
		size_t end = time.size() - 1;
		size_t i = 1;

		if (data.empty() || time.empty() || data.size() < 2 || time.size() < 2)
			return false;

		while (data[i] == data[0] && i < data.size())
			i++;
		start = i;

		i = end - 1;

		while (data[i] == data[end] && i > 0)
			i--;
		end = i;

		double lastState = data[start];
		double timeStart = time[start];

		for (i = start; i <= end; i++)
		{
			if (lastState != data[i] && timeStart < time[i])
			{
				if (data[i] > 0.0)
					Lvec.push_back(time[i] - timeStart);
				else
					Hvec.push_back(time[i] - timeStart);

				timeStart = time[i];
				lastState = data[i];
			}
		}

		return true;
	}
};

#endif