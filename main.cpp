#include <algorithm>
#include <iostream>
#include <random>

#include "io.hpp"
#include "report.hpp"
#include "sim.hpp"
#include "simulation.hpp"

int main() {
std::random_device rd;
std::mt19937 rng(rd());

std::cout << "\n正在从 data.csv 读取参数...\n";
CsvParams csv = read_csv_params("data.csv");

// 按预期战斗力排序
sort_regiments_by_power(csv.side_a_regiments, csv.mult);
sort_regiments_by_power(csv.side_b_regiments, csv.mult);

double hit_k = csv.hit_k;
Multipliers mult = csv.mult;
int max_rounds = std::max(1, csv.max_rounds);
int battlefield_width = std::max(1, csv.battlefield_width);
double flank_multiplier = csv.flank_multiplier;
FlankTargetMode flank_target_mode = csv.flank_target_mode;
int vertical_fill_delay = csv.vertical_fill_delay;

// 用于运行时修改的参数副本
std::vector<Regiment> a_regs = csv.side_a_regiments;
std::vector<Regiment> b_regs = csv.side_b_regiments;

auto print_summary = [&]() {
int a_total = total_regiment_count(a_regs);
int b_total = total_regiment_count(b_regs);
std::cout << "已加载 " << a_total + b_total << " 个单位（A=" << a_total << " B=" << b_total << "），战场宽度=" << battlefield_width << "\n";
std::cout << "  A 兵种数=" << a_regs.size() << " B 兵种数=" << b_regs.size() << "\n";
};
print_summary();

bool keep_running = true;
while (keep_running) {
std::cout << "\n== 选择操作 ==\n";
std::cout << "1) 单次推演（逐回合查看战报）\n";
std::cout << "2) 百次测试（快速测试 100 场，输出胜率与平均残余）\n";
std::cout << "3) 修改全局参数\n";
std::cout << "4) 重新读取 CSV（丢弃内存中的修改）\n";
std::cout << "0) 退出\n";

int choice = read_int("请选择", 1);
if (choice == 0) { keep_running = false; break; }

if (choice == 2) {
run_hundred_test(battlefield_width, a_regs, b_regs,
mult, hit_k, max_rounds, flank_multiplier, flank_target_mode, vertical_fill_delay, rng);
continue;
}
if (choice == 3) {
edit_global_menu(hit_k, mult, max_rounds, battlefield_width,
flank_multiplier, flank_target_mode, vertical_fill_delay);
continue;
}
if (choice == 4) {
std::cout << "重新读取 data.csv...\n";
CsvParams csv = read_csv_params("data.csv");
sort_regiments_by_power(csv.side_a_regiments, csv.mult);
sort_regiments_by_power(csv.side_b_regiments, csv.mult);
hit_k = csv.hit_k;
mult = csv.mult;
max_rounds = std::max(1, csv.max_rounds);
battlefield_width = std::max(1, csv.battlefield_width);
flank_multiplier = csv.flank_multiplier;
flank_target_mode = csv.flank_target_mode;
vertical_fill_delay = csv.vertical_fill_delay;
a_regs = csv.side_a_regiments;
b_regs = csv.side_b_regiments;
print_summary();
continue;
}

// 选项 1：单次推演
if (!run_single_simulation(
battlefield_width, a_regs, b_regs,
mult, hit_k, max_rounds,
flank_multiplier, flank_target_mode,
vertical_fill_delay, rng
)) {
keep_running = false;
}
}

return 0;
}
