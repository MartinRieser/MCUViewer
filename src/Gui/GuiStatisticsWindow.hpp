/**
 * @file GuiStatisticsWindow.hpp
 * @brief Statistics display window for plot analysis
 *
 * Provides UI for displaying statistical analysis results for both
 * analog and digital signals with range selection capabilities.
 */

#pragma once

#include "GuiHelper.hpp"
#include "GuiStatisticsWindow.hpp"
#include "Plot.hpp"
#include "Statistics.hpp"

/**
 * @class StatisticsWindow
 * @brief Window displaying statistical analysis results for plots
 *
 * Creates a floating window showing:
 * - Series selection dropdown
 * - Range selection controls (markers x0, x1)
 * - Statistical metrics (min, max, mean, stddev for analog; pulse widths and frequency for digital)
 *
 * @note Works with marker-based range selection from plots
 * @note Displays different metrics depending on signal type
 */
class StatisticsWindow
{
   public:
	/**
	 * @brief Draws statistics window for analog signals
	 *
	 * Displays min, max, mean, and standard deviation within selected range.
	 *
	 * @param plt Shared pointer to plot to analyze
	 * @note Creates "Statistics" window if series is selected
	 */
	void drawAnalog(std::shared_ptr<Plot> plt)
	{
		std::vector<std::string> serNames{"OFF"};
		for (auto& [name, ser] : plt->getSeriesMap())
			serNames.push_back(name);

		ImGui::Text("statistics ");
		ImGui::SameLine();
		ImGui::Combo("##stats", &plt->statisticsSeries, serNames);

		if (plt->statisticsSeries != 0)
		{
			static bool selectRange = false;
			ImGui::Begin("Statistics");

			auto ser = plt->getSeries(serNames[plt->statisticsSeries]);

			ImGui::ColorEdit4("##", &ser->var->getColor().r, ImGuiColorEditFlags_NoInputs);
			ImGui::SameLine();
			ImGui::Text("%s", ser->var->getName().c_str());

			ImGui::Text("select range: ");
			ImGui::SameLine();
			ImGui::Checkbox("##selectrange", &selectRange);

			plt->stats.setState(selectRange);

			Statistics::AnalogResults results;
			Statistics::calculateResults(ser.get(), plt->getXAxisSeries(), plt->stats.getValueX0(), plt->stats.getValueX1(), results);

			GuiHelper::drawDescriptionWithNumber("t0:      ", plt->stats.getValueX0());
			GuiHelper::drawDescriptionWithNumber("t1:      ", plt->stats.getValueX1());
			GuiHelper::drawDescriptionWithNumber("t1-t0:   ", plt->stats.getValueX1() - plt->stats.getValueX0());
			GuiHelper::drawDescriptionWithNumber("min:     ", results.min);
			GuiHelper::drawDescriptionWithNumber("max:     ", results.max);
			GuiHelper::drawDescriptionWithNumber("mean:    ", results.mean);
			GuiHelper::drawDescriptionWithNumber("stddev:  ", results.stddev);
			ImGui::End();
		}
		else
			plt->stats.setState(false);
	}

	/**
	 * @brief Draws statistics window for digital signals
	 *
	 * Displays pulse width analysis (Lmin, Lmax, Hmin, Hmax) and frequency (fmin, fmax).
	 *
	 * @param plt Shared pointer to plot to analyze
	 * @note Creates "Statistics" window if series is selected
	 */
	void drawDigital(std::shared_ptr<Plot> plt)
	{
		std::vector<std::string> serNames{"OFF"};
		for (auto& [name, ser] : plt->getSeriesMap())
			serNames.push_back(name);

		ImGui::Text("statistics ");
		ImGui::SameLine();
		ImGui::Combo("##stats", &plt->statisticsSeries, serNames);

		if (plt->statisticsSeries != 0)
		{
			static bool selectRange = false;
			ImGui::Begin("Statistics");

			auto ser = plt->getSeries(serNames[plt->statisticsSeries]);

			ImGui::ColorEdit4("##", &ser->var->getColor().r, ImGuiColorEditFlags_NoInputs);
			ImGui::SameLine();
			ImGui::Text("%s", ser->var->getName().c_str());

			ImGui::Text("select range: ");
			ImGui::SameLine();
			ImGui::Checkbox("##selectrange", &selectRange);

			plt->stats.setState(selectRange);

			Statistics::DigitalResults results;
			Statistics::calculateResults(ser.get(), plt->getXAxisSeries(), plt->stats.getValueX0(), plt->stats.getValueX1(), results);

			GuiHelper::drawDescriptionWithNumber("t0:      ", plt->stats.getValueX0());
			GuiHelper::drawDescriptionWithNumber("t1:      ", plt->stats.getValueX1());
			GuiHelper::drawDescriptionWithNumber("t1-t0:   ", plt->stats.getValueX1() - plt->stats.getValueX0());
			GuiHelper::drawDescriptionWithNumber("Lmin:    ", results.Lmin);
			GuiHelper::drawDescriptionWithNumber("Lmax:    ", results.Lmax);
			GuiHelper::drawDescriptionWithNumber("Hmin:    ", results.Hmin);
			GuiHelper::drawDescriptionWithNumber("Hmax:    ", results.Hmax);
			GuiHelper::drawDescriptionWithNumber("fmin:    ", results.fmin);
			GuiHelper::drawDescriptionWithNumber("fmax:    ", results.fmax);
			ImGui::End();
		}
		else
			plt->stats.setState(false);
	}
};