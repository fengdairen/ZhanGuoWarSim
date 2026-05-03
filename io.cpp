#include "io.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

// ==== 通用工具 ====

static double clamp01(double v) {
	// 将输入限制到 0-1 范围
	if (v < 0.0) return 0.0;
	if (v > 1.0) return 1.0;
	return v;
}

static double clamp_range(double v, double min_v, double max_v) {
	// 将输入限制到指定范围
	if (v < min_v) return min_v;
	if (v > max_v) return max_v;
	return v;
}

static int clamp_non_negative(int v) {
	return std::max(0, v);
}

// ==== 输入 ====

double read_double(const std::string& prompt, double default_value) {
	// 读取浮点输入，空行使用默认值
	while (true) {
		std::cout << prompt << " [默认值 " << default_value << "]: ";
		std::string line;
		if (!std::getline(std::cin, line)) {
			return default_value;
		}
		if (line.empty()) {
			return default_value;
		}
		try {
			size_t idx = 0;
			double value = std::stod(line, &idx);
			if (idx == line.size()) {
				return value;
			}
		} catch (...) {
		}
		std::cout << "数字无效，请重试。\n";
	}
}

int read_int(const std::string& prompt, int default_value) {
	// 读取整数输入，空行使用默认值
	while (true) {
		std::cout << prompt << " [默认值 " << default_value << "]: ";
		std::string line;
		if (!std::getline(std::cin, line)) {
			return default_value;
		}
		if (line.empty()) {
			return default_value;
		}
		try {
			size_t idx = 0;
			int value = std::stoi(line, &idx);
			if (idx == line.size()) {
				return value;
			}
		} catch (...) {
		}
		std::cout << "整数无效，请重试。\n";
	}
}

SideInputs read_side_inputs(const std::string& name, const SideInputs& defaults) {
	// 读取某一方的基础参数
	SideInputs s = defaults;
	s.name = name;

	std::cout << "\n== " << name << " 参数（比率用 0-1，组织度用 0-100） ==\n";
	std::cout << "快速输入：可用空格分隔的数字一行输入\n";
	std::cout << "顺序：攻击 闪避 训练度 满编率 护甲减伤率 防御率 组织度(0-100) 基础士气 时间损耗 战中损耗系数\n";
	std::cout << "直接回车则逐项输入。\n";
	std::cout << "快速输入：";
	std::string fast_line;
	std::getline(std::cin, fast_line);

	std::istringstream iss(fast_line);
	std::vector<double> values;
	double v = 0.0;
	while (iss >> v) {
		values.push_back(v);
	}

	if (!values.empty()) {
		// 按顺序填充，缺项保留默认值
		if (values.size() > 0) s.attack = values[0];
		if (values.size() > 1) s.evade = values[1];
		if (values.size() > 2) s.training = values[2];
		if (values.size() > 3) s.full_rate = values[3];
		if (values.size() > 4) s.armor_reduction = values[4];
		if (values.size() > 5) s.defense_rate = values[5];
		if (values.size() > 6) s.organization = values[6];
		if (values.size() > 7) s.base_morale = values[7];
		if (values.size() > 8) s.time_loss_per_round = values[8];
		if (values.size() > 9) s.battle_loss_factor = values[9];
	} else {
		s.attack = read_double("攻击", s.attack);
		s.evade = read_double("闪避", s.evade);
		s.training = read_double("训练度", s.training);
		s.full_rate = read_double("满编率", s.full_rate);
		s.armor_reduction = read_double("护甲减伤率", s.armor_reduction);
		s.defense_rate = read_double("防御率", s.defense_rate);
		s.organization = read_double("组织度(0-100)", s.organization);
		s.base_morale = read_double("基础士气", s.base_morale);
		s.time_loss_per_round = read_double("每回合时间损耗", s.time_loss_per_round);
		s.battle_loss_factor = read_double("战中损耗系数（每点伤害）", s.battle_loss_factor);
	}

	s.training = clamp01(s.training);
	s.full_rate = clamp01(s.full_rate);
	s.armor_reduction = clamp01(s.armor_reduction);
	s.defense_rate = clamp01(s.defense_rate);
	s.organization = clamp_range(s.organization, 0.0, 100.0);

	return s;
}

// ==== 菜单 ====

void print_side_summary(const SideInputs& s) {
	std::cout << "  攻击=" << s.attack
		<< " 闪避=" << s.evade
		<< " 训练度=" << s.training
		<< " 满编率=" << s.full_rate
		<< " 护甲减伤率=" << s.armor_reduction
		<< " 防御率=" << s.defense_rate
		<< " 组织度=" << s.organization
		<< " 基础士气=" << s.base_morale
		<< " 时间损耗=" << s.time_loss_per_round
		<< " 战中损耗系数=" << s.battle_loss_factor
		<< "\n";
}

void edit_side_menu(SideInputs& s) {
	while (true) {
		std::cout << "\n== 修改 " << s.name << " 参数 ==\n";
		print_side_summary(s);
		std::cout << "1) 攻击\n";
		std::cout << "2) 闪避\n";
		std::cout << "3) 训练度\n";
		std::cout << "4) 满编率\n";
		std::cout << "5) 护甲减伤率\n";
		std::cout << "6) 防御率\n";
		std::cout << "7) 组织度\n";
		std::cout << "8) 基础士气\n";
		std::cout << "9) 每回合时间损耗\n";
		std::cout << "10) 战中损耗系数\n";
		std::cout << "0) 返回\n";

		int choice = read_int("选择要修改的项目", 0);
		switch (choice) {
		case 1:
			s.attack = read_double("攻击", s.attack);
			break;
		case 2:
			s.evade = read_double("闪避", s.evade);
			break;
		case 3:
			s.training = clamp01(read_double("训练度", s.training));
			break;
		case 4:
			s.full_rate = clamp01(read_double("满编率", s.full_rate));
			break;
		case 5:
			s.armor_reduction = clamp01(read_double("护甲减伤率", s.armor_reduction));
			break;
		case 6:
			s.defense_rate = clamp01(read_double("防御率", s.defense_rate));
			break;
		case 7:
			s.organization = clamp_range(read_double("组织度(0-100)", s.organization), 0.0, 100.0);
			break;
		case 8:
			s.base_morale = read_double("基础士气", s.base_morale);
			break;
		case 9:
			s.time_loss_per_round = read_double("每回合时间损耗", s.time_loss_per_round);
			break;
		case 10:
			s.battle_loss_factor = read_double("战中损耗系数", s.battle_loss_factor);
			break;
		case 0:
			return;
		default:
			std::cout << "无效选项，请重试。\n";
			break;
		}
	}
}

const char* flank_target_mode_label(FlankTargetMode mode) {
	return mode == FlankTargetMode::Morale ? "士气" : "血量";
}

void edit_global_menu(
	double& hit_k,
	Multipliers& mult,
	int& max_rounds,
	int& battlefield_width,
	int& a_count,
	int& b_count,
	double& flank_multiplier,
	FlankTargetMode& flank_target_mode,
	int& vertical_fill_delay
) {
	while (true) {
		std::cout << "\n== 修改全局参数 ==\n";
		std::cout << "  命中公式 k=" << hit_k
			<< " 战斗类型乘数=" << mult.battle_type
			<< " 特殊情况乘数=" << mult.special
			<< " 最大回合数=" << max_rounds
			<< " 战场宽度 n=" << battlefield_width
			<< " A 队伍数量=" << a_count
			<< " B 队伍数量=" << b_count
			<< " 夹击倍率=" << flank_multiplier
			<< " 夹击目标=" << flank_target_mode_label(flank_target_mode)
			<< " 纵向补位延迟=" << vertical_fill_delay
			<< "\n";
		std::cout << "1) 命中公式 k\n";
		std::cout << "2) 战斗类型乘数\n";
		std::cout << "3) 特殊情况乘数\n";
		std::cout << "4) 最大回合数\n";
		std::cout << "5) 战场宽度 n\n";
		std::cout << "6) A 队伍数量\n";
		std::cout << "7) B 队伍数量\n";
		std::cout << "8) 夹击倍率\n";
		std::cout << "9) 夹击目标（1 血量 / 2 士气）\n";
		std::cout << "10) 纵向补位延迟（空位持续回合数）\n";
		std::cout << "0) 返回\n";

		int choice = read_int("选择要修改的项目", 0);
		switch (choice) {
		case 1:
			hit_k = read_double("命中公式 k", hit_k);
			break;
		case 2:
			mult.battle_type = read_double("战斗类型乘数", mult.battle_type);
			break;
		case 3:
			mult.special = read_double("特殊情况乘数", mult.special);
			break;
		case 4:
			max_rounds = read_int("最大回合数", max_rounds);
			break;
		case 5:
			battlefield_width = std::max(1, read_int("战场宽度 n", battlefield_width));
			break;
		case 6:
			a_count = clamp_non_negative(read_int("A 队伍数量", a_count));
			break;
		case 7: {
			b_count = clamp_non_negative(read_int("B 队伍数量", b_count));
			break;
		}
		case 8:
			flank_multiplier = read_double("夹击倍率", flank_multiplier);
			break;
		case 9: {
			int value = read_int("夹击目标（1 血量 / 2 士气）", flank_target_mode == FlankTargetMode::Morale ? 2 : 1);
			flank_target_mode = (value == 2) ? FlankTargetMode::Morale : FlankTargetMode::Hp;
			break;
		}
		case 10:
			vertical_fill_delay = std::max(1, read_int("纵向补位延迟（空位持续回合数）", vertical_fill_delay));
			break;
		case 0:
			return;
		default:
			std::cout << "无效选项，请重试。\n";
			break;
		}
	}
}

// ==== 解析辅助 ====

std::vector<int> parse_indices(const std::string& line, int max_index) {
	std::vector<int> result;
	std::istringstream iss(line);
	int value = 0;
	while (iss >> value) {
		if (value >= 1 && value <= max_index) {
			result.push_back(value - 1);
		}
	}
	return result;
}

// ==== CSV 读取 ====

CsvParams read_csv_params(const std::string& filepath) {
	CsvParams params;
	params.side_a.name = "A";
	params.side_b.name = "B";

	std::ifstream file(filepath);
	if (!file.is_open()) {
		std::cerr << "警告：无法打开 " << filepath << "，将使用默认参数。\n";
		return params;
	}

	std::string line;
	// 跳过表头
	std::getline(file, line);

	auto trim = [](std::string& s) {
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.erase(s.begin());
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
	};

	while (std::getline(file, line)) {
		if (line.empty()) continue;
		std::istringstream iss(line);
		std::string category, name, val_str;
		std::getline(iss, category, ',');
		std::getline(iss, name, ',');
		std::getline(iss, val_str, ',');
		trim(category);
		trim(name);
		trim(val_str);
		if (name.empty() || val_str.empty()) continue;

		double value = 0.0;
		try {
			value = std::stod(val_str);
		} catch (...) {
			std::cerr << "警告：CSV 行 \"" << line << "\" 数值格式无效，已跳过。\n";
			continue;
		}

		if (category == "global") {
			if (name == "hit_k") params.hit_k = value;
			else if (name == "battle_mult") params.mult.battle_type = value;
			else if (name == "special_mult") params.mult.special = value;
			else if (name == "max_rounds") params.max_rounds = static_cast<int>(value);
			else if (name == "battlefield_width") params.battlefield_width = static_cast<int>(value);
			else if (name == "sideA_count") params.a_count = static_cast<int>(value);
			else if (name == "sideB_count") params.b_count = static_cast<int>(value);
			else if (name == "flank_mult") params.flank_multiplier = value;
			else if (name == "flank_target") {
				params.flank_target_mode = (static_cast<int>(value) == 2) ? FlankTargetMode::Morale : FlankTargetMode::Hp;
			}
			else if (name == "fill_delay") params.vertical_fill_delay = static_cast<int>(value);
		} else if (category == "sideA") {
			if (name == "attack") params.side_a.attack = value;
			else if (name == "evade") params.side_a.evade = value;
			else if (name == "training") params.side_a.training = value;
			else if (name == "full_rate") params.side_a.full_rate = value;
			else if (name == "armor_reduction") params.side_a.armor_reduction = value;
			else if (name == "defense_rate") params.side_a.defense_rate = value;
			else if (name == "organization") params.side_a.organization = value;
			else if (name == "base_morale") params.side_a.base_morale = value;
			else if (name == "time_loss") params.side_a.time_loss_per_round = value;
			else if (name == "battle_loss_factor") params.side_a.battle_loss_factor = value;
		} else if (category == "sideB") {
			if (name == "attack") params.side_b.attack = value;
			else if (name == "evade") params.side_b.evade = value;
			else if (name == "training") params.side_b.training = value;
			else if (name == "full_rate") params.side_b.full_rate = value;
			else if (name == "armor_reduction") params.side_b.armor_reduction = value;
			else if (name == "defense_rate") params.side_b.defense_rate = value;
			else if (name == "organization") params.side_b.organization = value;
			else if (name == "base_morale") params.side_b.base_morale = value;
			else if (name == "time_loss") params.side_b.time_loss_per_round = value;
			else if (name == "battle_loss_factor") params.side_b.battle_loss_factor = value;
		}
	}

	// 值域裁剪（与 read_side_inputs 保持一致）
	for (SideInputs* s : { &params.side_a, &params.side_b }) {
		s->training = clamp01(s->training);
		s->full_rate = clamp01(s->full_rate);
		s->armor_reduction = clamp01(s->armor_reduction);
		s->defense_rate = clamp01(s->defense_rate);
		s->organization = clamp_range(s->organization, 0.0, 100.0);
	}

	return params;
}
