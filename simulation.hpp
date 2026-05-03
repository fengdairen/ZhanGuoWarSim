#pragma once

#include <random>
#include <vector>

#include "types.hpp"

// 单次推演（逐回合交互查看战报）
// 返回 false 表示用户选择退出程序
bool run_single_simulation(
	int& battlefield_width,
	const std::vector<Regiment>& a_regs,
	const std::vector<Regiment>& b_regs,
	Multipliers& mult,
	double& hit_k,
	int& max_rounds,
	double& flank_multiplier,
	FlankTargetMode& flank_target_mode,
	int& vertical_fill_delay,
	std::mt19937& rng
);
