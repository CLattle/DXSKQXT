/* ============================================================
 *  attendance_core.h —— 界面无关的业务核心（丙的 MFC 版，书内层）
 *
 *  由原控制台版 main.cpp（丙）改写而来：
 *    · 分页、格式化、字段规格、调用甲的逻辑原样保留；
 *    · 原提示语（printf 风格）改成按“字段编号”生成中文串；
 *    · 原 readLine/readInt 负责的“怎么读”，交给界面层（MFC 控件）。
 *
 *  本文件只用教材《面向对象程序设计及C++（第3版）》的知识：
 *    2.2 名字空间 / 2.3 string 类型 / 2.5 引用 / 3.1~3.4 类与对象 /
 *    4.2 静态成员 / 4.4 常成员函数 / 2.4.1 域解析符 / 8.1.1 streambuf
 *  不含 MFC、不含 windows.h、不含 STL 容器（报表抓取用 ostringstream，
 *  属于 8.1 流类库）。
 * ============================================================ */

#ifndef ATTENDANCE_CORE_H
#define ATTENDANCE_CORE_H

#include "record.h"
#include "sort_report.h"

#include <string>

/* 同上：头文件里不写 using namespace std;，避免 std::byte 与 Windows 的 byte 冲突 */
using std::string;

class AttendanceCore
{
public:
    /* 字段编号：界面层用它来问“某一个字段”，核心据此给出规格与提示语 */
    enum {
        FIELD_ID = 0,      // 学号
        FIELD_NAME,        // 姓名
        FIELD_CLASS,       // 班级
        FIELD_MONTH,       // 月份
        FIELD_DAY,         // 日期
        FIELD_STATUS,      // 状态码
        FIELD_KEY,         // 查找关键字
        FIELD_COUNT
    };

    /* 报表编号 */
    enum {
        REPORT_DETAIL = 0, // 考勤明细表
        REPORT_DAILY,      // 考勤日报表
        REPORT_SUMMARY,    // 考勤汇总表
        REPORT_ABNORMAL,   // 考勤异常表
        REPORT_LEAVE,      // 请假汇总表
        REPORT_ALL,        // 全部显示
        REPORT_COUNT
    };

    AttendanceCore(const string& dataFile);        // 3.4.1 构造函数 + 3.4.4 初始化列表

    /* ---------- 字段规格与提示语（代替原来的 prompt 字符串） ---------- */
    int    fieldLow(int field) const;              // 数值字段下限
    int    fieldHigh(int field) const;             // 数值字段上限
    bool   fieldIsText(int field) const;           // 是否为文本字段
    string getPrompt(int field) const;             // 输入框标签
    string getInvalidMessage(int field) const;     // “输入无效”的完整提示
    string valueToString(int field, int value) const;  // 状态码 -> “出勤 √”

    /* ---------- 文本字段校验（规则与甲的 validId 等保持一致） ---------- */
    bool checkText(int field, const string& text, string& reason) const;

    /* ---------- 业务操作：全部转交给甲（2.4.1 域解析符） ---------- */
    bool add(const Record& r, string& reason);
    bool update(int index, const Record& r, string& reason);
    bool remove(int index);
    bool save() const;                             // 4.4.2 常成员函数
    bool load() const;
    int  recordSize() const;                       // 返回 recordCount
    const Record& recordAt(int index) const;       // 返回 records[index]

    /* ---------- 查找 / 收集下标（转交甲的 findById 等） ---------- */
    int findById(const string& id, int idx[], int max) const;
    int findByName(const string& name, int idx[], int max) const;
    int findByDate(int month, int day, int idx[], int max) const;
    int collectAll(int idx[], int max) const;

    /* ---------- 排序 / 分页（排序交给乙，分页算法与原来一致） ---------- */
    void sort(SortKey key) const;
    int  pageCount(int total) const;               // (n + PAGE_SIZE - 1) / PAGE_SIZE
    int  pageStart(int page) const;
    int  pageStop(int page, int total) const;

    /* ---------- 格式化（2.3 string，纯字符运算不用 sprintf） ---------- */
    string formatDate(int month, int day) const;
    string statusText(int status) const;           // “出勤 √”

    /* ---------- 报表：把乙的 cout 输出抓成字符串（8.1.1 streambuf） ---------- */
    string report(int which) const;

    string getPath() const;                        // 数据文件路径
    const char* getPathC() const;                  // 给甲的 c_str()

private:
    static const int PAGE_SIZE = 12;               // 4.2.1 静态数据成员
    string path;                                   // 数据文件路径
};

#endif
