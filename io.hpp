#pragma once

#include <string>
#include <vector>

#include "types.hpp"

// ==== 输入与菜单 ====

double read_double(const std::string& prompt, double default_value);
int read_int(const std::string& prompt, int default_value);

void edit_global_menu(
	double& hit_k,
	Multipliers& mult,
	int& max_rounds,
	int& battlefield_width,
	double& flank_multiplier,
	FlankTargetMode& flank_target_mode,
	int& vertical_fill_delay
);

std::vector<int> parse_indices(const std::string& line, int max_index);
const char* flank_target_mode_label(FlankTargetMode mode);

// ==== CSV 读取 ====

struct CsvParams {
	double hit_k = 10.0;
	Multipliers mult;
	int max_rounds = 10;
	int battlefield_width = 10;
	double flank_multiplier = 0.4;
	FlankTargetMode flank_target_mode = FlankTargetMode::Hp;
	int vertical_fill_delay = 2;
	std::vector<Regiment> side_a_regiments;
	std::vector<Regiment> side_b_regiments;
};

CsvParams read_csv_params(const std::string& filepath);
