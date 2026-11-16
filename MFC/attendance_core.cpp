/* ============================================================
 *  attendance_core.cpp —— 业务核心实现（丙的 MFC 版，书内层）
 *
 *  与界面完全无关：这里不读控件、不弹消息框、不碰 MFC。
 *  界面层只负责“把控件里的文本变成参数”和“把结果画回去”。
 * ============================================================ */

#include "attendance_core.h"

#include <iostream>
#include <sstream>

/* 头文件里不再 using namespace std;，这里补上 */
using namespace std;

/* ------------------------------------------------------------
 *  构造
 * ---------------------------------------------------------- */

/* 3.4.1 构造函数 + 3.4.4 初始化列表：对象一创建就拿到数据文件路径 */
AttendanceCore::AttendanceCore(const string& dataFile) : path(dataFile)
{
}

/* ------------------------------------------------------------
 *  一、字段规格与提示语
 *
 *  原来控制台版把这些提示语散落在 menuAdd 等处；现在集中到一处，
 *  界面层按字段编号来取，保证“提示文字”和“校验规则”永远一致。
 * ---------------------------------------------------------- */

int AttendanceCore::fieldLow(int field) const
{
    switch (field) {
    case FIELD_MONTH:  return 1;
    case FIELD_DAY:    return 1;
    case FIELD_STATUS: return 0;
    default:           return 0;
    }
}

int AttendanceCore::fieldHigh(int field) const
{
    switch (field) {
    case FIELD_MONTH:  return 12;
    case FIELD_DAY:    return 31;
    case FIELD_STATUS: return STATUS_COUNT - 1;    // 0~5
    default:           return 0;
    }
}

bool AttendanceCore::fieldIsText(int field) const
{
    return field == FIELD_ID || field == FIELD_NAME || field == FIELD_CLASS;
}

/* 输入框标签（对应原来 readText/readInt 的 prompt 参数） */
string AttendanceCore::getPrompt(int field) const
{
    switch (field) {
    case FIELD_ID:     return "学号（6~12 位数字）";
    case FIELD_NAME:   return "姓名（不超过 20 字节）";
    case FIELD_CLASS:  return "班级（不超过 30 字节）";
    case FIELD_MONTH:  return "月份（1~12）";
    case FIELD_DAY:    return "日期（1~31）";
    case FIELD_STATUS: return "状态码（0~5）";
    case FIELD_KEY:    return "查找关键字";
    default:           return "输入";
    }
}

/* “输入无效”提示：对应原来 readInt 里 cout << "  [!] 输入无效：请输入 "
 *                                      << low << " ~ " << high ... */
string AttendanceCore::getInvalidMessage(int field) const
{
    switch (field) {
    case FIELD_MONTH:  return "月份输入无效：请输入 1 ~ 12 之间的整数。";
    case FIELD_DAY:    return "日期输入无效：请输入 1 ~ 31 之间的整数。";
    case FIELD_STATUS: return "状态码输入无效：请输入 0 ~ 5 之间的整数。";
    case FIELD_ID:     return "学号输入无效：必须是 6~12 位数字。";
    case FIELD_NAME:   return "姓名输入无效：不能为空、不能超过 20 字节、不能含 |。";
    case FIELD_CLASS:  return "班级输入无效：不能为空、不能超过 30 字节、不能含 |。";
    default:           return "输入无效。";
    }
}

/* 状态码 -> “出勤 √”（原 printLine 里的 statusName + statusSymbol） */
string AttendanceCore::valueToString(int field, int value) const
{
    if (field == FIELD_STATUS) {
        return statusText(value);
    }
    char buffer[16];
    buffer[0] = '\0';

    if (value < 0) {
        return string("0");
    }

    int i = 0;
    if (value == 0) {
        buffer[i++] = '0';
    }
    else {
        char tmp[16];
        int n = 0;
        int v = value;
        while (v > 0 && n < 15) {
            tmp[n++] = (char)('0' + v % 10);
            v /= 10;
        }
        while (n > 0) {
            buffer[i++] = tmp[--n];
        }
    }
    buffer[i] = '\0';
    return string(buffer);
}

/* ------------------------------------------------------------
 *  二、文本字段校验
 *
 *  与甲的 validId / validName / validClass 规则一致，提前在界面层拦一道，
 *  避免用户填完所有字段才被甲整体拒绝。
 * ---------------------------------------------------------- */

bool AttendanceCore::checkText(int field, const string& text, string& reason) const
{
    reason.clear();

    if (field == FIELD_ID) {
        if (text.size() < 6 || text.size() > 12) {
            reason = getInvalidMessage(FIELD_ID);
            return false;
        }
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') {
                reason = getInvalidMessage(FIELD_ID);
                return false;
            }
        }
        return true;
    }

    if (field == FIELD_NAME) {
        if (text.empty() || text.size() > 20 || text.find('|') != string::npos) {
            reason = getInvalidMessage(FIELD_NAME);
            return false;
        }
        return true;
    }

    if (field == FIELD_CLASS) {
        if (text.empty() || text.size() > 30 || text.find('|') != string::npos) {
            reason = getInvalidMessage(FIELD_CLASS);
            return false;
        }
        return true;
    }

    return true;
}

/* ------------------------------------------------------------
 *  三、业务操作：全部转交甲
 *
 *  “::” 是 2.4.1 的域解析符，明确表示“调用全局名字空间里的甲的函数”，
 *  避免和本类同名成员混淆。
 * ---------------------------------------------------------- */

bool AttendanceCore::add(const Record& r, string& reason)
{
    reason = "未知错误";
    return ::addRecord(r, reason) == OK;
}

bool AttendanceCore::update(int index, const Record& r, string& reason)
{
    reason = "未知错误";
    return ::updateRecord(index, r, reason) == OK;
}

bool AttendanceCore::remove(int index)
{
    return ::deleteRecord(index) == OK;
}

bool AttendanceCore::save() const
{
    return ::saveData(path.c_str()) == OK;
}

bool AttendanceCore::load() const
{
    return ::loadData(path.c_str()) == OK;
}

int AttendanceCore::recordSize() const
{
    return recordCount;
}

const Record& AttendanceCore::recordAt(int index) const
{
    return records[index];
}

/* ------------------------------------------------------------
 *  四、查找：一律交给甲
 * ---------------------------------------------------------- */

int AttendanceCore::findById(const string& id, int idx[], int max) const
{
    return ::findById(id, idx, max);
}

int AttendanceCore::findByName(const string& name, int idx[], int max) const
{
    return ::findByName(name, idx, max);
}

int AttendanceCore::findByDate(int month, int day, int idx[], int max) const
{
    return ::findByDate(month, day, idx, max);
}

/* 收集全部下标（对应原来 menuList 里的 for 循环） */
int AttendanceCore::collectAll(int idx[], int max) const
{
    int n = 0;
    for (int i = 0; i < recordCount && n < max; ++i) {
        idx[n] = i;
        ++n;
    }
    return n;
}

/* ------------------------------------------------------------
 *  五、排序与分页
 * ---------------------------------------------------------- */

void AttendanceCore::sort(SortKey key) const
{
    ::sortRecords(records, recordCount, key);      // 乙：原地排序
}

int AttendanceCore::pageCount(int total) const
{
    if (total <= 0) {
        return 0;
    }
    return (total + PAGE_SIZE - 1) / PAGE_SIZE;    // 向上取整
}

int AttendanceCore::pageStart(int page) const
{
    if (page < 0) {
        return 0;
    }
    return page * PAGE_SIZE;
}

int AttendanceCore::pageStop(int page, int total) const
{
    const int stop = pageStart(page) + PAGE_SIZE;
    return (stop < total) ? stop : total;
}

/* ------------------------------------------------------------
 *  六、格式化
 * ---------------------------------------------------------- */

/* 月日 -> "05-06"，纯字符运算（对比原来 formatDate 的写法） */
string AttendanceCore::formatDate(int month, int day) const
{
    char buffer[6];
    buffer[0] = (char)('0' + (month / 10) % 10);
    buffer[1] = (char)('0' + month % 10);
    buffer[2] = '-';
    buffer[3] = (char)('0' + (day / 10) % 10);
    buffer[4] = (char)('0' + day % 10);
    buffer[5] = '\0';
    return string(buffer);
}

/* 状态码 -> “出勤 √”（复用甲的状态名和符号） */
string AttendanceCore::statusText(int status) const
{
    return string(statusName(status)) + " " + statusSymbol(status);
}

/* ------------------------------------------------------------
 *  七、报表：把乙的 cout 输出“抓”成字符串
 *
 *  原理（教材 8.1.1 streambuf）：
 *    cout 只是“外壳”，真正搬运字符的是它内部的 streambuf。
 *    把 cout 的 rdbuf() 换成 ostringstream 的缓冲区，
 *    乙内部的 cout << ... 就全部写进这个字符串流里；
 *    用完必须换回去，否则后面的输出会写进已失效的对象。
 * ---------------------------------------------------------- */

string AttendanceCore::report(int which) const
{
    ostringstream capture;
    streambuf* old = cout.rdbuf(capture.rdbuf());   // 接管 cout

    if (which == REPORT_DETAIL) {
        reportDetail(records, recordCount);
    }
    else if (which == REPORT_DAILY) {
        reportDaily(records, recordCount);
    }
    else if (which == REPORT_SUMMARY) {
        reportSummary(records, recordCount);
    }
    else if (which == REPORT_ABNORMAL) {
        reportAbnormal(records, recordCount);
    }
    else if (which == REPORT_LEAVE) {
        reportLeave(records, recordCount);
    }
    else {
        reportDetail(records, recordCount);
        reportDaily(records, recordCount);
        reportSummary(records, recordCount);
        reportAbnormal(records, recordCount);
        reportLeave(records, recordCount);
    }

    cout.rdbuf(old);                                // 恢复原缓冲区
    return capture.str();
}

/* ------------------------------------------------------------
 *  八、路径与静态成员定义
 * ---------------------------------------------------------- */

string AttendanceCore::getPath() const
{
    return path;
}

const char* AttendanceCore::getPathC() const
{
    return path.c_str();
}

const int AttendanceCore::PAGE_SIZE;                // 4.2.1 类外定义
