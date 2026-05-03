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

// ==== 菜单 ====

const char* flank_target_mode_label(FlankTargetMode mode) {
	return mode == FlankTargetMode::Morale ? "士气" : "血量";
}

void edit_global_menu(
	double& hit_k,
	Multipliers& mult,
	int& max_rounds,
	int& battlefield_width,
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
			<< " 夹击倍率=" << flank_multiplier
			<< " 夹击目标=" << flank_target_mode_label(flank_target_mode)
			<< " 纵向补位延迟=" << vertical_fill_delay
			<< "\n";
		std::cout << "1) 命中公式 k\n";
		std::cout << "2) 战斗类型乘数\n";
		std::cout << "3) 特殊情况乘数\n";
		std::cout << "4) 最大回合数\n";
		std::cout << "5) 战场宽度 n\n";
		std::cout << "6) 夹击倍率\n";
		std::cout << "7) 夹击目标（1 血量 / 2 士气）\n";
		std::cout << "8) 纵向补位延迟（空位持续回合数）\n";
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
			flank_multiplier = read_double("夹击倍率", flank_multiplier);
			break;
		case 7: {
			int value = read_int("夹击目标（1 血量 / 2 士气）", flank_target_mode == FlankTargetMode::Morale ? 2 : 1);
			flank_target_mode = (value == 2) ? FlankTargetMode::Morale : FlankTargetMode::Hp;
			break;
		}
		case 8:
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

// ==== CSV 读取（旋转格式：中文表头 + 按分类读取多行）====

static std::vector<double> read_csv_data_line(std::ifstream& file) {
	std::string line;
	if (!std::getline(file, line)) return {};
	std::istringstream iss(line);
	std::string cell;
	// 跳过第0列（分类名/表头名）
	std::getline(iss, cell, ',');
	std::vector<double> values;
	while (std::getline(iss, cell, ',')) {
		try {
			values.push_back(std::stod(cell));
		} catch (...) {
			values.push_back(0.0);
		}
	}
	return values;
}

CsvParams read_csv_params(const std::string& filepath) {
	CsvParams params;

	std::ifstream file(filepath);
	if (!file.is_open()) {
		std::cerr << "警告：无法打开 " << filepath << "，将使用默认参数。\n";
		return params;
	}

	// 第1行：global表头（跳过）
	std::string header;
	std::getline(file, header);

	// 第2行：global 数据
	std::vector<double> global = read_csv_data_line(file);
	if (global.size() >= 8) {
		params.hit_k = global[0];
		params.mult.battle_type = global[1];
		params.mult.special = global[2];
		params.max_rounds = static_cast<int>(global[3]);
		params.battlefield_width = static_cast<int>(global[4]);
		params.flank_multiplier = global[5];
		params.flank_target_mode = (static_cast<int>(global[6]) == 2) ? FlankTargetMode::Morale : FlankTargetMode::Hp;
		params.vertical_fill_delay = static_cast<int>(global[7]);
	}

	// 第3行：side表头（跳过）
	std::getline(file, header);

	// 第4行起：sideA / sideB 数据行
	// 每行格式: sideA,攻击,闪避,训练度,满编率,护甲减伤率,防御率,组织度,基础士气,时间损耗,战中损耗系数,队伍数
	while (true) {
		std::string line;
		if (!std::getline(file, line)) break;
		if (line.empty()) continue;

		std::istringstream iss(line);
		std::string cat;
		std::getline(iss, cat, ',');
		std::vector<double> vals;
		std::string cell;
		while (std::getline(iss, cell, ',')) {
			try {
				vals.push_back(std::stod(cell));
			} catch (...) {
				vals.push_back(0.0);
			}
		}
		if (vals.size() < 11) continue; // 需要至少 11 个数值（10属性 + 队伍数）

		SideInputs s;
		s.attack = vals[0];
		s.evade = vals[1];
		s.training = clamp01(vals[2]);
		s.full_rate = clamp01(vals[3]);
		s.armor_reduction = clamp01(vals[4]);
		s.defense_rate = clamp01(vals[5]);
		s.organization = clamp_range(vals[6], 0.0, 100.0);
		s.base_morale = vals[7];
		s.time_loss_per_round = vals[8];
		s.battle_loss_factor = vals[9];

		Regiment reg;
		reg.stats = s;
		reg.count = std::max(0, static_cast<int>(vals[10]));

		if (cat == "sideA") {
			reg.stats.name = "A";
			params.side_a_regiments.push_back(reg);
		} else if (cat == "sideB") {
			reg.stats.name = "B";
			params.side_b_regiments.push_back(reg);
		}
	}

	return params;
}
