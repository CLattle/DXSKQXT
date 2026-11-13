/* ============================================================================
 *  main.cpp —— 大学生考勤系统 · 主控程序（C++ 版）
 *
 *  由 main.c 转换而来，保留原来的菜单、分页、输入校验和全部操作流程，
 *  但不再使用 extern "C" / record_c_api.cpp，而是直接调用 record.h 和
 *  sort_report.h 中的 C++ 接口：
 *
 *      Record、records、recordCount
 *      addRecord / updateRecord / deleteRecord / findRecord
 *      loadData / saveData
 *      sortRecords / reportDetail / reportDaily / reportSummary
 *      reportAbnormal / reportLeave
 *
 *  主要 C++ 化改动：
 *    · 字符串使用 std::string，查找结果使用 std::vector<int>；
 *    · 输入输出使用 std::cin / std::cout，全程读整行再解析；
 *    · 用匿名命名空间封装界面辅助函数，避免全局符号污染；
 *    · 用 Record 直接构造数据，不再经过 char* 字段包装层；
 *    · 保留防御式输入：非法输入、超长输入、EOF 都不会造成死循环。
 *
 *  编译（VS2022 开发者命令行，与 record.cpp / sort_report.cpp 同目录）：
 *      cl /nologo /EHsc /utf-8 main.cpp record.cpp sort_report.cpp /Fe:attendance.exe
 *
 *  运行：
 *      attendance.exe [数据文件]
 *      数据文件默认是 attendance_data.txt。
 * ========================================================================== */

#include "record.h"
#include "sort_report.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

    constexpr int kPageSize = 12;
    constexpr const char* kDefaultDataFile = "attendance_data.txt";

    /* ==========================================================================
     *  一、控制台与输入辅助
     * ========================================================================== */

     /* 把 Windows 控制台的输入、输出代码页切到 UTF-8。
      * 输出被重定向或不在 Windows 上时，什么也不做。 */
    void setupConsole()
    {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
#endif
    }

    /* 读取一整行。成功返回 true；遇到 EOF 返回 false。 */
    bool readLine(std::string& line)
    {
        if (!std::getline(std::cin, line)) {
            return false;
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        return true;
    }

    /* 去掉首尾空白字符。 */
    void trim(std::string& text)
    {
        std::size_t begin = 0;
        while (begin < text.size() &&
            std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
            ++begin;
        }

        std::size_t end = text.size();
        while (end > begin &&
            std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
            --end;
        }

        text = text.substr(begin, end - begin);
    }

    /* 判断整串是否全为十进制数字。 */
    bool allDigits(const std::string& text)
    {
        if (text.empty()) {
            return false;
        }
        for (char ch : text) {
            if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
                return false;
            }
        }
        return true;
    }

    /* 把 text 解析成 [low, high] 内的整数；失败返回 false。 */
    bool parseBoundedInt(const std::string& text, int low, int high, int& out)
    {
        if (!allDigits(text)) {
            return false;
        }

        const long value = std::strtol(text.c_str(), nullptr, 10);
        if (value < low || value > high) {
            return false;
        }

        out = static_cast<int>(value);
        return true;
    }

    /* 读一个整数。
     * 返回 1：成功；0：输入非法（已经提示，可重试）；-1：输入流结束。 */
    int readInt(const std::string& prompt, int low, int high, int& out)
    {
        std::cout << prompt << std::flush;

        std::string line;
        if (!readLine(line)) {
            return -1;
        }
        trim(line);

        if (!parseBoundedInt(line, low, high, out)) {
            std::cout << "  [!] 输入无效：请输入 " << low << "~" << high
                << " 之间的整数。\n";
            return 0;
        }
        return 1;
    }

    /* 反复询问，直到得到 [low, high] 内的整数。
     * 返回 false 表示输入流结束，调用方应退出当前操作。 */
    bool readChoice(const std::string& prompt, int low, int high, int& out)
    {
        for (;;) {
            const int rc = readInt(prompt, low, high, out);
            if (rc == 1) {
                return true;
            }
            if (rc == -1) {
                return false;
            }
            /* rc == 0：已经提示过，继续询问 */
        }
    }

    /* 读取一行文本并去掉首尾空白。成功返回 true。 */
    bool readText(const std::string& prompt, std::string& out)
    {
        std::cout << prompt << std::flush;
        if (!readLine(out)) {
            return false;
        }
        trim(out);
        return true;
    }

    /* 询问 y/n。只接受 y、Y、n、N；EOF 按“否”处理。 */
    bool askYesNo(const std::string& prompt)
    {
        std::string line;
        for (;;) {
            std::cout << prompt << std::flush;
            if (!readLine(line)) {
                return false;
            }
            trim(line);

            if (line == "y" || line == "Y") {
                return true;
            }
            if (line == "n" || line == "N") {
                return false;
            }
            std::cout << "  [!] 请输入 y 或 n。\n";
        }
    }

    /* ==========================================================================
     *  二、UTF-8 显示宽度与分页
     * ========================================================================== */

     /* 计算 UTF-8 字符串在控制台上的显示宽度：ASCII 占 1 列，汉字占 2 列。 */
    int displayWidth(const std::string& text)
    {
        int width = 0;
        std::size_t i = 0;

        while (i < text.size()) {
            const unsigned char ch = static_cast<unsigned char>(text[i]);
            if (ch < 0x80) {
                width += 1;
                i += 1;
            }
            else if ((ch & 0xE0) == 0xC0) {
                width += 1;
                i += 2;
            }
            else if ((ch & 0xF0) == 0xE0) {
                width += 2;
                i += 3;
            }
            else {
                width += 2;
                i += 4;
            }
        }
        return width;
    }

    /* 左对齐输出，并用空格补齐到指定显示宽度。 */
    void printPadded(const std::string& text, int width)
    {
        std::cout << text;
        const int used = displayWidth(text);
        if (used < width) {
            std::cout << std::string(static_cast<std::size_t>(width - used), ' ');
        }
    }

    /* 右对齐输出，用于序号等数字列。 */
    void printRightAligned(const std::string& text, int width)
    {
        const int used = displayWidth(text);
        if (used < width) {
            std::cout << std::string(static_cast<std::size_t>(width - used), ' ');
        }
        std::cout << text;
    }

    std::string formatDate(int month, int day)
    {
        char date[16];
        std::snprintf(date, sizeof(date), "%02d-%02d", month, day);
        return std::string(date);
    }

    void printTableHeader()
    {
        std::cout << "\n  ";
        printRightAligned("序号", 4);
        std::cout << "  ";
        printPadded("学号", 12);
        std::cout << "  ";
        printPadded("姓名", 10);
        std::cout << "  ";
        printPadded("班级", 12);
        std::cout << "  ";
        printPadded("日期", 7);
        std::cout << "  状态\n";
        std::cout << "  " << std::string(70, '-') << '\n';
    }

    void printRecordLine(int index)
    {
        const Record& record = records[index];

        const std::string date = formatDate(record.month, record.day);

        std::cout << "  ";
        printRightAligned(std::to_string(index + 1), 4);
        std::cout << "  ";
        printPadded(record.id, 12);
        std::cout << "  ";
        printPadded(record.name, 10);
        std::cout << "  ";
        printPadded(record.cls, 12);
        std::cout << "  ";
        printPadded(date, 7);
        std::cout << "  " << statusName(record.status)
            << ' ' << statusSymbol(record.status) << '\n';
    }

    /* 按页显示 [start, stop) 区间内的记录。 */
    void showPageRange(int start, int stop)
    {
        if (start < 0) {
            start = 0;
        }
        if (stop > recordCount) {
            stop = recordCount;
        }
        if (start >= stop) {
            std::cout << "  没有可显示的记录。\n";
            return;
        }

        const int total = stop - start;
        const int pages = (total + kPageSize - 1) / kPageSize;

        int shown = 0;
        int page = 1;
        printTableHeader();

        for (int index = start; index < stop; ++index) {
            printRecordLine(index);
            ++shown;

            if (shown % kPageSize == 0 && index + 1 < stop) {
                std::cout << "  " << std::string(70, '-') << '\n';
                std::cout << "  第 " << page << '/' << pages
                    << " 页已显示，回车继续，输入 q 退出：" << std::flush;

                std::string input;
                if (!readLine(input)) {
                    return;
                }
                trim(input);
                if (input == "q" || input == "Q") {
                    std::cout << "  已提前结束显示。\n";
                    return;
                }

                ++page;
                printTableHeader();
            }
        }

        std::cout << "  " << std::string(70, '-') << '\n';
        std::cout << "  共显示 " << shown << " 条记录（第 " << start + 1
            << " 条~第 " << start + shown << " 条，每页 "
            << kPageSize << " 条）。\n";
    }

    /* 显示全部记录（自动分页）。 */
    void showAll()
    {
        showPageRange(0, recordCount);

        if (recordCount == 0) {
            std::cout << "  提示：当前没有记录，可先使用【1 新增记录】或"
                "【8 读取文件】。\n";
        }
    }

    /* ==========================================================================
     *  三、查找结果输出
     * ========================================================================== */

    void printSearchResults(const std::vector<int>& matches)
    {
        if (matches.empty()) {
            return;
        }

        std::cout << "\n  找到以下记录：\n";
        for (int index : matches) {
            printRecordLine(index);
        }
        std::cout << "  共 " << matches.size() << " 条。\n";
    }

    /* ==========================================================================
     *  四、各功能子菜单
     * ========================================================================== */

     /* 1 新增记录 */
    void menuAdd()
    {
        std::cout << "\n=== 1 新增考勤记录 ===\n";

        std::string id;
        std::string name;
        std::string cls;
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
        if (!readChoice("  月份（1~12）：", 1, 12, month)) {
            return;
        }
        if (!readChoice("  日期（1~31，按月份自动校验）：", 1, 31, day)) {
            return;
        }

        std::cout << "  状态码：0出勤 1旷课 2事假 3病假 4迟到 5早退\n";
        if (!readChoice("  状态码（0~5）：", 0, STATUS_COUNT - 1, status)) {
            return;
        }

        Record record;
        record.id = id;
        record.name = name;
        record.cls = cls;
        record.month = month;
        record.day = day;
        record.status = status;

        std::string error;
        const int rc = addRecord(record, error);
        if (rc == OK) {
            std::cout << "  新增成功！当前共 " << recordCount << " 条记录。\n";
        }
        else {
            std::cout << "  新增失败：" << (error.empty() ? "未知错误" : error)
                << '\n';
        }
    }

    /* 2 查找记录（按学号 / 姓名 / 日期） */
    void menuFind()
    {
        std::cout << "\n=== 2 查找考勤记录 ===\n";
        std::cout << "  1) 按学号查找\n"
            "  2) 按姓名查找\n"
            "  3) 按日期查找\n"
            "  0) 返回上级菜单\n";

        int choice = 0;
        if (!readChoice("  请选择（0~3）：", 0, 3, choice)) {
            return;
        }
        if (choice == 0) {
            return;
        }

        if (choice == 1) {
            std::string id;
            if (!readText("  请输入学号：", id)) {
                return;
            }

            std::vector<int> matches;
            for (int i = 0; i < recordCount; ++i) {
                if (records[i].id == id) {
                    matches.push_back(i);
                }
            }

            if (matches.empty()) {
                std::cout << "  未找到学号为 " << id << " 的记录。\n";
            }
            else {
                printSearchResults(matches);
            }
        }
        else if (choice == 2) {
            std::string name;
            if (!readText("  请输入姓名：", name)) {
                return;
            }

            std::vector<int> matches;
            for (int i = 0; i < recordCount; ++i) {
                if (records[i].name == name) {
                    matches.push_back(i);
                }
            }

            if (matches.empty()) {
                std::cout << "  未找到姓名为 " << name << " 的记录。\n";
            }
            else {
                printSearchResults(matches);
            }
        }
        else {
            int month = 0;
            int day = 0;
            if (!readChoice("  月份（1~12）：", 1, 12, month)) {
                return;
            }
            if (!readChoice("  日期（1~31）：", 1, 31, day)) {
                return;
            }

            std::vector<int> matches;
            for (int i = 0; i < recordCount; ++i) {
                if (records[i].month == month && records[i].day == day) {
                    matches.push_back(i);
                }
            }

            if (matches.empty()) {
                std::cout << "  未找到 " << formatDate(month, day)
                    << " 的记录。\n";
            }
            else {
                printSearchResults(matches);
            }
        }
    }

    /* 3 修改记录：先按学号+日期定位，再逐项输入新值。 */
    void menuUpdate()
    {
        std::cout << "\n=== 3 修改考勤记录 ===\n";
        if (recordCount == 0) {
            std::cout << "  当前没有记录。\n";
            return;
        }

        std::string id;
        if (!readText("  请输入要修改记录的学号：", id)) {
            return;
        }

        int month = 0;
        int day = 0;
        if (!readChoice("  原记录月份（1~12）：", 1, 12, month)) {
            return;
        }
        if (!readChoice("  原记录日期（1~31）：", 1, 31, day)) {
            return;
        }

        const int index = findRecord(id, month, day);
        if (index < 0) {
            std::cout << "  未找到该学号在该日期的记录。\n";
            return;
        }

        Record record = records[index];
        std::cout << "  当前内容：" << record.id << ' ' << record.name << ' '
            << record.cls << ' ' << formatDate(record.month, record.day)
            << " 状态码 " << record.status << '\n';
        std::cout << "  直接回车表示保留原值。\n";

        std::string input;

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

        if (!readText("  月份（回车保留）：", input)) {
            return;
        }
        if (!input.empty()) {
            int value = 0;
            if (parseBoundedInt(input, 1, 12, value)) {
                record.month = value;
            }
            else {
                std::cout << "  [!] 月份无效，保留原值 " << record.month << "。\n";
            }
        }

        if (!readText("  日期（回车保留）：", input)) {
            return;
        }
        if (!input.empty()) {
            int value = 0;
            if (parseBoundedInt(input, 1, 31, value)) {
                record.day = value;
            }
            else {
                std::cout << "  [!] 日期无效，保留原值 " << record.day << "。\n";
            }
        }

        std::cout << "  状态码：0出勤 1旷课 2事假 3病假 4迟到 5早退\n";
        if (!readText("  状态码（回车保留）：", input)) {
            return;
        }
        if (!input.empty()) {
            int value = 0;
            if (parseBoundedInt(input, 0, STATUS_COUNT - 1, value)) {
                record.status = value;
            }
            else {
                std::cout << "  [!] 状态码无效，保留原值 " << record.status << "。\n";
            }
        }

        std::string error;
        const int rc = updateRecord(index, record, error);
        if (rc == OK) {
            std::cout << "  修改成功。\n";
        }
        else {
            std::cout << "  修改失败：" << (error.empty() ? "未知错误" : error)
                << '\n';
        }
    }

    /* 4 删除记录 */
    void menuDelete()
    {
        std::cout << "\n=== 4 删除考勤记录 ===\n";
        if (recordCount == 0) {
            std::cout << "  当前没有记录。\n";
            return;
        }

        std::string id;
        if (!readText("  请输入要删除记录的学号：", id)) {
            return;
        }

        int month = 0;
        int day = 0;
        if (!readChoice("  记录月份（1~12）：", 1, 12, month)) {
            return;
        }
        if (!readChoice("  记录日期（1~31）：", 1, 31, day)) {
            return;
        }

        const int index = findRecord(id, month, day);
        if (index < 0) {
            std::cout << "  未找到该记录。\n";
            return;
        }

        if (!askYesNo("  确认删除该记录？(y/n)：")) {
            std::cout << "  已取消。\n";
            return;
        }

        const int rc = deleteRecord(index);
        if (rc == OK) {
            std::cout << "  删除成功，当前共 " << recordCount << " 条记录。\n";
        }
        else {
            std::cout << "  删除失败。\n";
        }
    }

    /* 5 显示列表（按排序方式） */
    void menuList()
    {
        std::cout << "\n=== 5 显示考勤记录列表 ===\n";
        std::cout << "  排序方式：\n"
            "  1) 按学号（学号→日期→姓名）\n"
            "  2) 按姓名（姓名→学号→日期）\n"
            "  3) 按日期（日期→学号）\n"
            "  0) 不排序，按录入顺序\n";

        int choice = 0;
        if (!readChoice("  请选择（0~3）：", 0, 3, choice)) {
            return;
        }

        switch (choice) {
        case 1:
            sortRecords(records, recordCount, SORT_BY_ID);
            break;
        case 2:
            sortRecords(records, recordCount, SORT_BY_NAME);
            break;
        case 3:
            sortRecords(records, recordCount, SORT_BY_DATE);
            break;
        default:
            break;
        }

        showAll();
    }

    /* 6 统计报表 */
    void menuStat()
    {
        std::cout << "\n=== 6 数据统计（报表） ===\n";
        std::cout << "  1) 考勤明细表\n"
            "  2) 考勤日报表\n"
            "  3) 考勤汇总表\n"
            "  4) 考勤异常表\n"
            "  5) 请假汇总表\n"
            "  6) 全部显示\n"
            "  0) 返回上级菜单\n";

        int choice = 0;
        if (!readChoice("  请选择（0~6）：", 0, 6, choice)) {
            return;
        }

        switch (choice) {
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
        default:
            break;
        }
    }

    /* 7 保存数据 */
    void menuSave(const std::string& path)
    {
        std::cout << "\n=== 7 保存数据到文件 ===\n";
        const int rc = saveData(path.c_str());
        if (rc == OK) {
            std::cout << "  已保存 " << recordCount << " 条记录到 " << path << '\n';
        }
        else {
            std::cout << "  保存失败：无法写入 " << path << '\n';
        }
    }

    /* 8 从文件读入 */
    void menuLoad(const std::string& path)
    {
        std::cout << "\n=== 8 从文件读入数据 ===\n";
        const int rc = loadData(path.c_str());
        if (rc == OK) {
            std::cout << "  读取成功，当前共 " << recordCount << " 条记录。\n";
        }
        else if (rc == ERR_FILE) {
            std::cout << "  读取失败：找不到文件 " << path << '\n';
        }
        else {
            std::cout << "  读取失败（错误码 " << rc << "）。\n";
        }
    }

    /* 9 退出 */
    void saveBeforeExit(const std::string& path)
    {
        std::cout << "\n=== 9 退出系统 ===\n";
        if (recordCount > 0) {
            if (askYesNo("  退出前要先保存到数据文件吗？(y/n)：")) {
                const int rc = saveData(path.c_str());
                if (rc == OK) {
                    std::cout << "  已保存 " << recordCount << " 条记录到 "
                        << path << '\n';
                }
                else {
                    std::cout << "  保存失败：无法写入 " << path << '\n';
                }
            }
        }
        std::cout << "  感谢使用大学生考勤系统，再见！\n";
    }

    /* ==========================================================================
     *  五、主菜单
     * ========================================================================== */

    void printMenu()
    {
        std::cout << "\n"
            "  ==================================================================\n"
            "                    大 学 生 考 勤 系 统\n"
            "  ==================================================================\n"
            "     1. 新增考勤记录          2. 查找考勤记录\n"
            "     3. 修改考勤记录          4. 删除考勤记录\n"
            "     5. 显示记录列表          6. 统计报表\n"
            "     7. 保存数据到文件        8. 从文件读入数据\n"
            "     9. 退出系统\n"
            "  ==================================================================\n";
        std::cout << "  当前记录数：" << recordCount << " 条\n";
    }

}  // namespace

int main(int argc, char** argv)
{
    setupConsole();

    const std::string path =
        (argc >= 2) ? std::string(argv[1]) : std::string(kDefaultDataFile);

    /* 启动时尝试自动读入数据文件；文件不存在属于正常情况。 */
    if (loadData(path.c_str()) != OK) {
        std::cout << "  提示：未找到数据文件 " << path
            << "，可作为新系统开始使用。\n";
    }
    else {
        std::cout << "  已从 " << path << " 读入 " << recordCount
            << " 条记录。\n";
    }

    for (;;) {
        printMenu();

        int choice = 0;
        if (!readChoice("  请选择功能（1~9）：", 1, 9, choice)) {
            std::cout << "\n  输入已结束，程序退出。\n";
            return 0;
        }

        switch (choice) {
        case 1:
            menuAdd();
            break;
        case 2:
            menuFind();
            break;
        case 3:
            menuUpdate();
            break;
        case 4:
            menuDelete();
            break;
        case 5:
            menuList();
            break;
        case 6:
            menuStat();
            break;
        case 7:
            menuSave(path);
            break;
        case 8:
            menuLoad(path);
            break;
        case 9:
            saveBeforeExit(path);
            return 0;
        default:
            std::cout << "  [!] 无效选项，请输入 1~9。\n";
            break;
        }
    }
}