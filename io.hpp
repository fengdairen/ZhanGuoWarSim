#pragma once

#include <string>
#include <vector>

#include "types.hpp"

// ==== 输入与菜单 ====

double read_double(const std::string& prompt, double default_value);
int read_int(const std::string& prompt, int default_value);
SideInputs read_side_inputs(const std::string& name, const SideInputs& defaults);

void print_side_summary(const SideInputs& s);
void edit_side_menu(SideInputs& s);
void edit_global_menu(
	double& hit_k,
	Multipliers& mult,
	int& max_rounds,
	int& battlefield_width,
	int& a_count,
	int& b_count,
	double& flank_multiplier,
	FlankTargetMode& flank_target_mode
);

std::vector<int> parse_indices(const std::string& line, int max_index);
const char* flank_target_mode_label(FlankTargetMode mode);
