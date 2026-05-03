#include "report.hpp"

#include <iomanip>
#include <iostream>

#include "engine.hpp"

void print_regiment_reports(
	const std::string& side_name,
	const std::vector<Regiment>& regiments,
	const std::vector<RegimentReport>& reports
) {
	std::cout << "=== " << side_name << " 兵种战报 ===\n";
	for (int r = 0; r < static_cast<int>(regiments.size()); ++r) {
		const auto& reg = regiments[r];
		const auto& rep = reports[r];
		int alive = rep.initial_count - rep.deaths;
		std::cout << "  " << rep.label
			<< " (攻=" << reg.stats.attack
			<< " 闪=" << reg.stats.evade
			<< " 训=" << reg.stats.training
			<< " 甲=" << reg.stats.armor_reduction
			<< " 士气=" << reg.stats.base_morale
			<< "): 初始=" << rep.initial_count
			<< " 阵亡=" << rep.deaths
			<< " 存活=" << alive
			<< " 造成伤害=" << std::fixed << std::setprecision(1) << rep.damage_dealt << "\n";
	}
}

void print_outcome(int a_total, int b_total, bool time_up) {
	if (time_up) std::cout << "\n== 时间结束 ==\n";
	if (a_total == 0 && b_total == 0)       std::cout << "结果：平局（双方死绝）\n";
	else if (a_total == 0)                  std::cout << "结果：B 胜利\n";
	else if (b_total == 0)                  std::cout << "结果：A 胜利\n";
	else if (a_total > b_total)             std::cout << "结果：A 胜利\n";
	else if (b_total > a_total)             std::cout << "结果：B 胜利\n";
	else                                    std::cout << "结果：平局\n";
}

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
) {
	const int NUM_TESTS = 100;
	int a_wins = 0, b_wins = 0, draws = 0;
	double sum_a = 0, sum_b = 0;

	for (int t = 0; t < NUM_TESTS; ++t) {
		BattleOutcome o = run_battle_multi(
			battlefield_width, a_regiments, b_regiments,
			mult, hit_k, max_rounds, flank_multiplier, flank_target_mode, vertical_fill_delay, rng);
		sum_a += o.a_remaining;
		sum_b += o.b_remaining;
		if (o.a_remaining > 0 && o.b_remaining == 0) ++a_wins;
		else if (o.b_remaining > 0 && o.a_remaining == 0) ++b_wins;
		else ++draws;
	}

	std::cout << "\n========================================\n";
	std::cout << "== 百次测试结果（" << NUM_TESTS << " 次） ==\n";
	std::cout << "========================================\n";
	std::cout << "  A 胜率: " << (a_wins * 100.0 / NUM_TESTS) << "%";
	std::cout << "  平均残余: " << (sum_a / NUM_TESTS) << "\n";
	std::cout << "  B 胜率: " << (b_wins * 100.0 / NUM_TESTS) << "%";
	std::cout << "  平均残余: " << (sum_b / NUM_TESTS) << "\n";
	std::cout << "  平局率: " << (draws * 100.0 / NUM_TESTS) << "%\n";
	std::cout << "========================================\n";
}
