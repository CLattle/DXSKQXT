// 文件名   : sort_report.cpp
// 模块功能 : 按学号 / 姓名 / 日期排序，并生成五张统计报表。
// 公开接口 : 见 sort_report.h，与甲、丙代码完全兼容。
//
// 出勤率口径：出勤率 = (出勤 + 迟到 + 早退) / 应出勤 × 100%
// 应出勤 = 统计范围内的记录条数（含事假、病假、旷课）；为 0 时按 0.0% 处理。
//
// 约定：只使用数组和下标；报表在静态数组副本上排序，不改变主程序数据顺序；
//       源文件 UTF-8，工程需开启 /utf-8。

#include "sort_report.h"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

using namespace std;   // 本项目统一：省去 std::，只写在 .cpp 里

namespace {

constexpr int kRuleWidth = 74;      // 标题分隔线宽度（显示列数）

// ---------------------------- 显示工具 ----------------------------

// 字符串的显示宽度：一个汉字 2 列，ASCII 1 列
int displayWidth(const string &text)
{
    int width = 0;
    size_t i = 0;
    while (i < text.size()) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < 0x80)      { width += 1; i += 1; }   // ASCII
        else if (c < 0xE0) { width += 1; i += 2; }   // 两字节字符
        else if (c < 0xF0) { width += 2; i += 3; }   // 三字节汉字
        else               { width += 2; i += 4; }   // 四字节字符
    }
    return width;
}

void printSpaces(int n)
{
    if (n > 0) {
        cout << string(static_cast<size_t>(n), ' ');
    }
}

// 左对齐，补空格到 width 列
void printPadded(const string &text, int width)
{
    const int used = displayWidth(text);
    cout << text;
    printSpaces(width - used);
}

// 右对齐（数字用）
void printRight(const string &text, int width)
{
    const int used = displayWidth(text);
    printSpaces(width - used);
    cout << text;
}

void printInt(int value, int width)
{
    printRight(to_string(value), width);
}

// 一行表头：按给定列宽依次左对齐输出
void printHeader(const char *const titles[], const int widths[], int n)
{
    for (int i = 0; i < n; ++i) {
        printPadded(titles[i], widths[i]);
    }
    cout << '\n';
}

// '=' 或 '-' 分隔线
void printLine(int width, char mark)
{
    cout << string(static_cast<size_t>(width), mark) << '\n';
}

// 报表标题：上下各一条 '='，标题居中
void printTitle(const string &title)
{
    int left = (kRuleWidth - displayWidth(title)) / 2;
    if (left < 0) {
        left = 0;
    }
    cout << '\n';
    printLine(kRuleWidth, '=');
    printSpaces(left);
    cout << title << '\n';
    printLine(kRuleWidth, '=');
}

// ---------------------------- 字段格式化 ----------------------------

string formatDate(const Record &record)
{
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d-%02d", record.month, record.day);
    return string(buffer);
}

string formatStatus(int status)
{
    return string(statusName(status)) + " " + statusSymbol(status);
}

string formatRate(double rate)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f%%", rate);
    return string(buffer);
}

int dateValue(const Record &record)
{
    return record.month * 100 + record.day;
}

bool isAbnormal(int status)
{
    return status == ABSENT || status == LATE || status == EARLY;
}

bool isLeave(int status)
{
    return status == PERSONAL_LEAVE || status == SICK_LEAVE;
}

// 出勤率；应出勤为 0 时返回 0
double attendanceRate(int attended, int expected)
{
    return expected <= 0 ? 0.0 : static_cast<double>(attended) * 100.0 / expected;
}

// ---------------------------- 状态计数 ----------------------------

struct StatusCount {
    int present = 0, absent = 0, personal = 0, sick = 0, late = 0, early = 0;

    void add(int status)
    {
        if (status == PRESENT)             ++present;
        else if (status == ABSENT)         ++absent;
        else if (status == PERSONAL_LEAVE) ++personal;
        else if (status == SICK_LEAVE)     ++sick;
        else if (status == LATE)           ++late;
        else if (status == EARLY)          ++early;
    }

    int attended() const { return present + late + early; }
    int abnormal() const { return absent + late + early; }
    int leave() const    { return personal + sick; }
};

StatusCount countStatus(const Record *data, int begin, int end)
{
    StatusCount counter;
    for (int i = begin; i < end; ++i) {
        counter.add(data[i].status);
    }
    return counter;
}

// 复制到 copy 并按 compare 排序（报表专用副本）
void fillSorted(Record *copy, const Record *data, int count,
                bool (*compare)(const Record &, const Record &))
{
    for (int i = 0; i < count; ++i) {
        copy[i] = data[i];
    }
    sort(copy, copy + count, compare);
}

// 打印"明细"型表格（异常表、请假表共用），返回命中的条数
int printDetailTable(const Record *sorted, int count, const char *heading,
                     const char *typeLabel, bool (*filter)(int), StatusCount &counter)
{
    const char *titles[] = { "序号", "学号", "姓名", "班级", "日期", typeLabel };
    const int widths[] = { 5, 13, 9, 9, 7, 10 };

    cout << heading << '\n';
    printPadded(titles[0], 5);
    cout << ' ';
    printHeader(titles + 1, widths + 1, 5);
    printLine(54, '-');

    int sequence = 0;
    for (int i = 0; i < count; ++i) {
        if (!filter(sorted[i].status)) {
            continue;
        }
        ++sequence;
        counter.add(sorted[i].status);
        printInt(sequence, 5); cout << ' ';
        printPadded(sorted[i].id, 13); printPadded(sorted[i].name, 9);
        printPadded(sorted[i].cls, 9); printPadded(formatDate(sorted[i]), 7);
        printPadded(statusName(sorted[i].status), 10); cout << '\n';
    }
    return sequence;
}

}  // namespace

// ============================ 排序 ============================

bool compareById(const Record &left, const Record &right)
{
    if (left.id != right.id)       return left.id < right.id;
    if (left.month != right.month) return left.month < right.month;
    if (left.day != right.day)     return left.day < right.day;
    return left.name < right.name;
}

bool compareByName(const Record &left, const Record &right)
{
    if (left.name != right.name)   return left.name < right.name;
    if (left.id != right.id)       return left.id < right.id;
    if (left.month != right.month) return left.month < right.month;
    return left.day < right.day;
}

bool compareByDate(const Record &left, const Record &right)
{
    if (left.month != right.month) return left.month < right.month;
    if (left.day != right.day)     return left.day < right.day;
    return left.id < right.id;
}

void sortRecords(Record *data, int count, SortKey key)
{
    if (data == nullptr || count < 2) {
        return;
    }
    if (key == SORT_BY_NAME)      sort(data, data + count, compareByName);
    else if (key == SORT_BY_DATE) sort(data, data + count, compareByDate);
    else                          sort(data, data + count, compareById);
}

// ============================ 五张报表 ============================

// 明细表：按日期列出全部记录
void reportDetail(const Record *data, int count)
{
    printTitle("考 勤 明 细 表");
    if (data == nullptr || count <= 0) {
        cout << "暂无考勤记录。\n";
        return;
    }

    static Record sorted[MAX_RECORDS];
    fillSorted(sorted, data, count, compareByDate);

    const char *titles[] = { "序号", "学号", "姓名", "班级", "日期", "状态" };
    const int widths[] = { 5, 13, 9, 9, 7, 10 };
    printPadded(titles[0], 5);
    cout << ' ';
    printHeader(titles + 1, widths + 1, 5);
    printLine(54, '-');

    for (int i = 0; i < count; ++i) {
        printInt(i + 1, 5); cout << ' ';
        printPadded(sorted[i].id, 13); printPadded(sorted[i].name, 9);
        printPadded(sorted[i].cls, 9); printPadded(formatDate(sorted[i]), 7);
        printPadded(formatStatus(sorted[i].status), 10); cout << '\n';
    }

    const StatusCount c = countStatus(sorted, 0, count);
    printLine(54, '-');
    cout << "合计 " << count << " 条：出勤 " << c.present
              << "，旷课 " << c.absent << "，事假 " << c.personal
              << "，病假 " << c.sick << "，迟到 " << c.late
              << "，早退 " << c.early << '\n';
    cout << "整体出勤率：(出勤 " << c.present << " + 迟到 " << c.late
              << " + 早退 " << c.early << ") / 应出勤 " << count
              << " = " << formatRate(attendanceRate(c.attended(), count)) << '\n';
}

// 日报表：按日期汇总，一天一行
void reportDaily(const Record *data, int count)
{
    printTitle("考 勤 日 报 表");
    if (data == nullptr || count <= 0) {
        cout << "暂无考勤记录。\n";
        return;
    }

    static Record sorted[MAX_RECORDS];
    fillSorted(sorted, data, count, compareByDate);

    const char *titles[] = { "日期", "应出勤", "出勤", "旷课", "事假", "病假", "迟到", "早退", "出勤率" };
    const int widths[] = { 8, 8, 6, 6, 6, 6, 6, 6, 8 };
    printHeader(titles, widths, 9);
    printLine(60, '-');

    int totalAttended = 0;
    for (int index = 0; index < count; ) {
        int end = index + 1;
        while (end < count && dateValue(sorted[index]) == dateValue(sorted[end])) {
            ++end;
        }
        const StatusCount c = countStatus(sorted, index, end);
        const int expected = end - index;
        totalAttended += c.attended();

        printPadded(formatDate(sorted[index]), 8); printInt(expected, 8);
        printInt(c.present, 6); printInt(c.absent, 6); printInt(c.personal, 6);
        printInt(c.sick, 6); printInt(c.late, 6); printInt(c.early, 6);
        printRight(formatRate(attendanceRate(c.attended(), expected)), 8); cout << '\n';
        index = end;
    }

    printLine(60, '-');
    cout << "合计：应出勤 " << count << " 次，到岗 " << totalAttended
              << " 次，出勤率 " << formatRate(attendanceRate(totalAttended, count)) << '\n';
}

// 汇总表：按学号汇总，一名学生一行
void reportSummary(const Record *data, int count)
{
    printTitle("考 勤 汇 总 表");
    if (data == nullptr || count <= 0) {
        cout << "暂无考勤记录。\n";
        return;
    }

    static Record sorted[MAX_RECORDS];
    fillSorted(sorted, data, count, compareById);

    const char *titles[] = { "学号", "姓名", "班级", "应出勤", "出勤", "旷课", "事假", "病假", "迟到", "早退", "出勤率" };
    const int widths[] = { 13, 9, 9, 7, 5, 5, 5, 5, 5, 5, 7 };
    printHeader(titles, widths, 11);
    printLine(74, '-');

    int totalAttended = 0;
    int students = 0;
    for (int index = 0; index < count; ) {
        int end = index + 1;
        while (end < count && sorted[index].id == sorted[end].id) {
            ++end;
        }
        const StatusCount c = countStatus(sorted, index, end);
        const int expected = end - index;
        totalAttended += c.attended();
        ++students;

        printPadded(sorted[index].id, 13); printPadded(sorted[index].name, 9);
        printPadded(sorted[index].cls, 9); printInt(expected, 7);
        printInt(c.present, 5); printInt(c.absent, 5); printInt(c.personal, 5);
        printInt(c.sick, 5); printInt(c.late, 5); printInt(c.early, 5);
        printRight(formatRate(attendanceRate(c.attended(), expected)), 7); cout << '\n';
        index = end;
    }

    printLine(74, '-');
    cout << "共 " << students << " 名学生，" << count
              << " 条记录；总体出勤率 " << formatRate(attendanceRate(totalAttended, count)) << '\n';
}

// 异常表：旷课 / 迟到 / 早退明细 + 每个学生的异常次数
void reportAbnormal(const Record *data, int count)
{
    printTitle("考 勤 异 常 表");
    if (data == nullptr || count <= 0) {
        cout << "暂无考勤记录。\n";
        return;
    }

    static Record byDate[MAX_RECORDS];
    fillSorted(byDate, data, count, compareByDate);

    StatusCount counter;
    const int sequence = printDetailTable(byDate, count,
        "[一] 异常明细（旷课 / 迟到 / 早退，按日期排列）", "异常类型", isAbnormal, counter);
    if (sequence == 0) {
        cout << "本次统计范围内没有旷课、迟到、早退记录。\n";
    } else {
        cout << "异常合计 " << sequence << " 条：旷课 " << counter.absent
                  << "，迟到 " << counter.late << "，早退 " << counter.early << '\n';
    }

    static Record byId[MAX_RECORDS];
    fillSorted(byId, data, count, compareById);

    cout << "\n[二] 异常学生汇总（按学号排列）\n";
    const char *titles[] = { "学号", "姓名", "班级", "旷课", "迟到", "早退", "异常合计", "出勤率" };
    const int widths[] = { 13, 9, 9, 6, 6, 6, 10, 8 };
    printHeader(titles, widths, 8);
    printLine(67, '-');

    for (int index = 0; index < count; ) {
        int end = index + 1;
        while (end < count && byId[index].id == byId[end].id) {
            ++end;
        }
        const StatusCount c = countStatus(byId, index, end);
        if (c.abnormal() > 0) {
            printPadded(byId[index].id, 13); printPadded(byId[index].name, 9);
            printPadded(byId[index].cls, 9);
            printInt(c.absent, 6); printInt(c.late, 6); printInt(c.early, 6);
            printInt(c.abnormal(), 10);
            printRight(formatRate(attendanceRate(c.attended(), end - index)), 8); cout << '\n';
        }
        index = end;
    }
}

// 请假表：事假 / 病假明细 + 每个学生的请假次数
void reportLeave(const Record *data, int count)
{
    printTitle("请 假 汇 总 表");
    if (data == nullptr || count <= 0) {
        cout << "暂无考勤记录。\n";
        return;
    }

    static Record byDate[MAX_RECORDS];
    fillSorted(byDate, data, count, compareByDate);

    StatusCount counter;
    const int sequence = printDetailTable(byDate, count,
        "[一] 请假明细（事假 / 病假，按日期排列）", "请假类型", isLeave, counter);
    if (sequence == 0) {
        cout << "本次统计范围内没有事假、病假记录。\n";
    } else {
        cout << "请假合计 " << sequence << " 次：事假 " << counter.personal
                  << " 次，病假 " << counter.sick << " 次\n";
    }

    static Record byId[MAX_RECORDS];
    fillSorted(byId, data, count, compareById);

    cout << "\n[二] 学生请假汇总（按学号排列）\n";
    const char *titles[] = { "学号", "姓名", "班级", "事假", "病假", "合计", "请假率" };
    const int widths[] = { 13, 9, 9, 6, 6, 6, 8 };
    printHeader(titles, widths, 7);
    printLine(57, '-');

    for (int index = 0; index < count; ) {
        int end = index + 1;
        while (end < count && byId[index].id == byId[end].id) {
            ++end;
        }
        const StatusCount c = countStatus(byId, index, end);
        if (c.leave() > 0) {
            printPadded(byId[index].id, 13); printPadded(byId[index].name, 9);
            printPadded(byId[index].cls, 9);
            printInt(c.personal, 6); printInt(c.sick, 6); printInt(c.leave(), 6);
            printRight(formatRate(attendanceRate(c.leave(), end - index)), 8); cout << '\n';
        }
        index = end;
    }
}
