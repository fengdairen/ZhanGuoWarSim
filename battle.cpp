#include "battle.hpp"

#include <algorithm>
#include <queue>

#include "sim.hpp"

std::vector<int> centered_positions(int width, int count) {
	std::vector<int> positions;
	if (width <= 0 || count <= 0) {
		return positions;
	}
	if (count > width) {
		count = width;
	}
	int start = (width - count) / 2;
	positions.reserve(count);
	for (int i = 0; i < count; ++i) {
		positions.push_back(start + i);
	}
	return positions;
}

std::vector<int> center_out_order(int width) {
	std::vector<int> order;
	if (width <= 0) {
		return order;
	}
	int left = (width - 1) / 2;
	int right = left + 1;
	order.reserve(width);
	order.push_back(left);
	while (static_cast<int>(order.size()) < width) {
		if (right < width) {
			order.push_back(right);
		}
		if (static_cast<int>(order.size()) >= width) {
			break;
		}
		--left;
		if (left >= 0) {
			order.push_back(left);
		}
		++right;
	}
	return order;
}

static bool side_has_enemy(
	const std::vector<UnitState>& enemy_units,
	int index,
	int dir
) {
	int width = static_cast<int>(enemy_units.size());
	for (int i = index + dir; i >= 0 && i < width; i += dir) {
		if (is_alive(enemy_units[i])) {
			return true;
		}
	}
	return false;
}

void apply_lateral_fill(
	std::vector<UnitState>& self_units,
	const std::vector<UnitState>& enemy_units
) {
	int width = static_cast<int>(self_units.size());
	double center = (static_cast<double>(width) - 1.0) / 2.0;
	for (int i = 0; i < width; ++i) {
		if (!is_alive(self_units[i])) {
			continue;
		}
		if (is_alive(enemy_units[i])) {
			continue;
		}
		int center_dir = (static_cast<double>(i) <= center) ? 1 : -1;
		int away_dir = -center_dir;
		int move_dir = 0;
		if (side_has_enemy(enemy_units, i, center_dir)) {
			move_dir = center_dir;
		} else if (side_has_enemy(enemy_units, i, away_dir)) {
			move_dir = away_dir;
		} else {
			continue;
		}
		int dest = i + move_dir;
		if (dest < 0 || dest >= width) {
			continue;
		}
		if (is_alive(self_units[dest])) {
			continue;
		}
		self_units[dest] = self_units[i];
		self_units[i] = UnitState();
		if (move_dir > 0) {
			++i;
		}
	}
}

std::string alive_line(const std::vector<UnitState>& units) {
	std::string line;
	line.reserve(units.size());
	for (const auto& u : units) {
		line.push_back(is_alive(u) ? '1' : '0');
	}
	return line;
}

DeployResult deploy_regiments(
	const std::vector<Regiment>& regiments,
	const Multipliers& mult,
	int battlefield_width
) {
	DeployResult result;

	// 1. 展开：生成展平列表（按战斗力降序，regiments 外部已排序）
	//    flattened[i] = regiment_idx 表示这个单位的兵种归属
	std::vector<int> flattened;
	for (int r = 0; r < static_cast<int>(regiments.size()); ++r) {
		for (int i = 0; i < regiments[r].count; ++i) {
			flattened.push_back(r);
		}
	}

	int total_units = static_cast<int>(flattened.size());

	// 2. 分配前排位置
	result.front_units.resize(battlefield_width);
	int front_count = std::min(battlefield_width, total_units);
	std::vector<int> front_positions = centered_positions(battlefield_width, front_count);

	// 按战斗力从高到低占中轴线附近的坑
	for (int i = 0; i < front_count; ++i) {
		int pos = front_positions[i];
		int reg_idx = flattened[i];
		result.front_units[pos] = initial_unit_state(regiments[reg_idx].stats, mult, reg_idx);
	}

	// 3. 分配后排队列
	result.backline_queue.resize(battlefield_width);
	if (total_units > battlefield_width) {
		// 剩余单位按 center_out_order 分配到各位置的后排队列
		std::vector<int> order = center_out_order(battlefield_width);
		int idx = 0;
		for (int i = battlefield_width; i < total_units; ++i) {
			int pos = order[idx % battlefield_width];
			result.backline_queue[pos].push_back(flattened[i]);
			++idx;
		}
	}

	return result;
}
