#pragma once

#include <random>
#include <vector>

#include "types.hpp"

// ==== 公式与数值 ====

double calc_base_damage(double attack, double training, double full_rate, double battle_multiplier);
double calc_hit_chance(double attacker_evade, double defender_evade, double k);
double calc_defense_multiplier(
	double defense_rate,
	double armor_reduction,
	std::mt19937& rng,
	std::string& tier_out
);
double calc_morale(const SideInputs& s, const Multipliers& m, int round, double battle_loss);
double calc_hp(double morale, const SideInputs& s);
double unit_morale(const SideInputs& s, const Multipliers& m, int round, const UnitState& u);

bool is_alive(const UnitState& u);
int count_alive(const std::vector<UnitState>& units);
std::vector<UnitState> init_units(
	const SideInputs& s,
	const Multipliers& m,
	int width,
	const std::vector<int>& positions
);
UnitState initial_unit_state(const SideInputs& s, const Multipliers& m);

double flank_damage_multiplier(double base_multiplier, int distance);

RoundResult simulate_round(
	const SideInputs& attacker,
	const SideInputs& defender,
	const Multipliers& battle_mult,
	double hit_k,
	int round,
	double defender_battle_loss_accum,
	std::mt19937& rng
);
