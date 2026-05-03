#include "sim.hpp"

#include <algorithm>
#include <cmath>

// ==== 公式与数值 ====

double calc_base_damage(double attack, double training, double full_rate, double battle_multiplier) {
	// 伤害 = 攻击 * (1+训练度^0.5) * (满编率^0.8) * 战斗类型乘数
	return attack * (1.0 + std::sqrt(training)) * std::pow(full_rate, 0.8) * battle_multiplier;
}

double calc_hit_chance(double attacker_evade, double defender_evade, double k) {
	// 命中率 = min(进攻方闪避 / (受击方闪避 + k), 1)
	if (defender_evade + k <= 0.0) {
		return 1.0;
	}
	double chance = attacker_evade / (defender_evade + k);
	if (chance > 1.0) chance = 1.0;
	if (chance < 0.0) chance = 0.0;
	return chance;
}

double calc_defense_multiplier(
	double defense_rate,
	double armor_reduction,
	std::mt19937& rng,
	std::string& tier_out
) {
	// 防御判定：先按防御率决定是否触发，再按权重选档位
	std::uniform_real_distribution<double> dist01(0.0, 1.0);
	if (dist01(rng) > defense_rate) {
		tier_out = "未防御";
		return 1.0;
	}

	std::uniform_real_distribution<double> dist100(0.0, 100.0);
	double roll = dist100(rng);
	if (roll < 10.0) {
		tier_out = "完美防御";
		double reduction = std::min(armor_reduction * 2.0, 0.8);
		return 1.0 - reduction;
	}
	if (roll < 70.0) {
		tier_out = "一般防御";
		double reduction = std::min(armor_reduction, 0.8);
		return 1.0 - reduction;
	}
	tier_out = "勉强防御";
	double reduction = std::min(armor_reduction * 0.5, 0.8);
	return 1.0 - reduction;
}

double calc_morale(const SideInputs& s, const Multipliers& m, int round, double battle_loss) {
	// 士气 = 基础士气 * 乘数 - 时间损耗 - 战中损耗
	double morale = s.base_morale * m.battle_type * m.special;
	morale -= s.time_loss_per_round * static_cast<double>(round);
	morale -= battle_loss;
	if (morale < 0.0) morale = 0.0;
	return morale;
}

double calc_hp(double morale, const SideInputs& s) {
	// 血量 = 士气 * (1 + 组织度/100) * (1+训练度^0.2) * (满编率^1.2)
	double hp = morale * (1.0 + s.organization / 100.0) * (1.0 + std::pow(s.training, 0.2)) * std::pow(s.full_rate, 1.2);
	if (hp < 0.0) hp = 0.0;
	return hp;
}

double unit_morale(const SideInputs& s, const Multipliers& m, int round, const UnitState& u) {
	return calc_morale(s, m, round, u.battle_loss_accum);
}

// 预期战斗力 = 基础伤害 × 初始血量
double calc_expected_power(const SideInputs& s, const Multipliers& m) {
	double base_dmg = calc_base_damage(s.attack, s.training, s.full_rate, m.battle_type);
	double morale = calc_morale(s, m, 0, 0.0);
	double hp = calc_hp(morale, s);
	return base_dmg * hp;
}

void sort_regiments_by_power(std::vector<Regiment>& regiments, const Multipliers& m) {
	std::sort(regiments.begin(), regiments.end(),
		[&](const Regiment& a, const Regiment& b) {
			return calc_expected_power(a.stats, m) > calc_expected_power(b.stats, m);
		});
}

bool is_alive(const UnitState& u) {
	return u.hp > 0.0;
}

int count_alive(const std::vector<UnitState>& units) {
	int count = 0;
	for (const auto& u : units) {
		if (is_alive(u)) {
			++count;
		}
	}
	return count;
}

int total_regiment_count(const std::vector<Regiment>& regiments) {
	int total = 0;
	for (const auto& r : regiments) {
		total += r.count;
	}
	return total;
}

UnitState initial_unit_state(const SideInputs& s, const Multipliers& m, int regiment_idx) {
	UnitState state;
	double morale = calc_morale(s, m, 0, 0.0);
	state.hp = calc_hp(morale, s);
	state.battle_loss_accum = 0.0;
	state.regiment_idx = regiment_idx;
	return state;
}

double flank_damage_multiplier(double base_multiplier, int distance) {
	// 距离每增加 1，就多乘一次倍率
	return std::pow(base_multiplier, static_cast<double>(distance));
}

RoundResult simulate_round(
	const SideInputs& attacker,
	const SideInputs& defender,
	const Multipliers& battle_mult,
	double hit_k,
	int round,
	double defender_battle_loss_accum,
	std::mt19937& rng
) {
	// 单回合（期望值）模拟
	RoundResult r;
	r.base_damage = calc_base_damage(attacker.attack, attacker.training, attacker.full_rate, battle_mult.battle_type);
	r.hit_chance = calc_hit_chance(attacker.evade, defender.evade, hit_k);
	std::uniform_real_distribution<double> dist01(0.0, 1.0);
	r.hit = dist01(rng) < r.hit_chance;
	if (r.hit) {
		r.defense_multiplier = calc_defense_multiplier(
			defender.defense_rate,
			defender.armor_reduction,
			rng,
			r.defense_tier
		);
		r.damage_taken = r.base_damage * r.defense_multiplier;
	} else {
		r.defense_tier = "未命中";
		r.defense_multiplier = 0.0;
		r.damage_taken = 0.0;
	}
	r.morale = calc_morale(defender, battle_mult, round, defender_battle_loss_accum);
	r.hp = calc_hp(r.morale, defender);
	return r;
}
