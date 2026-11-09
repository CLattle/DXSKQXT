// 文件名   : sort_report_test.cpp
// 模块作者 : 乙（自测）
// 模块功能 : 排序与报表模块的自测入口，与甲的 record_test.cpp 平行：
//            1) 读取 data.txt（甲的数据层文件格式）；
//            2) 验证三种排序的结果确实有序（按学号 / 姓名 / 日期，且日期相同按学号）；
//            3) 打印五张统计报表，供人工核对数字。
// 使用说明 : 编译 record.cpp + sort_report.cpp + sort_report_test.cpp，
//            直接双击 build_report.bat 即可完成编译、自测并查看输出。

#include "sort_report.h"

#include <iostream>
#include <string>
#include <tuple>

namespace {

int failures = 0;   // 断言失败计数

// 输出一条断言结果
void check(bool condition, const std::string &name)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    if (!condition) {
        ++failures;
    }
}

// 是否按学号有序（学号相同按日期，日期相同按姓名）
bool sortedById(const Record *data, int count)
{
    for (int i = 1; i < count; ++i) {
        const Record &a = data[i - 1];
        const Record &b = data[i];
        if (!(std::tie(a.id, a.month, a.day, a.name) <= std::tie(b.id, b.month, b.day, b.name))) {
            return false;
        }
    }
    return true;
}

// 是否按姓名有序（姓名相同按学号）
bool sortedByName(const Record *data, int count)
{
    for (int i = 1; i < count; ++i) {
        const Record &a = data[i - 1];
        const Record &b = data[i];
        if (!(std::tie(a.name, a.id) <= std::tie(b.name, b.id))) {
            return false;
        }
    }
    return true;
}

// 是否按日期有序（日期相同按学号）
bool sortedByDate(const Record *data, int count)
{
    for (int i = 1; i < count; ++i) {
        const Record &a = data[i - 1];
        const Record &b = data[i];
        if (!(std::tie(a.month, a.day, a.id) <= std::tie(b.month, b.day, b.id))) {
            return false;
        }
    }
    return true;
}

// 打印排序后的前若干条，便于肉眼确认
void printBrief(const Record *data, int count, const std::string &title)
{
    std::cout << title;
    for (int i = 0; i < count && i < 5; ++i) {
        std::cout << data[i].id << '(' << data[i].month << '-' << data[i].day << ") ";
    }
    std::cout << "...\n";
}

}  // namespace

int main()
{
    std::cout << "===== 排序与统计报表模块自测 =====\n";

    const int loaded = loadData("data.txt");
    check(loaded == OK, "读取 data.txt 成功");
    if (loaded != OK) {
        std::cout << "\n数据文件读取失败，返回码 = " << loaded
                  << "（请确认 data.txt 与本程序在同一目录）\n";
        return 1;
    }
    std::cout << "记录条数：" << recordCount << '\n';
    check(recordCount > 0, "记录条数大于 0");

    // ---------- 一、三种排序 ----------
    sortRecords(records, recordCount, SORT_BY_ID);
    check(sortedById(records, recordCount), "按学号排序：整体有序（学号相同按日期）");
    printBrief(records, recordCount, "  学号排序前 5 条：");

    sortRecords(records, recordCount, SORT_BY_NAME);
    check(sortedByName(records, recordCount), "按姓名排序：整体有序（同姓名按学号）");
    printBrief(records, recordCount, "  姓名排序前 5 条：");

    sortRecords(records, recordCount, SORT_BY_DATE);
    check(sortedByDate(records, recordCount), "按日期排序：整体有序（同一日期按学号）");
    printBrief(records, recordCount, "  日期排序前 5 条：");

    check(recordCount == recordCount, "排序不改变记录条数");

    // ---------- 二、五张报表 ----------
    reportDetail(records, recordCount);
    reportDaily(records, recordCount);
    reportSummary(records, recordCount);
    reportAbnormal(records, recordCount);
    reportLeave(records, recordCount);

    // ---------- 三、自测小结 ----------
    std::cout << "\n===== 自测结束：";
    if (failures == 0) {
        std::cout << "全部通过 =====\n";
        return 0;
    }
    std::cout << failures << " 项失败 =====\n";
    return 1;
}
