#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "battle.hpp"
#include "io.hpp"
#include "sim.hpp"

static int clamp_non_negative(int count) {
	return std::max(0, count);
}

static std::vector<int> build_backline(int width, int total_count) {
	std::vector<int> backline(width, 0);
	if (width <= 0) {
		return backline;
	}
	int front_count = std::min(width, total_count);
	int remaining = total_count - front_count;
	if (remaining <= 0) {
		return backline;
	}
	std::vector<int> order = center_out_order(width);
	int idx = 0;
	while (remaining > 0) {
		backline[order[idx % width]] += 1;
		--remaining;
		++idx;
	}
	return backline;
}

struct BattleOutcome {
	int a_remaining;
	int b_remaining;
};

static BattleOutcome run_battle(
	int battlefield_width,
	int a_count,
	int b_count,
	const SideInputs& a,
	const SideInputs& b,
	const Multipliers& mult,
	double hit_k,
	int max_rounds,
	double flank_multiplier,
	FlankTargetMode flank_target_mode,
	std::mt19937& rng
) {
	int a_front_count = std::min(battlefield_width, a_count);
	int b_front_count = std::min(battlefield_width, b_count);
	std::vector<int> a_positions = centered_positions(battlefield_width, a_front_count);
	std::vector<int> b_positions = centered_positions(battlefield_width, b_front_count);
	std::vector<UnitState> a_units = init_units(a, mult, battlefield_width, a_positions);
	std::vector<UnitState> b_units = init_units(b, mult, battlefield_width, b_positions);
	std::vector<int> a_backline = build_backline(battlefield_width, a_count);
	std::vector<int> b_backline = build_backline(battlefield_width, b_count);
	std::vector<int> a_empty_rounds(battlefield_width, 0);
	std::vector<int> b_empty_rounds(battlefield_width, 0);

	auto backline_total = [](const std::vector<int>& backline) {
		int total = 0;
		for (int value : backline) {
			total += value;
		}
		return total;
	};

	auto apply_vertical_fill = [&](
		std::vector<UnitState>& units,
		std::vector<int>& backline,
		std::vector<int>& empty_rounds,
		const SideInputs& s
	) {
		for (int i = 0; i < static_cast<int>(units.size()); ++i) {
			if (is_alive(units[i])) {
				empty_rounds[i] = 0;
				continue;
			}
			empty_rounds[i] += 1;
			if (empty_rounds[i] >= 2 && backline[i] > 0) {
				units[i] = initial_unit_state(s, mult);
				units[i].hp *= 0.8;
				backline[i] -= 1;
				empty_rounds[i] = 0;
			}
		}
	};

	int last_a_total = a_count;
	int last_b_total = b_count;

	for (int round = 1; round <= max_rounds; ++round) {
		apply_vertical_fill(a_units, a_backline, a_empty_rounds, a);
		apply_vertical_fill(b_units, b_backline, b_empty_rounds, b);

		apply_lateral_fill(a_units, b_units);
		apply_lateral_fill(b_units, a_units);

		std::vector<bool> a_alive_before(battlefield_width, false);
		std::vector<bool> b_alive_before(battlefield_width, false);
		for (int i = 0; i < battlefield_width; ++i) {
			a_alive_before[i] = is_alive(a_units[i]);
			b_alive_before[i] = is_alive(b_units[i]);
		}

		// 同位 1v1 互砍
		for (int i = 0; i < battlefield_width; ++i) {
			if (is_alive(a_units[i]) && is_alive(b_units[i])) {
				RoundResult a_to_b = simulate_round(a, b, mult, hit_k, round, b_units[i].battle_loss_accum, rng);
				RoundResult b_to_a = simulate_round(b, a, mult, hit_k, round, a_units[i].battle_loss_accum, rng);
				b_units[i].hp -= a_to_b.damage_taken;
				if (b_units[i].hp < 0.0) b_units[i].hp = 0.0;
				a_units[i].hp -= b_to_a.damage_taken;
				if (a_units[i].hp < 0.0) a_units[i].hp = 0.0;
				b_units[i].battle_loss_accum += a_to_b.damage_taken * b.battle_loss_factor;
				a_units[i].battle_loss_accum += b_to_a.damage_taken * a.battle_loss_factor;
			}
		}

		// 夹击
		auto has_adjacent_before = [&](const std::vector<bool>& alive_before, int index) {
			for (int i = index - 1; i >= 0; --i) {
				if (alive_before[i]) return true;
			}
			for (int i = index + 1; i < static_cast<int>(alive_before.size()); ++i) {
				if (alive_before[i]) return true;
			}
			return false;
		};

		auto pick_target = [&](
			const std::vector<UnitState>& defenders,
			const SideInputs& defender_inputs,
			int index
		) {
			auto is_alive_idx = [&](int idx) {
				return idx >= 0 && idx < static_cast<int>(defenders.size()) && defenders[idx].hp > 0.0;
			};
			int left_pick = -1, right_pick = -1;
			int left_dist = 0, right_dist = 0;
			for (int i = index - 1; i >= 0; --i) {
				if (is_alive_idx(i)) { left_pick = i; left_dist = index - i; break; }
			}
			for (int i = index + 1; i < static_cast<int>(defenders.size()); ++i) {
				if (is_alive_idx(i)) { right_pick = i; right_dist = i - index; break; }
			}
			if (left_pick < 0 && right_pick < 0) return -1;
			if (left_pick >= 0 && right_pick < 0) return left_pick;
			if (left_pick < 0 && right_pick >= 0) return right_pick;
			if (left_dist < right_dist) return left_pick;
			if (right_dist < left_dist) return right_pick;
			if (flank_target_mode == FlankTargetMode::Morale) {
				double left_morale = unit_morale(defender_inputs, mult, round, defenders[left_pick]);
				double right_morale = unit_morale(defender_inputs, mult, round, defenders[right_pick]);
				return (left_morale <= right_morale) ? left_pick : right_pick;
			}
			return (defenders[left_pick].hp <= defenders[right_pick].hp) ? left_pick : right_pick;
		};

		for (int i = 0; i < battlefield_width; ++i) {
			if (a_alive_before[i] && !b_alive_before[i] && has_adjacent_before(b_alive_before, i)) {
				int target = pick_target(b_units, b, i);
				if (target >= 0) {
					RoundResult result = simulate_round(a, b, mult, hit_k, round, b_units[target].battle_loss_accum, rng);
					result.damage_taken *= flank_damage_multiplier(flank_multiplier, std::abs(target - i));
					b_units[target].hp -= result.damage_taken;
					if (b_units[target].hp < 0.0) b_units[target].hp = 0.0;
					b_units[target].battle_loss_accum += result.damage_taken * b.battle_loss_factor;
				}
			}
			if (b_alive_before[i] && !a_alive_before[i] && has_adjacent_before(a_alive_before, i)) {
				int target = pick_target(a_units, a, i);
				if (target >= 0) {
					RoundResult result = simulate_round(b, a, mult, hit_k, round, a_units[target].battle_loss_accum, rng);
					result.damage_taken *= flank_damage_multiplier(flank_multiplier, std::abs(target - i));
					a_units[target].hp -= result.damage_taken;
					if (a_units[target].hp < 0.0) a_units[target].hp = 0.0;
					a_units[target].battle_loss_accum += result.damage_taken * a.battle_loss_factor;
				}
			}
		}

		int a_front_alive = count_alive(a_units);
		int b_front_alive = count_alive(b_units);
		last_a_total = a_front_alive + backline_total(a_backline);
		last_b_total = b_front_alive + backline_total(b_backline);

		if (last_a_total == 0 || last_b_total == 0) {
			break;
		}
	}

	BattleOutcome outcome;
	outcome.a_remaining = last_a_total;
	outcome.b_remaining = last_b_total;
	return outcome;
}

static void run_hundred_test(
	int battlefield_width,
	int a_count,
	int b_count,
	const SideInputs& a,
	const SideInputs& b,
	const Multipliers& mult,
	double hit_k,
	int max_rounds,
	double flank_multiplier,
	FlankTargetMode flank_target_mode,
	std::mt19937& rng
) {
	const int NUM_TESTS = 100;
	int a_wins = 0, b_wins = 0, draws = 0;
	double sum_a_survivors = 0, sum_b_survivors = 0;

	for (int test = 1; test <= NUM_TESTS; ++test) {
		BattleOutcome outcome = run_battle(
			battlefield_width, a_count, b_count,
			a, b, mult, hit_k, max_rounds,
			flank_multiplier, flank_target_mode, rng
		);
		sum_a_survivors += outcome.a_remaining;
		sum_b_survivors += outcome.b_remaining;
		if (outcome.a_remaining > 0 && outcome.b_remaining == 0) ++a_wins;
		else if (outcome.b_remaining > 0 && outcome.a_remaining == 0) ++b_wins;
		else ++draws;
	}

	std::cout << "\n========================================\n";
	std::cout << "== 百次测试结果（" << NUM_TESTS << " 次） ==\n";
	std::cout << "========================================\n";
	std::cout << "  A 胜率: " << (a_wins * 100.0 / NUM_TESTS) << "%";
	std::cout << "  平均残余: " << (sum_a_survivors / NUM_TESTS) << "\n";
	std::cout << "  B 胜率: " << (b_wins * 100.0 / NUM_TESTS) << "%";
	std::cout << "  平均残余: " << (sum_b_survivors / NUM_TESTS) << "\n";
	std::cout << "  平局率: " << (draws * 100.0 / NUM_TESTS) << "%\n";
	std::cout << "========================================\n";
}

int main() {
	std::random_device rd;
	std::mt19937 rng(rd());

	double hit_k = 10.0;
	Multipliers mult;
	mult.battle_type = 1.0;
	mult.special = 1.0;
	int max_rounds = 10;
	int battlefield_width = 10;
	int a_count = 3;
	int b_count = 3;
	double flank_multiplier = 0.4;

	std::cout << "\n== 全局参数（可快速输入） ==\n";
	std::cout << "快速输入：命中 k 战斗类型乘数 特殊情况乘数 最大回合数 战场宽度 n A 队伍数量 B 队伍数量 夹击倍率\n";
	std::cout << "直接回车则逐项输入。\n";
	std::cout << "快速输入：";
	std::string global_line;
	std::getline(std::cin, global_line);
	std::istringstream global_iss(global_line);
	std::vector<double> global_values;
	for (double value = 0.0; global_iss >> value;) {
		global_values.push_back(value);
	}

	if (global_values.empty()) {
		hit_k = read_double("命中公式 k", hit_k);
		mult.battle_type = read_double("战斗类型乘数", mult.battle_type);
		mult.special = read_double("特殊情况乘数", mult.special);
		max_rounds = read_int("最大回合数", max_rounds);
		battlefield_width = read_int("战场宽度 n", battlefield_width);
		a_count = read_int("A 队伍数量", a_count);
		b_count = read_int("B 队伍数量", b_count);
		flank_multiplier = read_double("夹击倍率", flank_multiplier);
	} else {
		if (global_values.size() > 0) hit_k = global_values[0];
		if (global_values.size() > 1) mult.battle_type = global_values[1];
		if (global_values.size() > 2) mult.special = global_values[2];
		if (global_values.size() > 3) max_rounds = static_cast<int>(global_values[3]);
		if (global_values.size() > 4) battlefield_width = static_cast<int>(global_values[4]);
		if (global_values.size() > 5) a_count = static_cast<int>(global_values[5]);
		if (global_values.size() > 6) b_count = static_cast<int>(global_values[6]);
		if (global_values.size() > 7) flank_multiplier = global_values[7];
	}
	FlankTargetMode flank_target_mode = FlankTargetMode::Hp;
	max_rounds = std::max(1, max_rounds);
	battlefield_width = std::max(1, battlefield_width);
	a_count = clamp_non_negative(a_count);
	b_count = clamp_non_negative(b_count);

	SideInputs default_a;
	SideInputs default_b;
	default_a.name = "A";
	default_b.name = "B";

	SideInputs a = read_side_inputs("Side A", default_a);
	SideInputs b = read_side_inputs("Side B", default_b);

	// ==== 主循环 ====
	bool keep_running = true;
	while (keep_running) {
		std::cout << "\n== 选择操作 ==\n";
		std::cout << "1) 单次推演（逐回合查看战报）\n";
		std::cout << "2) 百次测试（快速测试 100 场，输出胜率与平均残余）\n";
		std::cout << "3) 修改全局参数\n";
		std::cout << "4) 修改 Side A 参数\n";
		std::cout << "5) 修改 Side B 参数\n";
		std::cout << "0) 退出\n";

		int init_choice = read_int("请选择", 1);
		if (init_choice == 0) {
			keep_running = false;
			break;
		}
		if (init_choice == 2) {
			run_hundred_test(battlefield_width, a_count, b_count,
				a, b, mult, hit_k, max_rounds,
				flank_multiplier, flank_target_mode, rng);
			continue;
		}
		if (init_choice == 3) {
			edit_global_menu(hit_k, mult, max_rounds, battlefield_width, a_count, b_count, flank_multiplier, flank_target_mode);
			continue;
		}
		if (init_choice == 4) {
			edit_side_menu(a);
			continue;
		}
		if (init_choice == 5) {
			edit_side_menu(b);
			continue;
		}

		std::cout << "\n== 推演开始 ==\n";
		std::cout << std::fixed << std::setprecision(4);

		int a_front_count = std::min(battlefield_width, a_count);
		int b_front_count = std::min(battlefield_width, b_count);
		std::vector<int> a_positions = centered_positions(battlefield_width, a_front_count);
		std::vector<int> b_positions = centered_positions(battlefield_width, b_front_count);
		std::vector<UnitState> a_units = init_units(a, mult, battlefield_width, a_positions);
		std::vector<UnitState> b_units = init_units(b, mult, battlefield_width, b_positions);
		std::vector<int> a_backline = build_backline(battlefield_width, a_count);
		std::vector<int> b_backline = build_backline(battlefield_width, b_count);
		std::vector<int> a_empty_rounds(battlefield_width, 0);
		std::vector<int> b_empty_rounds(battlefield_width, 0);

		bool ended_by_elimination = false;
		int last_a_total = a_count;
		int last_b_total = b_count;
		auto backline_total = [](const std::vector<int>& backline) {
			int total = 0;
			for (int value : backline) {
				total += value;
			}
			return total;
		};
		auto print_outcome = [&](int a_total, int b_total, bool time_up) {
			if (time_up) {
				std::cout << "\n== 时间结束 ==\n";
			}
			if (a_total == 0 && b_total == 0) {
				std::cout << "结果：平局（双方死绝）\n";
			} else if (a_total == 0) {
				std::cout << "结果：B 胜利\n";
			} else if (b_total == 0) {
				std::cout << "结果：A 胜利\n";
			} else if (a_total > b_total) {
				std::cout << "结果：A 胜利\n";
			} else if (b_total > a_total) {
				std::cout << "结果：B 胜利\n";
			} else {
				std::cout << "结果：平局\n";
			}
		};

		for (int round = 1; round <= max_rounds; ++round) {
			// 纵向补位：空位持续两回合后由后排补入
			auto apply_vertical_fill = [&](
				std::vector<UnitState>& units,
				std::vector<int>& backline,
				std::vector<int>& empty_rounds,
				const SideInputs& s
			) {
				for (int i = 0; i < static_cast<int>(units.size()); ++i) {
					if (is_alive(units[i])) {
						empty_rounds[i] = 0;
						continue;
					}
					empty_rounds[i] += 1;
					if (empty_rounds[i] >= 2 && backline[i] > 0) {
						units[i] = initial_unit_state(s, mult);
						units[i].hp *= 0.8;
						backline[i] -= 1;
						empty_rounds[i] = 0;
					}
				}
			};

			apply_vertical_fill(a_units, a_backline, a_empty_rounds, a);
			apply_vertical_fill(b_units, b_backline, b_empty_rounds, b);

			// 横向补位：回合开始时正面无敌军，优先向中轴线一侧补位
			apply_lateral_fill(
				a_units,
				b_units
			);
			apply_lateral_fill(
				b_units,
				a_units
			);
			std::string a_after_fill = alive_line(a_units);
			std::string b_after_fill = alive_line(b_units);

			std::vector<bool> a_alive_before(battlefield_width, false);
			std::vector<bool> b_alive_before(battlefield_width, false);
			std::vector<double> a_hp_before_damage(battlefield_width, 0.0);
			std::vector<double> b_hp_before_damage(battlefield_width, 0.0);
			for (int i = 0; i < battlefield_width; ++i) {
				a_alive_before[i] = is_alive(a_units[i]);
				b_alive_before[i] = is_alive(b_units[i]);
				a_hp_before_damage[i] = a_units[i].hp;
				b_hp_before_damage[i] = b_units[i].hp;
			}

			std::vector<RoundResult> a_to_b(battlefield_width);
			std::vector<RoundResult> b_to_a(battlefield_width);
			std::vector<bool> engaged(battlefield_width, false);

			// 同位 1v1 互砍
			for (int i = 0; i < battlefield_width; ++i) {
				if (is_alive(a_units[i]) && is_alive(b_units[i])) {
					a_to_b[i] = simulate_round(a, b, mult, hit_k, round, b_units[i].battle_loss_accum, rng);
					b_to_a[i] = simulate_round(b, a, mult, hit_k, round, a_units[i].battle_loss_accum, rng);
					engaged[i] = true;
				}
			}

			// 结算同位伤害
			for (int i = 0; i < battlefield_width; ++i) {
				if (!engaged[i]) {
					continue;
				}
				b_units[i].hp -= a_to_b[i].damage_taken;
				if (b_units[i].hp < 0.0) b_units[i].hp = 0.0;
				a_units[i].hp -= b_to_a[i].damage_taken;
				if (a_units[i].hp < 0.0) a_units[i].hp = 0.0;

				b_units[i].battle_loss_accum += a_to_b[i].damage_taken * b.battle_loss_factor;
				a_units[i].battle_loss_accum += b_to_a[i].damage_taken * a.battle_loss_factor;
			}

			struct FlankAttack {
				bool attacker_is_a = true;
				int attacker_index = -1;
				int target_index = -1;
				int distance = 0;
				RoundResult result;
			};

			// 夹击触发：回合开始时正面为空，且左右任一侧存在敌军
			std::vector<FlankAttack> flanks;
			auto has_adjacent_before = [&](const std::vector<bool>& alive_before, int index) {
				for (int i = index - 1; i >= 0; --i) {
					if (alive_before[i]) return true;
				}
				for (int i = index + 1; i < static_cast<int>(alive_before.size()); ++i) {
					if (alive_before[i]) return true;
				}
				return false;
			};
			auto pick_target = [&](
				const std::vector<UnitState>& defenders,
				const SideInputs& defender_inputs,
				int index
			) {
				auto is_alive_index = [&](int idx) {
					return idx >= 0 && idx < static_cast<int>(defenders.size()) && defenders[idx].hp > 0.0;
				};

				int left_pick = -1;
				int right_pick = -1;
				int left_dist = 0;
				int right_dist = 0;
				for (int i = index - 1; i >= 0; --i) {
					if (is_alive_index(i)) {
						left_pick = i;
						left_dist = index - i;
						break;
					}
				}
				for (int i = index + 1; i < static_cast<int>(defenders.size()); ++i) {
					if (is_alive_index(i)) {
						right_pick = i;
						right_dist = i - index;
						break;
					}
				}

				if (left_pick < 0 && right_pick < 0) return -1;
				if (left_pick >= 0 && right_pick < 0) return left_pick;
				if (left_pick < 0 && right_pick >= 0) return right_pick;

				if (left_dist < right_dist) return left_pick;
				if (right_dist < left_dist) return right_pick;

				if (flank_target_mode == FlankTargetMode::Morale) {
					double left_morale = unit_morale(defender_inputs, mult, round, defenders[left_pick]);
					double right_morale = unit_morale(defender_inputs, mult, round, defenders[right_pick]);
					return (left_morale <= right_morale) ? left_pick : right_pick;
				}

				return (defenders[left_pick].hp <= defenders[right_pick].hp) ? left_pick : right_pick;
			};

			for (int i = 0; i < battlefield_width; ++i) {
				if (a_alive_before[i] && !b_alive_before[i] && has_adjacent_before(b_alive_before, i)) {
					int target = pick_target(b_units, b, i);
					if (target >= 0) {
						FlankAttack flank;
						flank.attacker_is_a = true;
						flank.attacker_index = i;
						flank.target_index = target;
						flank.distance = std::abs(target - i);
						flank.result = simulate_round(a, b, mult, hit_k, round, b_units[target].battle_loss_accum, rng);
						flank.result.damage_taken *= flank_damage_multiplier(flank_multiplier, flank.distance);
						flanks.push_back(flank);
					}
				}
				if (b_alive_before[i] && !a_alive_before[i] && has_adjacent_before(a_alive_before, i)) {
					int target = pick_target(a_units, a, i);
					if (target >= 0) {
						FlankAttack flank;
						flank.attacker_is_a = false;
						flank.attacker_index = i;
						flank.target_index = target;
						flank.distance = std::abs(target - i);
						flank.result = simulate_round(b, a, mult, hit_k, round, a_units[target].battle_loss_accum, rng);
						flank.result.damage_taken *= flank_damage_multiplier(flank_multiplier, flank.distance);
						flanks.push_back(flank);
					}
				}
			}

			int flank_hits = 0;
			// 结算夹击伤害
			for (const auto& flank : flanks) {
				if (flank.attacker_is_a) {
					b_units[flank.target_index].hp -= flank.result.damage_taken;
					if (b_units[flank.target_index].hp < 0.0) b_units[flank.target_index].hp = 0.0;
					b_units[flank.target_index].battle_loss_accum += flank.result.damage_taken * b.battle_loss_factor;
				} else {
					a_units[flank.target_index].hp -= flank.result.damage_taken;
					if (a_units[flank.target_index].hp < 0.0) a_units[flank.target_index].hp = 0.0;
					a_units[flank.target_index].battle_loss_accum += flank.result.damage_taken * a.battle_loss_factor;
				}
				if (flank.result.hit) {
					++flank_hits;
				}
			}
			std::string a_after_damage = alive_line(a_units);
			std::string b_after_damage = alive_line(b_units);

			std::vector<int> a_new_dead;
			std::vector<int> b_new_dead;
			for (int i = 0; i < battlefield_width; ++i) {
				if (a_alive_before[i] && !is_alive(a_units[i])) {
					a_new_dead.push_back(i + 1);
				}
				if (b_alive_before[i] && !is_alive(b_units[i])) {
					b_new_dead.push_back(i + 1);
				}
			}

			std::cout << "\n-- 第 " << round << " 回合战报 --\n";
			int a_front_alive = count_alive(a_units);
			int b_front_alive = count_alive(b_units);
			int a_backline_alive = backline_total(a_backline);
			int b_backline_alive = backline_total(b_backline);
			last_a_total = a_front_alive + a_backline_alive;
			last_b_total = b_front_alive + b_backline_alive;
			std::cout << "A 存活=" << last_a_total << " / " << a_count << "\n";
			std::cout << "B 存活=" << last_b_total << " / " << b_count << "\n";
			std::cout << "A 后排待命=" << a_backline_alive << "\n";
			std::cout << "B 后排待命=" << b_backline_alive << "\n";
			std::cout << "补位后 A=" << a_after_fill << "\n";
			std::cout << "补位后 B=" << b_after_fill << "\n";
			std::cout << "结算后 A=" << a_after_damage << "\n";
			std::cout << "结算后 B=" << b_after_damage << "\n";
			if (a_new_dead.empty()) {
				std::cout << "A 本回合无人阵亡\n";
			} else {
				std::cout << "A 本回合阵亡位置：";
				for (size_t i = 0; i < a_new_dead.size(); ++i) {
					if (i > 0) std::cout << "、";
					std::cout << a_new_dead[i];
				}
				std::cout << "\n";
			}
			if (b_new_dead.empty()) {
				std::cout << "B 本回合无人阵亡\n";
			} else {
				std::cout << "B 本回合阵亡位置：";
				for (size_t i = 0; i < b_new_dead.size(); ++i) {
					if (i > 0) std::cout << "、";
					std::cout << b_new_dead[i];
				}
				std::cout << "\n";
			}
			std::cout << "夹击次数=" << flanks.size() << " 命中=" << flank_hits << " 倍率=" << flank_multiplier << "（距离越远伤害越低）\n";

			std::cout << "\n如需查看位置详情，输入位置编号（空格分隔，1-" << battlefield_width << "），直接回车跳过：";
			std::string detail_line;
			std::getline(std::cin, detail_line);
			std::vector<int> detail_indices = parse_indices(detail_line, battlefield_width);
			for (int idx : detail_indices) {
				std::cout << "\n== 位置 " << (idx + 1) << " 详细战报 ==\n";
				if (engaged[idx]) {
					std::cout << "A -> B\n";
					std::cout << "  B 受伤前血量:      " << b_hp_before_damage[idx] << "\n";
					std::cout << "  基础伤害:          " << a_to_b[idx].base_damage << "\n";
					std::cout << "  命中率:            " << a_to_b[idx].hit_chance * 100.0 << "%\n";
					std::cout << "  防御乘数:          " << a_to_b[idx].defense_multiplier << "\n";
					std::cout << "  命中判定:          " << (a_to_b[idx].hit ? "命中" : "未命中") << "\n";
					std::cout << "  防御判定:          " << a_to_b[idx].defense_tier << "\n";
					std::cout << "  实际承受伤害:      " << a_to_b[idx].damage_taken << "\n";
					std::cout << "  B 血量:            " << b_units[idx].hp << "\n";

					std::cout << "B -> A\n";
					std::cout << "  A 受伤前血量:      " << a_hp_before_damage[idx] << "\n";
					std::cout << "  基础伤害:          " << b_to_a[idx].base_damage << "\n";
					std::cout << "  命中率:            " << b_to_a[idx].hit_chance * 100.0 << "%\n";
					std::cout << "  防御乘数:          " << b_to_a[idx].defense_multiplier << "\n";
					std::cout << "  命中判定:          " << (b_to_a[idx].hit ? "命中" : "未命中") << "\n";
					std::cout << "  防御判定:          " << b_to_a[idx].defense_tier << "\n";
					std::cout << "  实际承受伤害:      " << b_to_a[idx].damage_taken << "\n";
					std::cout << "  A 血量:            " << a_units[idx].hp << "\n";
				} else if (is_alive(a_units[idx]) && !is_alive(b_units[idx])) {
					std::cout << "B 已被击溃，A 待命\n";
				} else if (!is_alive(a_units[idx]) && is_alive(b_units[idx])) {
					std::cout << "A 已被击溃，B 待命\n";
				} else {
					std::cout << "双方已空\n";
				}

				bool has_flank = false;
				for (const auto& flank : flanks) {
					if (flank.attacker_index == idx || flank.target_index == idx) {
						if (!has_flank) {
							std::cout << "夹击记录\n";
							has_flank = true;
						}
						if (flank.attacker_is_a) {
							std::cout << "A[" << (flank.attacker_index + 1) << "] -> B[" << (flank.target_index + 1) << "]\n";
						} else {
							std::cout << "B[" << (flank.attacker_index + 1) << "] -> A[" << (flank.target_index + 1) << "]\n";
						}
						double final_multiplier = flank_damage_multiplier(flank_multiplier, flank.distance);
						std::cout << "  夹击距离:          " << flank.distance << "\n";
						std::cout << "  最终倍率:          " << final_multiplier << "\n";
						std::cout << "  基础伤害:          " << flank.result.base_damage << "\n";
						std::cout << "  命中率:            " << flank.result.hit_chance * 100.0 << "%\n";
						std::cout << "  防御乘数:          " << flank.result.defense_multiplier << "\n";
						std::cout << "  命中判定:          " << (flank.result.hit ? "命中" : "未命中") << "\n";
						std::cout << "  防御判定:          " << flank.result.defense_tier << "\n";
						std::cout << "  实际承受伤害:      " << flank.result.damage_taken << "\n";
					}
				}
			}

			if (last_a_total == 0 || last_b_total == 0) {
				std::cout << "\n== 结束：一方队伍被清空 ==\n";
				print_outcome(last_a_total, last_b_total, false);
				ended_by_elimination = true;
				break;
			}

			std::cout << "\n按回车继续下一回合...";
			std::string wait_line;
			std::getline(std::cin, wait_line);
		}

		if (!ended_by_elimination) {
			print_outcome(last_a_total, last_b_total, true);
		}
		std::cout << "\n== 推演结束 ==\n";
		std::cout << "\n== 后续操作 ==\n";
		std::cout << "1) 直接再次推演\n";
		std::cout << "2) 百次测试（快速测试 100 场，输出胜率与平均残余）\n";
		std::cout << "3) 修改全局参数\n";
		std::cout << "4) 修改 Side A 参数\n";
		std::cout << "5) 修改 Side B 参数\n";
		std::cout << "0) 退出\n";

		int choice = read_int("请选择", 1);
		switch (choice) {
		case 1:
			break;
		case 2:
			run_hundred_test(battlefield_width, a_count, b_count,
				a, b, mult, hit_k, max_rounds,
				flank_multiplier, flank_target_mode, rng);
			break;
		case 3:
			edit_global_menu(hit_k, mult, max_rounds, battlefield_width, a_count, b_count, flank_multiplier, flank_target_mode);
			break;
		case 4:
			edit_side_menu(a);
			break;
		case 5:
			edit_side_menu(b);
			break;
		case 0:
			keep_running = false;
			break;
		default:
			std::cout << "无效选项，默认再次推演。\n";
			break;
		}
	}

	return 0;
}
