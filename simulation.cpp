#include "simulation.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "battle.hpp"
#include "engine.hpp"
#include "io.hpp"
#include "report.hpp"
#include "sim.hpp"

static const SideInputs& get_side_inputs(const std::vector<Regiment>& regs, int idx) {
	return regs[idx].stats;
}

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
) {
	std::cout << "\n== 推演开始 ==\n";
	std::cout << std::fixed << std::setprecision(4);

	DeployResult a_deploy = deploy_regiments(a_regs, mult, battlefield_width);
	DeployResult b_deploy = deploy_regiments(b_regs, mult, battlefield_width);
	std::vector<UnitState> a_units = std::move(a_deploy.front_units);
	std::vector<UnitState> b_units = std::move(b_deploy.front_units);
	std::vector<std::vector<int>> a_backline = std::move(a_deploy.backline_queue);
	std::vector<std::vector<int>> b_backline = std::move(b_deploy.backline_queue);
	std::vector<int> a_empty_rounds(battlefield_width, 0);
	std::vector<int> b_empty_rounds(battlefield_width, 0);

	int a_total_init = total_regiment_count(a_regs);
	int b_total_init = total_regiment_count(b_regs);

	bool ended_by_elimination = false;
	int last_a_total = a_total_init, last_b_total = b_total_init;

	auto get_a_inputs = [&](int reg_idx) -> const SideInputs& { return a_regs[reg_idx].stats; };
	auto get_b_inputs = [&](int reg_idx) -> const SideInputs& { return b_regs[reg_idx].stats; };

	// 兵种战损追踪
	std::vector<RegimentReport> a_reports, b_reports;
	for (int r = 0; r < static_cast<int>(a_regs.size()); ++r)
		a_reports.push_back({ "A#" + std::to_string(r + 1), a_regs[r].count, 0, 0.0 });
	for (int r = 0; r < static_cast<int>(b_regs.size()); ++r)
		b_reports.push_back({ "B#" + std::to_string(r + 1), b_regs[r].count, 0, 0.0 });

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
		apply_vertical_fill(a_units, a_backline, a_empty_rounds, a_regs);
		apply_vertical_fill(b_units, b_backline, b_empty_rounds, b_regs);

		// ----- 横向补位 -----
		apply_lateral_fill(a_units, b_units);
		apply_lateral_fill(b_units, a_units);
		std::string a_after_fill = alive_line(a_units);
		std::string b_after_fill = alive_line(b_units);

		// 快照
		std::vector<bool> a_alive_before(battlefield_width, false);
		std::vector<bool> b_alive_before(battlefield_width, false);
		std::vector<double> a_hp_before(battlefield_width, 0.0);
		std::vector<double> b_hp_before(battlefield_width, 0.0);
		for (int i = 0; i < battlefield_width; ++i) {
			a_alive_before[i] = is_alive(a_units[i]);
			b_alive_before[i] = is_alive(b_units[i]);
			a_hp_before[i] = a_units[i].hp;
			b_hp_before[i] = b_units[i].hp;
		}

		std::vector<RoundResult> a_to_b(battlefield_width);
		std::vector<RoundResult> b_to_a(battlefield_width);
		std::vector<bool> engaged(battlefield_width, false);

		// ----- 1v1 -----
		for (int i = 0; i < battlefield_width; ++i) {
			if (!is_alive(a_units[i]) || !is_alive(b_units[i])) continue;
			engaged[i] = true;
			int a_reg = a_units[i].regiment_idx;
			int b_reg = b_units[i].regiment_idx;
			const SideInputs& a_s = get_a_inputs(a_reg);
			const SideInputs& b_s = get_b_inputs(b_reg);
			a_to_b[i] = simulate_round(a_s, b_s, mult, hit_k, round, b_units[i].battle_loss_accum, rng);
			b_to_a[i] = simulate_round(b_s, a_s, mult, hit_k, round, a_units[i].battle_loss_accum, rng);
		}
		for (int i = 0; i < battlefield_width; ++i) {
			if (!engaged[i]) continue;
			int a_reg = a_units[i].regiment_idx;
			int b_reg = b_units[i].regiment_idx;
			b_units[i].hp -= a_to_b[i].damage_taken;
			if (b_units[i].hp < 0.0) b_units[i].hp = 0.0;
			a_units[i].hp -= b_to_a[i].damage_taken;
			if (a_units[i].hp < 0.0) a_units[i].hp = 0.0;
			b_units[i].battle_loss_accum += a_to_b[i].damage_taken * get_b_inputs(b_reg).battle_loss_factor;
			a_units[i].battle_loss_accum += b_to_a[i].damage_taken * get_a_inputs(a_reg).battle_loss_factor;
			if (a_reg >= 0 && a_reg < static_cast<int>(a_reports.size()))
				a_reports[a_reg].damage_dealt += a_to_b[i].damage_taken;
			if (b_reg >= 0 && b_reg < static_cast<int>(b_reports.size()))
				b_reports[b_reg].damage_dealt += b_to_a[i].damage_taken;
		}

		// ----- 夹击 -----
		struct FlankRec { bool is_a; int src; int dst; int dist; RoundResult res; };
		std::vector<FlankRec> flanks;

		auto has_adj = [&](const std::vector<bool>& alive_before, int idx) {
			for (int j = idx - 1; j >= 0; --j) if (alive_before[j]) return true;
			for (int j = idx + 1; j < battlefield_width; ++j) if (alive_before[j]) return true;
			return false;
		};
		auto pick_target = [&](const std::vector<UnitState>& defs, const std::vector<Regiment>& def_regs, int idx) {
			auto alive_idx = [&](int j) { return j >= 0 && j < battlefield_width && defs[j].hp > 0.0; };
			int lp = -1, rp = -1, ld = 0, rd = 0;
			for (int j = idx - 1; j >= 0; --j) if (alive_idx(j)) { lp = j; ld = idx - j; break; }
			for (int j = idx + 1; j < battlefield_width; ++j) if (alive_idx(j)) { rp = j; rd = j - idx; break; }
			if (lp < 0 && rp < 0) return -1;
			if (lp >= 0 && rp < 0) return lp;
			if (lp < 0 && rp >= 0) return rp;
			if (ld < rd) return lp;
			if (rd < ld) return rp;
			if (flank_target_mode == FlankTargetMode::Morale) {
				double lm = unit_morale(get_side_inputs(def_regs, defs[lp].regiment_idx), mult, round, defs[lp]);
				double rm = unit_morale(get_side_inputs(def_regs, defs[rp].regiment_idx), mult, round, defs[rp]);
				return (lm <= rm) ? lp : rp;
			}
			return (defs[lp].hp <= defs[rp].hp) ? lp : rp;
		};

		for (int i = 0; i < battlefield_width; ++i) {
			if (a_alive_before[i] && !b_alive_before[i] && has_adj(b_alive_before, i)) {
				int t = pick_target(b_units, b_regs, i);
				if (t >= 0) {
					int a_reg = a_units[i].regiment_idx;
					int b_reg = b_units[t].regiment_idx;
					const SideInputs& a_s = get_a_inputs(a_reg);
					const SideInputs& b_s = get_b_inputs(b_reg);
					RoundResult res = simulate_round(a_s, b_s, mult, hit_k, round, b_units[t].battle_loss_accum, rng);
					res.damage_taken *= flank_damage_multiplier(flank_multiplier, std::abs(t - i));
					flanks.push_back({ true, i, t, std::abs(t - i), res });
				}
			}
			if (b_alive_before[i] && !a_alive_before[i] && has_adj(a_alive_before, i)) {
				int t = pick_target(a_units, a_regs, i);
				if (t >= 0) {
					int b_reg = b_units[i].regiment_idx;
					int a_reg = a_units[t].regiment_idx;
					const SideInputs& b_s = get_b_inputs(b_reg);
					const SideInputs& a_s = get_a_inputs(a_reg);
					RoundResult res = simulate_round(b_s, a_s, mult, hit_k, round, a_units[t].battle_loss_accum, rng);
					res.damage_taken *= flank_damage_multiplier(flank_multiplier, std::abs(t - i));
					flanks.push_back({ false, i, t, std::abs(t - i), res });
				}
			}
		}

		int flank_hits = 0;
		for (const auto& f : flanks) {
			if (f.is_a) {
				int b_reg = b_units[f.dst].regiment_idx;
				int a_reg = a_units[f.src].regiment_idx;
				b_units[f.dst].hp -= f.res.damage_taken;
				if (b_units[f.dst].hp < 0.0) b_units[f.dst].hp = 0.0;
				b_units[f.dst].battle_loss_accum += f.res.damage_taken * get_b_inputs(b_reg).battle_loss_factor;
				if (a_reg >= 0 && a_reg < static_cast<int>(a_reports.size()))
					a_reports[a_reg].damage_dealt += f.res.damage_taken;
			} else {
				int a_reg = a_units[f.dst].regiment_idx;
				int b_reg = b_units[f.src].regiment_idx;
				a_units[f.dst].hp -= f.res.damage_taken;
				if (a_units[f.dst].hp < 0.0) a_units[f.dst].hp = 0.0;
				a_units[f.dst].battle_loss_accum += f.res.damage_taken * get_a_inputs(a_reg).battle_loss_factor;
				if (b_reg >= 0 && b_reg < static_cast<int>(b_reports.size()))
					b_reports[b_reg].damage_dealt += f.res.damage_taken;
			}
			if (f.res.hit) ++flank_hits;
		}

		// 阵亡统计
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

		std::string a_after = alive_line(a_units);
		std::string b_after = alive_line(b_units);

		// ===== 回合战报 =====
		std::vector<int> a_new_dead, b_new_dead;
		for (int i = 0; i < battlefield_width; ++i) {
			if (a_alive_before[i] && !is_alive(a_units[i])) a_new_dead.push_back(i + 1);
			if (b_alive_before[i] && !is_alive(b_units[i])) b_new_dead.push_back(i + 1);
		}

		int a_front = count_alive(a_units);
		int b_front = count_alive(b_units);
		int a_bl = backline_queue_total(a_backline);
		int b_bl = backline_queue_total(b_backline);
		last_a_total = a_front + a_bl;
		last_b_total = b_front + b_bl;

		std::cout << "\n-- 第 " << round << " 回合战报 --\n";
		std::cout << "A 存活=" << last_a_total << " / " << a_total_init << "\n";
		std::cout << "B 存活=" << last_b_total << " / " << b_total_init << "\n";
		std::cout << "A 后排待命=" << a_bl << "\n";
		std::cout << "B 后排待命=" << b_bl << "\n";
		std::cout << "补位后 A=" << a_after_fill << "\n";
		std::cout << "补位后 B=" << b_after_fill << "\n";
		std::cout << "结算后 A=" << a_after << "\n";
		std::cout << "结算后 B=" << b_after << "\n";

		auto print_dead = [](const std::string& side, const std::vector<int>& dead) {
			if (dead.empty()) std::cout << side << " 本回合无人阵亡\n";
			else {
				std::cout << side << " 本回合阵亡位置：";
				for (size_t i = 0; i < dead.size(); ++i) {
					if (i > 0) std::cout << "、";
					std::cout << dead[i];
				}
				std::cout << "\n";
			}
		};
		print_dead("A", a_new_dead);
		print_dead("B", b_new_dead);
		std::cout << "夹击次数=" << flanks.size() << " 命中=" << flank_hits << " 倍率=" << flank_multiplier << "\n";

		// 位置详情
		std::cout << "\n如需查看位置详情，输入位置编号（空格分隔，1-" << battlefield_width << "），直接回车跳过：";
		std::string detail_line;
		std::getline(std::cin, detail_line);
		std::vector<int> detail_indices = parse_indices(detail_line, battlefield_width);
		for (int idx : detail_indices) {
			std::cout << "\n== 位置 " << (idx + 1) << " 详细战报 ==\n";
			if (engaged[idx]) {
				int a_reg = a_units[idx].regiment_idx;
				int b_reg = b_units[idx].regiment_idx;
				std::cout << "A[兵种" << (a_reg + 1) << "] -> B[兵种" << (b_reg + 1) << "]\n";
				std::cout << "  B 受伤前血量:      " << b_hp_before[idx] << "\n";
				std::cout << "  基础伤害:          " << a_to_b[idx].base_damage << "\n";
				std::cout << "  命中率:            " << (a_to_b[idx].hit_chance * 100.0) << "%\n";
				std::cout << "  防御乘数:          " << a_to_b[idx].defense_multiplier << "\n";
				std::cout << "  命中判定:          " << (a_to_b[idx].hit ? "命中" : "未命中") << "\n";
				std::cout << "  防御判定:          " << a_to_b[idx].defense_tier << "\n";
				std::cout << "  实际承受伤害:      " << a_to_b[idx].damage_taken << "\n";
				std::cout << "  B 血量:            " << b_units[idx].hp << "\n";

				std::cout << "B[兵种" << (b_reg + 1) << "] -> A[兵种" << (a_reg + 1) << "]\n";
				std::cout << "  A 受伤前血量:      " << a_hp_before[idx] << "\n";
				std::cout << "  基础伤害:          " << b_to_a[idx].base_damage << "\n";
				std::cout << "  命中率:            " << (b_to_a[idx].hit_chance * 100.0) << "%\n";
				std::cout << "  防御乘数:          " << b_to_a[idx].defense_multiplier << "\n";
				std::cout << "  命中判定:          " << (b_to_a[idx].hit ? "命中" : "未命中") << "\n";
				std::cout << "  防御判定:          " << b_to_a[idx].defense_tier << "\n";
				std::cout << "  实际承受伤害:      " << b_to_a[idx].damage_taken << "\n";
				std::cout << "  A 血量:            " << a_units[idx].hp << "\n";
			} else if (is_alive(a_units[idx]) && !is_alive(b_units[idx]))
				std::cout << "B 已被击溃，A 待命\n";
			else if (!is_alive(a_units[idx]) && is_alive(b_units[idx]))
				std::cout << "A 已被击溃，B 待命\n";
			else
				std::cout << "双方已空\n";

			for (const auto& f : flanks) {
				if (f.src != idx && f.dst != idx) continue;
				if (f.is_a)
					std::cout << "夹击 A[" << (f.src + 1) << "] -> B[" << (f.dst + 1) << "]\n";
				else
					std::cout << "夹击 B[" << (f.src + 1) << "] -> A[" << (f.dst + 1) << "]\n";
				double fm = flank_damage_multiplier(flank_multiplier, f.dist);
				std::cout << "  距离=" << f.dist << " 最终倍率=" << fm
					<< " 伤害=" << f.res.damage_taken << " 命中=" << (f.res.hit ? "是" : "否") << "\n";
			}
		}

		if (last_a_total == 0 || last_b_total == 0) {
			std::cout << "\n== 结束：一方队伍被清空 ==\n";
			print_outcome(last_a_total, last_b_total, false);
			ended_by_elimination = true;
			break;
		}
		std::cout << "\n按回车继续下一回合...";
		std::string _;
		std::getline(std::cin, _);
	}

	if (!ended_by_elimination) print_outcome(last_a_total, last_b_total, true);

	// 兵种战报
	print_regiment_reports("Side A", a_regs, a_reports);
	print_regiment_reports("Side B", b_regs, b_reports);

	std::cout << "\n== 推演结束 ==\n";

	// 后续操作
	std::cout << "\n== 后续操作 ==\n";
	std::cout << "1) 直接再次推演\n";
	std::cout << "2) 百次测试\n";
	std::cout << "3) 修改全局参数\n";
	std::cout << "0) 退出\n";

	int next = read_int("请选择", 1);
	switch (next) {
	case 1: return true;
	case 2:
		run_hundred_test(battlefield_width, a_regs, b_regs,
			mult, hit_k, max_rounds, flank_multiplier, flank_target_mode, vertical_fill_delay, rng);
		return true;
	case 3:
		edit_global_menu(hit_k, mult, max_rounds, battlefield_width,
			flank_multiplier, flank_target_mode, vertical_fill_delay);
		return true;
	case 0: return false;
	default: std::cout << "无效选项。\n"; return true;
	}
}
