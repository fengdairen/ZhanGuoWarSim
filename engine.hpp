#pragma once

#include <random>
#include <vector>

#include "types.hpp"

struct BattleOutcome {
	int a_remaining;
	int b_remaining;
};

int backline_queue_total(const std::vector<std::vector<int>>& queues);

// 多兵种战斗引擎（静默模式，同时收集兵种战报）
BattleOutcome run_battle_multi(
	int battlefield_width,
	const std::vector<Regiment>& a_regiments,
	const std::vector<Regiment>& b_regiments,
	const Multipliers& mult,
	double hit_k,
	int max_rounds,
	double flank_multiplier,
	FlankTargetMode flank_target_mode,
	int vertical_fill_delay,
	std::mt19937& rng,
	std::vector<RegimentReport>* a_reports_out = nullptr,
	std::vector<RegimentReport>* b_reports_out = nullptr
);
