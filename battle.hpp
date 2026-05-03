#pragma once

#include <string>
#include <vector>

#include "types.hpp"

std::vector<int> centered_positions(int width, int count);
std::vector<int> center_out_order(int width);
void apply_lateral_fill(
	std::vector<UnitState>& self_units,
	const std::vector<UnitState>& enemy_units
);
std::string alive_line(const std::vector<UnitState>& units);

// 多兵种布阵：先按战斗力排序（外部已排好），展开为展平列表，再用 center_out_order 分配位置
// 返回 (前排units, 后排队列 vector<vector<int>> 每个位置存兵种索引)
struct DeployResult {
	std::vector<UnitState> front_units;
	std::vector<std::vector<int>> backline_queue; // 每个位置的待补队列（存 regiment_idx）
};
DeployResult deploy_regiments(
	const std::vector<Regiment>& regiments,
	const Multipliers& mult,
	int battlefield_width
);
