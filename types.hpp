#pragma once

#include <string>

struct SideInputs {
	std::string name;
	double attack = 100.0;
	double evade = 50.0;
	double training = 0.5;      // 0-1 训练度
	double full_rate = 1.0;     // 0-1 满编率
	double armor_reduction = 0.3; // 0-1 护甲减伤率
	double defense_rate = 0.6;  // 0-1 防御率
	double organization = 80.0;  // 0-100 组织度
	double base_morale = 100.0;
	double time_loss_per_round = 1.0;
	double battle_loss_factor = 0.1; // 战中损耗系数（每点伤害）
};

struct Multipliers {
	double battle_type = 1.0;
	double special = 1.0;
};

struct RoundResult {
	double base_damage = 0.0;
	double hit_chance = 0.0;
	double defense_multiplier = 1.0;
	double damage_taken = 0.0;
	bool hit = false;
	std::string defense_tier;
	double morale = 0.0;
	double hp = 0.0;
};

struct UnitState {
	double hp = 0.0;
	double battle_loss_accum = 0.0;
};

// 夹击目标偏好
enum class FlankTargetMode {
	Hp = 1,
	Morale = 2
};
