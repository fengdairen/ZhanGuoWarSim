#include "battle.hpp"

#include <algorithm>

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
