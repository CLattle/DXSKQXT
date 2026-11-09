// 文件名   : sort_report.cpp
// 模块作者 : 乙（排序与统计）
// 模块功能 : 本模块负责考勤系统的「排序」和「统计报表」两部分：
//            1) 提供按学号、姓名、日期三种排序（std::sort + 比较函数）；
//            2) 生成五张报表：考勤明细表、考勤日报表、考勤汇总表、
//               考勤异常表、请假汇总表。
// 语言标准 : C++14 及以上（VS2022 默认即可）
// 依赖     : record.h（甲的数据层：Record、状态码、records/recordCount）
//            sort_report.h（本模块对外接口）
// 使用说明 : 本模块只读取调用方传入的记录，不会改变原有顺序——
//            需要排序时一律在副本上排序，报表结果输出到 std::cout。
//
// 出勤率口径（全模块统一，务必与文档一致）：
//     出勤率 = (出勤次数 + 迟到次数 + 早退次数) / 应出勤次数 × 100%
//   · 应出勤次数：统计范围内「应当到场」的考勤记录条数。考勤按次记录，
//     每一次考勤都要求到场，因此 应出勤 = 该范围内的记录总数
//     （其中也包含事假、病假、旷课的记录）。
//   · 迟到、早退虽未全程在岗，但仍算「已出勤」，因此计入分子；
//   · 事假、病假、旷课不计入分子；
//   · 应出勤为 0 时出勤率按 0.0% 处理，避免除以零。

#include "sort_report.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace {

// ============================ 一、常量 ============================

constexpr int kRuleWidth = 74;      // 报表标题分隔线宽度（字符显示宽度）

// ======================= 二、控制台输出工具 =======================

// 判断当前字符串是 UTF-8 还是 GBK：汉字「中」在两种编码下的字节数不同。
// （工程若用 /utf-8 编译，这里自动走 UTF-8 分支，保证中文列对齐。）
bool encodingIsUtf8()
{
    static const bool cached = (std::string("中").size() == 3);
    return cached;
}

// 计算字符串在控制台上的显示宽度：中文/中文标点占 2 列，ASCII 占 1 列。
// 报表对齐必须按显示宽度而不是字节数补空格，否则中文列会错位。
int displayWidth(const std::string &text)
{
    int width = 0;
    const unsigned char *cursor = reinterpret_cast<const unsigned char *>(text.c_str());

    if (encodingIsUtf8()) {
        while (*cursor != '\0') {
            if (*cursor < 0x80) {
                width += 1;
                cursor += 1;
            } else if ((*cursor & 0xE0) == 0xC0) {
                width += 1;
                cursor += 2;
            } else if ((*cursor & 0xF0) == 0xE0) {
                width += 2;
                cursor += 3;
            } else {
                width += 2;
                cursor += 4;
            }
        }
        return width;
    }

    while (*cursor != '\0') {
        if (*cursor < 0x80) {
            width += 1;
            cursor += 1;
        } else {
            width += 2;
            cursor += 2;
        }
    }
    return width;
}

// 输出指定个数的空格
void printSpaces(int count)
{
    if (count > 0) {
        std::cout << std::string(static_cast<std::size_t>(count), ' ');
    }
}

// 左对齐输出并用空格补齐到指定显示宽度
void printPadded(const std::string &text, int width)
{
    const int used = displayWidth(text);
    std::cout << text;
    if (used < width) {
        printSpaces(width - used);
    }
}

// 右对齐输出（用于数字列）
void printRightAligned(const std::string &text, int width)
{
    const int used = displayWidth(text);
    if (used < width) {
        printSpaces(width - used);
    }
    std::cout << text;
}

// 右对齐输出一个整数
void printIntRight(int value, int width)
{
    printRightAligned(std::to_string(value), width);
}

// 把百分比格式化为「87.5%」形式
std::string formatRate(double rate)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f%%", rate);
    return std::string(buffer);
}

// 右对齐输出一个百分比
void printRateRight(double rate, int width)
{
    printRightAligned(formatRate(rate), width);
}

// 输出一条整行分隔线
void printRule()
{
    std::cout << std::string(kRuleWidth, '=') << '\n';
}

// 输出一条指定宽度的虚线（用于表头与表尾）
void printDashLine(int width)
{
    std::cout << std::string(static_cast<std::size_t>(width), '-') << '\n';
}

// 输出报表标题：上下各一条分隔线，标题尽量居中
void printTitle(const std::string &title)
{
    int left = (kRuleWidth - displayWidth(title)) / 2;
    if (left < 0) {
        left = 0;
    }
    std::cout << '\n';
    printRule();
    printSpaces(left);
    std::cout << title << '\n';
    printRule();
}

// ======================= 三、记录字段格式化 =======================

// 把考勤日期格式化为「月-日」
std::string formatDate(const Record &record)
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d-%02d", record.month, record.day);
    return std::string(buffer);
}

// 把考勤状态格式化为「中文 + 符号」，例如「出勤 √」
// 中文和符号都复用甲在 record.cpp 里实现的 statusName / statusSymbol
std::string formatStatus(int status)
{
    return std::string(statusName(status)) + " " + statusSymbol(status);
}

// 日期换算成可比较的整数（月 * 100 + 日）
int dateValue(const Record &record)
{
    return record.month * 100 + record.day;
}

// 是否属于「异常」状态（旷课、迟到、早退）
bool isAbnormal(int status)
{
    return status == ABSENT || status == LATE || status == EARLY;
}

// 是否属于「请假」状态（事假、病假）
bool isLeave(int status)
{
    return status == PERSONAL_LEAVE || status == SICK_LEAVE;
}

// 按出勤率口径计算百分比；应出勤为 0 时返回 0.0
double attendanceRate(int attended, int expected)
{
    if (expected <= 0) {
        return 0.0;
    }
    return static_cast<double>(attended) * 100.0 / static_cast<double>(expected);
}

// ======================= 四、状态计数小工具 =======================

// 一段记录内各考勤状态的出现次数
struct StatusCount {
    int present  = 0;   // 出勤
    int absent   = 0;   // 旷课
    int personal = 0;   // 事假
    int sick     = 0;   // 病假
    int late     = 0;   // 迟到
    int early    = 0;   // 早退

    void add(int status)
    {
        switch (status) {
        case PRESENT:        ++present;  break;
        case ABSENT:         ++absent;   break;
        case PERSONAL_LEAVE: ++personal; break;
        case SICK_LEAVE:     ++sick;     break;
        case LATE:           ++late;     break;
        case EARLY:          ++early;    break;
        default:                          break;
        }
    }

    int attended() const { return present + late + early; }   // 到岗 = 出勤 + 迟到 + 早退
    int abnormal() const { return absent + late + early; }    // 异常 = 旷课 + 迟到 + 早退
    int leave() const    { return personal + sick; }          // 请假 = 事假 + 病假
};

// 统计 data[begin..end) 区间内各状态次数
StatusCount countStatus(const Record *data, int begin, int end)
{
    StatusCount counter;
    for (int i = begin; i < end; ++i) {
        counter.add(data[i].status);
    }
    return counter;
}

// 复制一份记录并按指定比较函数排序（报表在副本上排序，不改变调用方顺序）
std::vector<Record> sortedCopy(const Record *data, int count,
                               bool (*compare)(const Record &, const Record &))
{
    std::vector<Record> copy(data, data + count);
    std::sort(copy.begin(), copy.end(), compare);
    return copy;
}

}  // namespace

// ======================= 五、排序功能 =======================

// 按学号升序；学号相同按日期，日期也相同按姓名
bool compareById(const Record &left, const Record &right)
{
    return std::tie(left.id, left.month, left.day, left.name)
         < std::tie(right.id, right.month, right.day, right.name);
}

// 按姓名升序；姓名相同按学号，最后按日期
// 说明：中文姓名按 UTF-8 编码顺序比较（标准库 strcmp 语义的字符串比较），
//       同音不同字不保证拼音先后，课程设计要求到「能按姓名排序」即可。
bool compareByName(const Record &left, const Record &right)
{
    return std::tie(left.name, left.id, left.month, left.day)
         < std::tie(right.name, right.id, right.month, right.day);
}

// 按日期升序；题目要求「日期相同再按学号」
bool compareByDate(const Record &left, const Record &right)
{
    return std::tie(left.month, left.day, left.id)
         < std::tie(right.month, right.day, right.id);
}

// 按指定方式排序：data 为记录数组，count 为条数，key 为排序方式
void sortRecords(Record *data, int count, SortKey key)
{
    if (data == nullptr || count < 2) {
        return;
    }
    switch (key) {
    case SORT_BY_NAME:
        std::sort(data, data + count, compareByName);
        break;
    case SORT_BY_DATE:
        std::sort(data, data + count, compareByDate);
        break;
    case SORT_BY_ID:
    default:
        std::sort(data, data + count, compareById);
        break;
    }
}

// ======================= 六、五张统计报表 =======================

// 考勤明细表：按日期列出每一条考勤记录
void reportDetail(const Record *data, int count)
{
    printTitle("考 勤 明 细 表");

    if (data == nullptr || count <= 0) {
        std::cout << "暂无考勤记录。\n";
        return;
    }

    const std::vector<Record> sorted = sortedCopy(data, count, compareByDate);

    printPadded("序号", 5);
    std::cout << ' ';
    printPadded("学号", 13);
    printPadded("姓名", 9);
    printPadded("班级", 9);
    printPadded("日期", 7);
    printPadded("状态", 10);
    std::cout << '\n';
    printDashLine(54);

    int sequence = 0;
    for (const Record &record : sorted) {
        ++sequence;
        printIntRight(sequence, 5);
        std::cout << ' ';
        printPadded(record.id, 13);
        printPadded(record.name, 9);
        printPadded(record.cls, 9);
        printPadded(formatDate(record), 7);
        printPadded(formatStatus(record.status), 10);
        std::cout << '\n';
    }

    const StatusCount counter = countStatus(data, 0, count);

    printDashLine(54);
    std::cout << "合计 " << count << " 条：出勤 " << counter.present
              << "，旷课 " << counter.absent
              << "，事假 " << counter.personal
              << "，病假 " << counter.sick
              << "，迟到 " << counter.late
              << "，早退 " << counter.early << '\n';
    std::cout << "整体出勤率：(出勤 " << counter.present
              << " + 迟到 " << counter.late
              << " + 早退 " << counter.early
              << ") / 应出勤 " << count
              << " = " << formatRate(attendanceRate(counter.attended(), count)) << '\n';
}

// 考勤日报表：按考勤日期汇总当天的出勤情况
void reportDaily(const Record *data, int count)
{
    printTitle("考 勤 日 报 表");

    if (data == nullptr || count <= 0) {
        std::cout << "暂无考勤记录。\n";
        return;
    }

    const std::vector<Record> sorted = sortedCopy(data, count, compareByDate);

    printPadded("日期", 8);
    printPadded("应出勤", 8);
    printPadded("出勤", 6);
    printPadded("旷课", 6);
    printPadded("事假", 6);
    printPadded("病假", 6);
    printPadded("迟到", 6);
    printPadded("早退", 6);
    printPadded("出勤率", 8);
    std::cout << '\n';
    printDashLine(60);

    int totalAttended = 0;
    int index = 0;
    while (index < count) {
        int end = index + 1;
        // 找出同一天的全部记录区间 [index, end)
        while (end < count && dateValue(sorted[index]) == dateValue(sorted[end])) {
            ++end;
        }

        const StatusCount counter = countStatus(sorted.data(), index, end);
        const int expected = end - index;
        totalAttended += counter.attended();

        printPadded(formatDate(sorted[index]), 8);
        printIntRight(expected, 8);
        printIntRight(counter.present, 6);
        printIntRight(counter.absent, 6);
        printIntRight(counter.personal, 6);
        printIntRight(counter.sick, 6);
        printIntRight(counter.late, 6);
        printIntRight(counter.early, 6);
        printRateRight(attendanceRate(counter.attended(), expected), 8);
        std::cout << '\n';

        index = end;
    }

    printDashLine(60);
    std::cout << "合计：应出勤 " << count
              << " 次，到岗 " << totalAttended
              << " 次，出勤率 " << formatRate(attendanceRate(totalAttended, count)) << '\n';
}

// 考勤汇总表：按学生（学号）汇总每人的考勤情况
void reportSummary(const Record *data, int count)
{
    printTitle("考 勤 汇 总 表");

    if (data == nullptr || count <= 0) {
        std::cout << "暂无考勤记录。\n";
        return;
    }

    const std::vector<Record> sorted = sortedCopy(data, count, compareById);

    printPadded("学号", 13);
    printPadded("姓名", 9);
    printPadded("班级", 9);
    printPadded("应出勤", 7);
    printPadded("出勤", 5);
    printPadded("旷课", 5);
    printPadded("事假", 5);
    printPadded("病假", 5);
    printPadded("迟到", 5);
    printPadded("早退", 5);
    printPadded("出勤率", 7);
    std::cout << '\n';
    printDashLine(74);

    int totalAttended = 0;
    int students = 0;
    int index = 0;
    while (index < count) {
        int end = index + 1;
        // 同一学号的连续记录合并为一行
        while (end < count && sorted[index].id == sorted[end].id) {
            ++end;
        }

        const StatusCount counter = countStatus(sorted.data(), index, end);
        const int expected = end - index;
        totalAttended += counter.attended();
        ++students;

        printPadded(sorted[index].id, 13);
        printPadded(sorted[index].name, 9);
        printPadded(sorted[index].cls, 9);
        printIntRight(expected, 7);
        printIntRight(counter.present, 5);
        printIntRight(counter.absent, 5);
        printIntRight(counter.personal, 5);
        printIntRight(counter.sick, 5);
        printIntRight(counter.late, 5);
        printIntRight(counter.early, 5);
        printRateRight(attendanceRate(counter.attended(), expected), 7);
        std::cout << '\n';

        index = end;
    }

    printDashLine(74);
    std::cout << "共 " << students << " 名学生，" << count
              << " 条记录；总体出勤率 " << formatRate(attendanceRate(totalAttended, count)) << '\n';
}

// 考勤异常表：列出全部旷课、迟到、早退记录，并按学生统计异常次数
void reportAbnormal(const Record *data, int count)
{
    printTitle("考 勤 异 常 表");

    if (data == nullptr || count <= 0) {
        std::cout << "暂无考勤记录。\n";
        return;
    }

    const std::vector<Record> byDate = sortedCopy(data, count, compareByDate);

    // ---------- 1. 异常明细 ----------
    std::cout << "[一] 异常明细（旷课 / 迟到 / 早退，按日期排列）\n";
    printPadded("序号", 5);
    std::cout << ' ';
    printPadded("学号", 13);
    printPadded("姓名", 9);
    printPadded("班级", 9);
    printPadded("日期", 7);
    printPadded("异常类型", 10);
    std::cout << '\n';
    printDashLine(54);

    StatusCount counter;
    int sequence = 0;
    for (const Record &record : byDate) {
        if (!isAbnormal(record.status)) {
            continue;
        }
        ++sequence;
        counter.add(record.status);

        printIntRight(sequence, 5);
        std::cout << ' ';
        printPadded(record.id, 13);
        printPadded(record.name, 9);
        printPadded(record.cls, 9);
        printPadded(formatDate(record), 7);
        printPadded(statusName(record.status), 10);
        std::cout << '\n';
    }

    if (sequence == 0) {
        std::cout << "本次统计范围内没有旷课、迟到、早退记录。\n";
    } else {
        std::cout << "异常合计 " << sequence << " 条：旷课 " << counter.absent
                  << "，迟到 " << counter.late
                  << "，早退 " << counter.early << '\n';
    }

    // ---------- 2. 按学生的异常汇总 ----------
    const std::vector<Record> byId = sortedCopy(data, count, compareById);

    std::cout << "\n[二] 异常学生汇总（按学号排列）\n";
    printPadded("学号", 13);
    printPadded("姓名", 9);
    printPadded("班级", 9);
    printPadded("旷课", 6);
    printPadded("迟到", 6);
    printPadded("早退", 6);
    printPadded("异常合计", 10);
    printPadded("出勤率", 8);
    std::cout << '\n';
    printDashLine(67);

    int index = 0;
    while (index < count) {
        int end = index + 1;
        while (end < count && byId[index].id == byId[end].id) {
            ++end;
        }

        const StatusCount group = countStatus(byId.data(), index, end);
        if (group.abnormal() > 0) {
            const int expected = end - index;
            printPadded(byId[index].id, 13);
            printPadded(byId[index].name, 9);
            printPadded(byId[index].cls, 9);
            printIntRight(group.absent, 6);
            printIntRight(group.late, 6);
            printIntRight(group.early, 6);
            printIntRight(group.abnormal(), 10);
            printRateRight(attendanceRate(group.attended(), expected), 8);
            std::cout << '\n';
        }
        index = end;
    }
}

// 请假汇总表：列出全部事假、病假记录，并按学生统计请假次数
void reportLeave(const Record *data, int count)
{
    printTitle("请 假 汇 总 表");

    if (data == nullptr || count <= 0) {
        std::cout << "暂无考勤记录。\n";
        return;
    }

    const std::vector<Record> byDate = sortedCopy(data, count, compareByDate);

    // ---------- 1. 请假明细 ----------
    std::cout << "[一] 请假明细（事假 / 病假，按日期排列）\n";
    printPadded("序号", 5);
    std::cout << ' ';
    printPadded("学号", 13);
    printPadded("姓名", 9);
    printPadded("班级", 9);
    printPadded("日期", 7);
    printPadded("请假类型", 10);
    std::cout << '\n';
    printDashLine(54);

    int personalTotal = 0;
    int sickTotal = 0;
    int sequence = 0;
    for (const Record &record : byDate) {
        if (!isLeave(record.status)) {
            continue;
        }
        ++sequence;
        if (record.status == PERSONAL_LEAVE) {
            ++personalTotal;
        } else {
            ++sickTotal;
        }

        printIntRight(sequence, 5);
        std::cout << ' ';
        printPadded(record.id, 13);
        printPadded(record.name, 9);
        printPadded(record.cls, 9);
        printPadded(formatDate(record), 7);
        printPadded(statusName(record.status), 10);
        std::cout << '\n';
    }

    if (sequence == 0) {
        std::cout << "本次统计范围内没有事假、病假记录。\n";
    } else {
        std::cout << "请假合计 " << sequence << " 次：事假 " << personalTotal
                  << " 次，病假 " << sickTotal << " 次\n";
    }

    // ---------- 2. 按学生的请假汇总 ----------
    const std::vector<Record> byId = sortedCopy(data, count, compareById);

    std::cout << "\n[二] 学生请假汇总（按学号排列）\n";
    printPadded("学号", 13);
    printPadded("姓名", 9);
    printPadded("班级", 9);
    printPadded("事假", 6);
    printPadded("病假", 6);
    printPadded("合计", 6);
    printPadded("请假率", 8);
    std::cout << '\n';
    printDashLine(57);

    int index = 0;
    while (index < count) {
        int end = index + 1;
        while (end < count && byId[index].id == byId[end].id) {
            ++end;
        }

        const StatusCount group = countStatus(byId.data(), index, end);
        if (group.leave() > 0) {
            const int expected = end - index;
            printPadded(byId[index].id, 13);
            printPadded(byId[index].name, 9);
            printPadded(byId[index].cls, 9);
            printIntRight(group.personal, 6);
            printIntRight(group.sick, 6);
            printIntRight(group.leave(), 6);
            printRateRight(attendanceRate(group.leave(), expected), 8);
            std::cout << '\n';
        }
        index = end;
    }
}
