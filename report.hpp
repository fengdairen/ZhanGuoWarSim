#pragma once

#include <random>
#include <string>
#include <vector>

#include "types.hpp"

void print_regiment_reports(
	const std::string& side_name,
	const std::vector<Regiment>& regiments,
	const std::vector<RegimentReport>& reports
);

void print_outcome(int a_total, int b_total, bool time_up);

void run_hundred_test(
	int battlefield_width,
	const std::vector<Regiment>& a_regiments,
	const std::vector<Regiment>& b_regiments,
	const Multipliers& mult,
	double hit_k,
	int max_rounds,
	double flank_multiplier,
	FlankTargetMode flank_target_mode,
	int vertical_fill_delay,
	std::mt19937& rng
);
