#pragma once

#include <string>
#include <vector>

#include "types.hpp"

std::vector<int> centered_positions(int width, int count);
std::vector<int> center_out_order(int width);
void apply_lateral_fill(
	std::vector<UnitState>& self_units,
	const std::vector<UnitState>& enemy_units
);
std::string alive_line(const std::vector<UnitState>& units);
