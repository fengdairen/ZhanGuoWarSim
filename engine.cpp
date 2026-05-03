#include "engine.hpp"

#include <algorithm>
#include <cmath>

#include "battle.hpp"
#include "sim.hpp"

static const SideInputs& get_side_inputs(const std::vector<Regiment>& regs, int idx) {
	return regs[idx].stats;
}

int backline_queue_total(const std::vector<std::vector<int>>& queues) {
	int total = 0;
	for (const auto& q : queues) total += static_cast<int>(q.size());
	return total;
}

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
	std::vector<RegimentReport>* a_reports_out,
	std::vector<RegimentReport>* b_reports_out
) {
	DeployResult a_deploy = deploy_regiments(a_regiments, mult, battlefield_width);
	DeployResult b_deploy = deploy_regiments(b_regiments, mult, battlefield_width);
	std::vector<UnitState> a_units = std::move(a_deploy.front_units);
	std::vector<UnitState> b_units = std::move(b_deploy.front_units);
	std::vector<std::vector<int>> a_backline = std::move(a_deploy.backline_queue);
	std::vector<std::vector<int>> b_backline = std::move(b_deploy.backline_queue);
	std::vector<int> a_empty_rounds(battlefield_width, 0);
	std::vector<int> b_empty_rounds(battlefield_width, 0);

	// 初始化兵种报告
	std::vector<RegimentReport> a_reports, b_reports;
	for (int r = 0; r < static_cast<int>(a_regiments.size()); ++r) {
		a_reports.push_back({ "A#" + std::to_string(r + 1), a_regiments[r].count, 0, 0.0 });
	}
	for (int r = 0; r < static_cast<int>(b_regiments.size()); ++r) {
		b_reports.push_back({ "B#" + std::to_string(r + 1), b_regiments[r].count, 0, 0.0 });
	}

	auto get_a_inputs = [&](int reg_idx) -> const SideInputs& { return a_regiments[reg_idx].stats; };
	auto get_b_inputs = [&](int reg_idx) -> const SideInputs& { return b_regiments[reg_idx].stats; };

	for (int round = 1; round <= max_rounds; ++round) {
		// ----- 纵向补位 -----
		auto apply_vertical_fill = [&](
			std::vector<UnitState>& units,
			std::vector<std::vector<int>>& backline,
			std::vector<int>& empty_rounds,
			const std::vector<Regiment>& regs
		) {
			for (int i = 0; i < static_cast<int>(units.size()); ++i) {
				if (is_alive(units[i])) { empty_rounds[i] = 0; continue; }
				empty_rounds[i] += 1;
				if (empty_rounds[i] >= vertical_fill_delay && !backline[i].empty()) {
					int reg_idx = backline[i].front();
					backline[i].erase(backline[i].begin());
					units[i] = initial_unit_state(regs[reg_idx].stats, mult, reg_idx);
					units[i].hp *= 0.8;
					empty_rounds[i] = 0;
				}
			}
		};
		apply_vertical_fill(a_units, a_backline, a_empty_rounds, a_regiments);
		apply_vertical_fill(b_units, b_backline, b_empty_rounds, b_regiments);

		// ----- 横向补位 -----
		apply_lateral_fill(a_units, b_units);
		apply_lateral_fill(b_units, a_units);

		// 快照战斗前状态
		std::vector<bool> a_alive_before(battlefield_width, false);
		std::vector<bool> b_alive_before(battlefield_width, false);
		for (int i = 0; i < battlefield_width; ++i) {
			a_alive_before[i] = is_alive(a_units[i]);
			b_alive_before[i] = is_alive(b_units[i]);
		}

		// ----- 同位 1v1 互砍 -----
		for (int i = 0; i < battlefield_width; ++i) {
			if (!is_alive(a_units[i]) || !is_alive(b_units[i])) continue;
			int a_reg = a_units[i].regiment_idx;
			int b_reg = b_units[i].regiment_idx;
			const SideInputs& a_s = get_a_inputs(a_reg);
			const SideInputs& b_s = get_b_inputs(b_reg);

			RoundResult a_to_b = simulate_round(a_s, b_s, mult, hit_k, round, b_units[i].battle_loss_accum, rng);
			RoundResult b_to_a = simulate_round(b_s, a_s, mult, hit_k, round, a_units[i].battle_loss_accum, rng);

			b_units[i].hp -= a_to_b.damage_taken;
			if (b_units[i].hp < 0.0) b_units[i].hp = 0.0;
			a_units[i].hp -= b_to_a.damage_taken;
			if (a_units[i].hp < 0.0) a_units[i].hp = 0.0;

			b_units[i].battle_loss_accum += a_to_b.damage_taken * b_s.battle_loss_factor;
			a_units[i].battle_loss_accum += b_to_a.damage_taken * a_s.battle_loss_factor;

			if (a_reg >= 0 && a_reg < static_cast<int>(a_reports.size()))
				a_reports[a_reg].damage_dealt += a_to_b.damage_taken;
			if (b_reg >= 0 && b_reg < static_cast<int>(b_reports.size()))
				b_reports[b_reg].damage_dealt += b_to_a.damage_taken;
		}

		// ----- 夹击 -----
		auto has_adjacent_before = [&](const std::vector<bool>& alive_before, int idx) {
			for (int j = idx - 1; j >= 0; --j) if (alive_before[j]) return true;
			for (int j = idx + 1; j < static_cast<int>(alive_before.size()); ++j) if (alive_before[j]) return true;
			return false;
		};
		auto pick_target = [&](const std::vector<UnitState>& defenders, const std::vector<Regiment>& def_regs, int idx) {
			auto is_alive_idx = [&](int j) { return j >= 0 && j < static_cast<int>(defenders.size()) && defenders[j].hp > 0.0; };
			int lp = -1, rp = -1, ld = 0, rd = 0;
			for (int j = idx - 1; j >= 0; --j) if (is_alive_idx(j)) { lp = j; ld = idx - j; break; }
			for (int j = idx + 1; j < static_cast<int>(defenders.size()); ++j) if (is_alive_idx(j)) { rp = j; rd = j - idx; break; }
			if (lp < 0 && rp < 0) return -1;
			if (lp >= 0 && rp < 0) return lp;
			if (lp < 0 && rp >= 0) return rp;
			if (ld < rd) return lp;
			if (rd < ld) return rp;
			if (flank_target_mode == FlankTargetMode::Morale) {
				const SideInputs& ls = get_side_inputs(def_regs, defenders[lp].regiment_idx);
				const SideInputs& rs = get_side_inputs(def_regs, defenders[rp].regiment_idx);
				double lm = unit_morale(ls, mult, round, defenders[lp]);
				double rm = unit_morale(rs, mult, round, defenders[rp]);
				return (lm <= rm) ? lp : rp;
			}
			return (defenders[lp].hp <= defenders[rp].hp) ? lp : rp;
		};

		// A 夹击 B
		for (int i = 0; i < battlefield_width; ++i) {
			if (a_alive_before[i] && !b_alive_before[i] && has_adjacent_before(b_alive_before, i)) {
				int target = pick_target(b_units, b_regiments, i);
				if (target < 0) continue;
				int a_reg = a_units[i].regiment_idx;
				int b_reg = b_units[target].regiment_idx;
				const SideInputs& a_s = get_a_inputs(a_reg);
				const SideInputs& b_s = get_b_inputs(b_reg);
				RoundResult result = simulate_round(a_s, b_s, mult, hit_k, round, b_units[target].battle_loss_accum, rng);
				result.damage_taken *= flank_damage_multiplier(flank_multiplier, std::abs(target - i));
				b_units[target].hp -= result.damage_taken;
				if (b_units[target].hp < 0.0) b_units[target].hp = 0.0;
				b_units[target].battle_loss_accum += result.damage_taken * b_s.battle_loss_factor;
				if (a_reg >= 0 && a_reg < static_cast<int>(a_reports.size()))
					a_reports[a_reg].damage_dealt += result.damage_taken;
			}
		}
		// B 夹击 A
		for (int i = 0; i < battlefield_width; ++i) {
			if (b_alive_before[i] && !a_alive_before[i] && has_adjacent_before(a_alive_before, i)) {
				int target = pick_target(a_units, a_regiments, i);
				if (target < 0) continue;
				int b_reg = b_units[i].regiment_idx;
				int a_reg = a_units[target].regiment_idx;
				const SideInputs& b_s = get_b_inputs(b_reg);
				const SideInputs& a_s = get_a_inputs(a_reg);
				RoundResult result = simulate_round(b_s, a_s, mult, hit_k, round, a_units[target].battle_loss_accum, rng);
				result.damage_taken *= flank_damage_multiplier(flank_multiplier, std::abs(target - i));
				a_units[target].hp -= result.damage_taken;
				if (a_units[target].hp < 0.0) a_units[target].hp = 0.0;
				a_units[target].battle_loss_accum += result.damage_taken * a_s.battle_loss_factor;
				if (b_reg >= 0 && b_reg < static_cast<int>(b_reports.size()))
					b_reports[b_reg].damage_dealt += result.damage_taken;
			}
		}

		// 统计阵亡
		for (int i = 0; i < battlefield_width; ++i) {
			if (a_alive_before[i] && !is_alive(a_units[i])) {
				int r = a_units[i].regiment_idx;
				if (r >= 0 && r < static_cast<int>(a_reports.size())) a_reports[r].deaths += 1;
			}
			if (b_alive_before[i] && !is_alive(b_units[i])) {
				int r = b_units[i].regiment_idx;
				if (r >= 0 && r < static_cast<int>(b_reports.size())) b_reports[r].deaths += 1;
			}
		}

		// 检查全灭
		int a_total = count_alive(a_units) + backline_queue_total(a_backline);
		int b_total = count_alive(b_units) + backline_queue_total(b_backline);
		if (a_total == 0 || b_total == 0) break;
	}

	if (a_reports_out) *a_reports_out = std::move(a_reports);
	if (b_reports_out) *b_reports_out = std::move(b_reports);

	int a_total = count_alive(a_units) + backline_queue_total(a_backline);
	int b_total = count_alive(b_units) + backline_queue_total(b_backline);
	return { a_total, b_total };
}
