/* ============================================================================
 *  main.cpp —— 大学生考勤系统 · 界面层（丙）
 *
 *  全部写法来自《面向对象程序设计及C++（第3版）》：
 *    2.2  名字空间的定义及使用      → using namespace std;
 *    2.3  新增字符串的处理          → string 类型
 *    2.5  引用的定义与应用          → const string& 作形参
 *    3.1  类的定义 / 3.2 访问属性    → class AttendanceUI 的 public/private
 *    3.4  构造函数与析构函数        → 构造函数 + 初始化列表
 *    3.6  对象的应用                → 对象作参数（Record 传引用）
 *    4.2  静态成员                  → static const int PAGE_SIZE
 *    4.4  常成员                    → 常成员函数（不修改对象状态）
 *    6.2  运算符重载                → operator<< 输出一条 Record
 *    8.2  键盘输入与屏幕输出        → cin.getline / cout
 *
 *  刻意不使用：vector、to_string、范围 for、auto、constexpr、lambda、
 *  std::sort、strtol、sprintf —— 这些均不在本书范围内。
 *
 *  中文显示：程序按 UTF-8 输出，开头用两个 Windows API 把控制台代码页
 *  切到 UTF-8（不是 C++ 语言知识，属于运行环境适配），编译时需加 /utf-8。
 * ========================================================================== */

#include "record.h"
#include "sort_report.h"

#include <iostream>
#include <string>
#include <windows.h>        // 只为 SetConsoleOutputCP / SetConsoleCP

using namespace std;

/* ----------------------------------------------------------------------------
 *  文件内的小工具（都是普通的自由函数，不属于任何类）
 * -------------------------------------------------------------------------- */

const int LINE_SIZE = 256;          // 一次最多读入的字符数

bool isSpaceChar(char ch)           // 代替 isspace
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}

void trim(string& text)             // 去掉首尾空白
{
    size_t begin = 0;
    size_t end = text.size();

    while (begin < end && isSpaceChar(text[begin])) {
        ++begin;
    }
    while (end > begin && isSpaceChar(text[end - 1])) {
        --end;
    }
    text = text.substr(begin, end - begin);
}

/* 把月、日格式化成 "05-06"，不用 sprintf，纯字符运算 */
string formatDate(int month, int day)
{
    char buffer[6];
    buffer[0] = (char)('0' + month / 10);
    buffer[1] = (char)('0' + month % 10);
    buffer[2] = '-';
    buffer[3] = (char)('0' + day / 10);
    buffer[4] = (char)('0' + day % 10);
    buffer[5] = '\0';
    return string(buffer);
}

/* 运算符重载：用 cout << record 直接输出一条记录（6.2 运算符重载） */
ostream& operator<<(ostream& os, const Record& r)
{
    os << r.id << ' ' << r.name << ' ' << r.cls << ' '
        << formatDate(r.month, r.day) << " 状态码 " << r.status;
    return os;
}

/* ============================================================================
 *  一、界面类
 *
 *  界面层只做三件事：转发调用、读输入并校验格式、按显示宽度分页打印。
 * ========================================================================== */
class AttendanceUI
{
public:
    AttendanceUI(const string& dataFile);   // 构造函数
    void run();                             // 主循环

private:
    /* ---------- 输入 ---------- */
    bool readLine(string& line) const;
    bool readText(const string& prompt, string& value) const;
    bool readInt(const string& prompt, int low, int high, int& value) const;
    bool readOptionalInt(const string& prompt, int low, int high,
        int& value, const string& fieldName) const;
    bool askYesNo(const string& prompt) const;
    bool parseRange(const string& text, int low, int high, int& value) const;

    /* ---------- 输出 ---------- */
    int    textWidth(const string& text) const;      // 显示宽度：汉字算 2 列
    string clip(const string& text, int maxWidth) const;
    void   printPadded(const string& text, int w) const;
    void   printRight(const string& text, int w) const;
    void   printIntRight(int value, int w) const;
    void   printHeader() const;
    void   printLine(int index) const;
    void   showPage(const int* idx, int n) const;    // 分页显示一批下标

    /* ---------- 菜单 ---------- */
    void printMenu() const;
    void menuAdd();
    void menuFind();
    void menuUpdate();
    void menuDelete();
    void menuList();
    void menuStat();
    void menuSave() const;
    void saveToFile() const;                         // 保存逻辑只写一份
    void menuLoad();
    void menuExit() const;

    static const int PAGE_SIZE = 12;                 // 静态数据成员（4.2.1）
    string path;                                     // 数据文件路径
};

const int AttendanceUI::PAGE_SIZE;                   // 类外定义

/* ============================================================================
 *  二、构造与主循环
 * ========================================================================== */

AttendanceUI::AttendanceUI(const string& dataFile) : path(dataFile)   // 初始化列表
{
}

void AttendanceUI::run()
{
    if (loadData(path.c_str()) != OK) {
        cout << "  提示：未找到数据文件 " << path
            << "，可作为新系统开始使用。\n";
    }
    else {
        cout << "  已从 " << path << " 读入 " << recordCount << " 条记录。\n";
    }

    for (;;) {
        printMenu();

        int choice = 0;
        /* 只有输入流结束才返回 false；范围外的输入会被反复重问 */
        if (!readInt("  请选择功能（1~9）：", 1, 9, choice)) {
            cout << "\n  输入已结束，程序退出。\n";
            return;
        }

        switch (choice) {
        case 1: menuAdd();    break;
        case 2: menuFind();   break;
        case 3: menuUpdate(); break;
        case 4: menuDelete(); break;
        case 5: menuList();   break;
        case 6: menuStat();   break;
        case 7: menuSave();   break;
        case 8: menuLoad();   break;
        case 9: menuExit();   return;
        }
        /* readInt 已保证 1~9，不需要 default */
    }
}

/* ============================================================================
 *  三、输入：整行读取 + 范围解析
 * ========================================================================== */

 /* 读一整行。成功返回 true；输入流结束返回 false。 */
bool AttendanceUI::readLine(string& line) const
{
    char buffer[LINE_SIZE] = { 0 };

    if (!cin.getline(buffer, LINE_SIZE)) {
        if (cin.eof()) {                 // 流结束
            return false;
        }
        cin.clear();                     // 本行超长：清状态，丢掉剩余字符
        while (cin.get() != '\n' && !cin.eof()) {
        }
    }

    line = buffer;
    trim(line);
    return true;
}

/* 严格解析：整串都是数字，且在 [low, high] 内。 */
bool AttendanceUI::parseRange(const string& text, int low, int high, int& value) const
{
    if (text.empty()) {
        return false;
    }

    int result = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        char ch = text[i];
        if (ch < '0' || ch > '9') {
            return false;
        }
        result = result * 10 + (ch - '0');
        if (result > high) {             // 提前剪枝，顺带防溢出
            return false;
        }
    }

    if (result < low) {
        return false;
    }
    value = result;
    return true;
}

/* 反复询问，直到得到 [low, high] 内的整数；流结束返回 false。 */
bool AttendanceUI::readInt(const string& prompt, int low, int high, int& value) const
{
    for (;;) {
        cout << prompt;

        string line;
        if (!readLine(line)) {
            return false;
        }
        if (line.empty()) {              // 直接回车：重问一次
            continue;
        }
        if (parseRange(line, low, high, value)) {
            return true;
        }
        cout << "  [!] 输入无效：请输入 " << low << " ~ " << high
            << " 之间的整数。\n";
    }
}

/* 读一行文本（可能是空串，由调用方决定空串含义）。 */
bool AttendanceUI::readText(const string& prompt, string& value) const
{
    cout << prompt;
    return readLine(value);
}

/* 回车保留原值；输入非法则提示并保留原值。 */
bool AttendanceUI::readOptionalInt(const string& prompt, int low, int high,
    int& value, const string& fieldName) const
{
    cout << prompt;

    string line;
    if (!readLine(line)) {
        return false;
    }
    if (line.empty()) {
        return true;                     // 保留原值
    }

    int parsed = 0;
    if (parseRange(line, low, high, parsed)) {
        value = parsed;
    }
    else {
        cout << "  [!] " << fieldName << "无效，保留原值 " << value << "。\n";
    }
    return true;
}

/* 只接受 y/Y/n/N；流结束按“否”处理。 */
bool AttendanceUI::askYesNo(const string& prompt) const
{
    for (;;) {
        cout << prompt;

        string line;
        if (!readLine(line)) {
            return false;
        }
        if (line == "y" || line == "Y") {
            return true;
        }
        if (line == "n" || line == "N") {
            return false;
        }
        cout << "  [!] 请输入 y 或 n。\n";
    }
}

/* ============================================================================
 *  四、输出：显示宽度与分页
 * ========================================================================== */

 /* 汉字、中文标点占 2 列，ASCII 占 1 列。按 UTF-8 首字节判断。 */
int AttendanceUI::textWidth(const string& text) const
{
    int w = 0;
    const unsigned char* p = (const unsigned char*)text.c_str();

    while (*p != '\0') {
        if (*p < 0x80) {
            w += 1;
            p += 1;
        }
        else if ((*p & 0xE0) == 0xC0) {
            w += 1;
            p += 2;
        }
        else if ((*p & 0xF0) == 0xE0) {
            w += 2;
            p += 3;
        }
        else {
            w += 2;
            p += 4;
        }
    }
    return w;
}

/* 超宽时按显示宽度截断，末尾补 ".."，避免把整张表顶歪。 */
string AttendanceUI::clip(const string& text, int maxWidth) const
{
    if (textWidth(text) <= maxWidth) {
        return text;
    }

    string out;
    int used = 0;
    size_t i = 0;
    const size_t n = text.size();

    while (i < n) {
        unsigned char ch = (unsigned char)text[i];
        size_t step = 1;
        int w = 1;

        if (ch < 0x80) {
            step = 1;
            w = 1;
        }
        else if ((ch & 0xE0) == 0xC0) {
            step = 2;
            w = 1;
        }
        else if ((ch & 0xF0) == 0xE0) {
            step = 3;
            w = 2;
        }
        else {
            step = 4;
            w = 2;
        }

        if (i + step > n || used + w > maxWidth) {
            break;
        }
        out += text.substr(i, step);
        used += w;
        i += step;
    }

    if (used + 2 <= maxWidth) {
        out += "..";
    }
    return out;
}

/* 左对齐并补空格到指定显示宽度 */
void AttendanceUI::printPadded(const string& text, int w) const
{
    const string shown = clip(text, w);
    cout << shown;

    const int used = textWidth(shown);
    if (used < w) {
        cout << string((size_t)(w - used), ' ');
    }
}

/* 右对齐（用于表头文字） */
void AttendanceUI::printRight(const string& text, int w) const
{
    const int used = textWidth(text);
    if (used < w) {
        cout << string((size_t)(w - used), ' ');
    }
    cout << text;
}

/* 右对齐一个整数（代替 to_string） */
void AttendanceUI::printIntRight(int value, int w) const
{
    int digits = 1;
    for (int t = value; t >= 10; t /= 10) {
        ++digits;
    }
    if (digits < w) {
        cout << string((size_t)(w - digits), ' ');
    }
    cout << value;
}

void AttendanceUI::printHeader() const
{
    cout << "\n  ";
    printRight("序号", 4);
    cout << "  ";
    printPadded("学号", 12);
    cout << "  ";
    printPadded("姓名", 12);
    cout << "  ";
    printPadded("班级", 18);
    cout << "  ";
    printPadded("日期", 7);
    cout << "  状态\n";
    cout << "  " << string(74, '-') << '\n';
}

void AttendanceUI::printLine(int index) const
{
    const Record& r = records[index];

    cout << "  ";
    printIntRight(index + 1, 4);
    cout << "  ";
    printPadded(r.id, 12);
    cout << "  ";
    printPadded(r.name, 12);
    cout << "  ";
    printPadded(r.cls, 18);
    cout << "  ";
    printPadded(formatDate(r.month, r.day), 7);
    cout << "  " << statusName(r.status) << ' ' << statusSymbol(r.status) << '\n';
}

/* 分页显示 idx[0..n-1] 指向的记录；每 PAGE_SIZE 条暂停一次。 */
void AttendanceUI::showPage(const int* idx, int n) const
{
    if (n <= 0) {
        cout << "  没有可显示的记录。\n";
        return;
    }

    const int pages = (n + PAGE_SIZE - 1) / PAGE_SIZE;
    int page = 1;

    printHeader();

    for (int i = 0; i < n; ++i) {
        printLine(idx[i]);

        if ((i + 1) % PAGE_SIZE == 0 && i + 1 < n) {
            cout << "  " << string(74, '-') << '\n';
            cout << "  第 " << page << '/' << pages
                << " 页已显示，回车继续，输入 q 退出：";

            string input;
            if (!readLine(input)) {
                return;
            }
            if (input == "q" || input == "Q") {
                cout << "  已提前结束显示。\n";
                return;
            }
            ++page;
            printHeader();
        }
    }

    cout << "  " << string(74, '-') << '\n';
    cout << "  共显示 " << n << " 条记录（每页 " << PAGE_SIZE << " 条）。\n";
}

/* ============================================================================
 *  五、各功能子菜单
 * ========================================================================== */

void AttendanceUI::menuAdd()
{
    cout << "\n=== 1 新增考勤记录 ===\n";

    string id;
    string name;
    string cls;
    if (!readText("  学号（6~12 位数字）：", id)) {
        return;
    }
    if (!readText("  姓名（不超过 20 字节）：", name)) {
        return;
    }
    if (!readText("  班级（不超过 30 字节）：", cls)) {
        return;
    }

    int month = 0;
    int day = 0;
    int status = 0;
    if (!readInt("  月份（1~12）：", 1, 12, month)) {
        return;
    }
    if (!readInt("  日期（1~31，月份天数由数据层校验）：", 1, 31, day)) {
        return;
    }

    cout << "  状态码：0出勤 1旷课 2事假 3病假 4迟到 5早退\n";
    if (!readInt("  状态码（0~5）：", 0, STATUS_COUNT - 1, status)) {
        return;
    }

    Record record;                       // 对象作参数（3.6.4），下面交给甲
    record.id = id;
    record.name = name;
    record.cls = cls;
    record.month = month;
    record.day = day;
    record.status = status;

    string error;
    if (addRecord(record, error) == OK) {
        cout << "  新增成功！当前共 " << recordCount << " 条记录。\n";
    }
    else {
        cout << "  新增失败：" << (error.empty() ? "未知错误" : error) << '\n';
    }
}

/* 查找一律交给甲的 findById / findByName / findByDate，
 * 界面层只负责收集下标、分页显示。 */
void AttendanceUI::menuFind()
{
    cout << "\n=== 2 查找考勤记录 ===\n";
    cout << "  1) 按学号查找\n"
        "  2) 按姓名查找\n"
        "  3) 按日期查找\n"
        "  0) 返回上级菜单\n";

    int choice = 0;
    if (!readInt("  请选择（0~3）：", 0, 3, choice)) {
        return;
    }
    if (choice == 0) {
        return;
    }

    int idx[MAX_RECORDS];
    int n = 0;

    if (choice == 1) {
        string id;
        if (!readText("  请输入学号：", id)) {
            return;
        }
        n = findById(id, idx, MAX_RECORDS);
        if (n == 0) {
            cout << "  未找到学号为 " << id << " 的记录。\n";
            return;
        }
    }
    else if (choice == 2) {
        string name;
        if (!readText("  请输入姓名：", name)) {
            return;
        }
        n = findByName(name, idx, MAX_RECORDS);
        if (n == 0) {
            cout << "  未找到姓名为 " << name << " 的记录。\n";
            return;
        }
    }
    else {
        int month = 0;
        int day = 0;
        if (!readInt("  月份（1~12）：", 1, 12, month)) {
            return;
        }
        if (!readInt("  日期（1~31）：", 1, 31, day)) {
            return;
        }
        n = findByDate(month, day, idx, MAX_RECORDS);
        if (n == 0) {
            cout << "  未找到 " << formatDate(month, day) << " 的记录。\n";
            return;
        }
    }

    cout << "\n  找到以下记录：\n";
    showPage(idx, n);
}

void AttendanceUI::menuUpdate()
{
    cout << "\n=== 3 修改考勤记录 ===\n";
    if (recordCount == 0) {
        cout << "  当前没有记录。\n";
        return;
    }

    string id;
    if (!readText("  请输入要修改记录的学号：", id)) {
        return;
    }

    int month = 0;
    int day = 0;
    if (!readInt("  原记录月份（1~12）：", 1, 12, month)) {
        return;
    }
    if (!readInt("  原记录日期（1~31）：", 1, 31, day)) {
        return;
    }

    const int index = findRecord(id, month, day);
    if (index < 0) {
        cout << "  未找到该学号在该日期的记录。\n";
        return;
    }

    Record record = records[index];       // 隐式调用复制构造函数（3.4.5）
    cout << "  当前内容：" << record << '\n';   // 使用重载的 <<
    cout << "  直接回车表示保留原值。\n";

    string input;
    if (!readText("  学号（回车保留）：", input)) {
        return;
    }
    if (!input.empty()) {
        record.id = input;
    }

    if (!readText("  姓名（回车保留）：", input)) {
        return;
    }
    if (!input.empty()) {
        record.name = input;
    }

    if (!readText("  班级（回车保留）：", input)) {
        return;
    }
    if (!input.empty()) {
        record.cls = input;
    }

    if (!readOptionalInt("  月份（回车保留）：", 1, 12, record.month, "月份")) {
        return;
    }
    if (!readOptionalInt("  日期（回车保留）：", 1, 31, record.day, "日期")) {
        return;
    }

    cout << "  状态码：0出勤 1旷课 2事假 3病假 4迟到 5早退\n";
    if (!readOptionalInt("  状态码（回车保留）：", 0, STATUS_COUNT - 1,
        record.status, "状态码")) {
        return;
    }

    string error;
    if (updateRecord(index, record, error) == OK) {
        cout << "  修改成功。\n";
    }
    else {
        cout << "  修改失败：" << (error.empty() ? "未知错误" : error) << '\n';
    }
}

void AttendanceUI::menuDelete()
{
    cout << "\n=== 4 删除考勤记录 ===\n";
    if (recordCount == 0) {
        cout << "  当前没有记录。\n";
        return;
    }

    string id;
    if (!readText("  请输入要删除记录的学号：", id)) {
        return;
    }

    int month = 0;
    int day = 0;
    if (!readInt("  记录月份（1~12）：", 1, 12, month)) {
        return;
    }
    if (!readInt("  记录日期（1~31）：", 1, 31, day)) {
        return;
    }

    const int index = findRecord(id, month, day);
    if (index < 0) {
        cout << "  未找到该记录。\n";
        return;
    }

    if (!askYesNo("  确认删除该记录？(y/n)：")) {
        cout << "  已取消。\n";
        return;
    }

    if (deleteRecord(index) == OK) {
        cout << "  删除成功，当前共 " << recordCount << " 条记录。\n";
    }
    else {
        cout << "  删除失败。\n";
    }
}

void AttendanceUI::menuList()
{
    cout << "\n=== 5 显示考勤记录列表 ===\n";
    cout << "  排序方式：\n"
        "  1) 按学号（学号→日期→姓名）\n"
        "  2) 按姓名（姓名→学号→日期）\n"
        "  3) 按日期（日期→学号）\n"
        "  0) 不排序，按录入顺序\n";

    int choice = 0;
    if (!readInt("  请选择（0~3）：", 0, 3, choice)) {
        return;
    }

    switch (choice) {                     // 排序交给乙
    case 1:
        sortRecords(records, recordCount, SORT_BY_ID);
        break;
    case 2:
        sortRecords(records, recordCount, SORT_BY_NAME);
        break;
    case 3:
        sortRecords(records, recordCount, SORT_BY_DATE);
        break;
    case 0:
        break;
    }

    if (recordCount == 0) {
        cout << "  提示：当前没有记录，可先使用【1 新增记录】或"
            "【8 读取文件】。\n";
        return;
    }

    int idx[MAX_RECORDS];
    for (int i = 0; i < recordCount; ++i) {
        idx[i] = i;
    }
    showPage(idx, recordCount);
}

void AttendanceUI::menuStat()
{
    cout << "\n=== 6 数据统计（报表） ===\n";
    cout << "  1) 考勤明细表\n"
        "  2) 考勤日报表\n"
        "  3) 考勤汇总表\n"
        "  4) 考勤异常表\n"
        "  5) 请假汇总表\n"
        "  6) 全部显示\n"
        "  0) 返回上级菜单\n";

    int choice = 0;
    if (!readInt("  请选择（0~6）：", 0, 6, choice)) {
        return;
    }

    switch (choice) {                     // 报表全部由乙输出
    case 1:
        reportDetail(records, recordCount);
        break;
    case 2:
        reportDaily(records, recordCount);
        break;
    case 3:
        reportSummary(records, recordCount);
        break;
    case 4:
        reportAbnormal(records, recordCount);
        break;
    case 5:
        reportLeave(records, recordCount);
        break;
    case 6:
        reportDetail(records, recordCount);
        reportDaily(records, recordCount);
        reportSummary(records, recordCount);
        reportAbnormal(records, recordCount);
        reportLeave(records, recordCount);
        break;
    case 0:
        break;
    }
}

void AttendanceUI::saveToFile() const
{
    if (saveData(path.c_str()) == OK) {
        cout << "  已保存 " << recordCount << " 条记录到 " << path << '\n';
    }
    else {
        cout << "  保存失败：无法写入 " << path << '\n';
    }
}

void AttendanceUI::menuSave() const
{
    cout << "\n=== 7 保存数据到文件 ===\n";
    saveToFile();
}

void AttendanceUI::menuLoad()
{
    cout << "\n=== 8 从文件读入数据 ===\n";

    const int rc = loadData(path.c_str());
    if (rc == OK) {
        cout << "  读取成功，当前共 " << recordCount << " 条记录。\n";
    }
    else if (rc == ERR_FILE) {
        cout << "  读取失败：找不到文件 " << path << '\n';
    }
    else {
        cout << "  读取失败（错误码 " << rc << "）。\n";
    }
}

void AttendanceUI::menuExit() const
{
    cout << "\n=== 9 退出系统 ===\n";
    if (recordCount > 0) {
        if (askYesNo("  退出前要先保存到数据文件吗？(y/n)：")) {
            saveToFile();
        }
    }
    cout << "  感谢使用大学生考勤系统，再见！\n";
}

/* ============================================================================
 *  六、主菜单
 * ========================================================================== */

void AttendanceUI::printMenu() const
{
    cout << "\n"
        "  ==================================================================\n"
        "                    大 学 生 考 勤 系 统\n"
        "  ==================================================================\n"
        "     1. 新增考勤记录          2. 查找考勤记录\n"
        "     3. 修改考勤记录          4. 删除考勤记录\n"
        "     5. 显示记录列表          6. 统计报表\n"
        "     7. 保存数据到文件        8. 从文件读入数据\n"
        "     9. 退出系统\n"
        "  ==================================================================\n";
    cout << "  当前记录数：" << recordCount << " 条\n";
}

/* ============================================================================
 *  七、入口
 * ========================================================================== */

int main(int argc, char** argv)
{
    /* 把控制台输入、输出代码页都切到 UTF-8，
     * 否则程序输出的 UTF-8 中文会被 GBK 控制台显示成乱码。 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    string path = "data.txt";
    if (argc >= 2) {
        path = argv[1];
    }

    AttendanceUI ui(path);                // 构造界面对象
    ui.run();                             // 进入主循环
    return 0;
}