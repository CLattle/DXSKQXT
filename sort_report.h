// 文件名   : sort_report.h
// 模块作者 : 乙（排序与统计）
// 模块功能 : 排序与五张统计报表对外接口。数据层结构来自甲的 record.h，
//            本头文件只声明乙负责的函数，不重复定义状态码、数据表和状态显示函数。
// 语言标准 : C++14 及以上（VS2022 默认即可）
// 依赖     : record.h

#ifndef SORT_REPORT_H
#define SORT_REPORT_H

#include "record.h"

// 排序方式：按学号 / 按姓名 / 按考勤日期
enum SortKey {
    SORT_BY_ID   = 0,
    SORT_BY_NAME = 1,
    SORT_BY_DATE = 2
};

// 三个比较函数：返回 true 表示 left 应排在 right 前面（std::sort 的要求）
// 按学号：学号相同按日期，日期相同按姓名
bool compareById(const Record &left, const Record &right);
// 按姓名：姓名相同按学号，最后按日期
bool compareByName(const Record &left, const Record &right);
// 按日期：日期相同按学号（题目要求）
bool compareByDate(const Record &left, const Record &right);

// 按指定方式排序，直接改变 data[0..count) 的顺序
// 参数：data 记录数组首地址；count 记录条数；key 排序方式
void sortRecords(Record *data, int count, SortKey key);

// 五张统计报表：只读取数据，不修改顺序，结果输出到 std::cout
// 参数统一为：data 记录数组首地址；count 记录条数
void reportDetail(const Record *data, int count);    // 考勤明细表
void reportDaily(const Record *data, int count);     // 考勤日报表
void reportSummary(const Record *data, int count);   // 考勤汇总表
void reportAbnormal(const Record *data, int count);  // 考勤异常表
void reportLeave(const Record *data, int count);     // 请假汇总表

#endif  // SORT_REPORT_H
