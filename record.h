#ifndef RECORD_H
#define RECORD_H

/* ============================================================================
 *  record.h —— 考勤系统数据层（甲 负责）
 *
 *  思路：把数据看成一张表
 *      表   ：records[] + recordCount
 *      行   ：struct Record
 *      主键 ：(学号, 月, 日)      一个学生一天只能有一条记录
 *      约束 ：validRecord()       相当于数据库的 NOT NULL / CHECK
 * ========================================================================== */

#include <string>
using namespace std;

const int MAX_RECORDS = 1000;    // 最多保存多少条记录

/* ---------------- 考勤状态码（存数字，显示时再换成符号） ---------------- */
enum {
    PRESENT = 0,        // 出勤 √
    ABSENT,             // 旷课 X
    PERSONAL_LEAVE,     // 事假 △
    SICK_LEAVE,         // 病假 ○
    LATE,               // 迟到 +
    EARLY,              // 早退 -
    STATUS_COUNT        // 状态总数（6）
};

/* ---------------------------- 函数返回码 -------------------------------- */
const int OK           =  0;    // 成功
const int ERR_DUP      = -1;    // 学号 + 日期 重复
const int ERR_INVALID  = -2;    // 字段不合法
const int ERR_FULL     = -3;    // 表已满
const int ERR_NOTFOUND = -4;    // 记录不存在
const int ERR_FILE     = -5;    // 文件打不开

/* ------------------------- 一条考勤记录（一行） -------------------------- */
struct Record {
    string id;      // 学号：6~12 位数字，主键 1
    string name;    // 姓名：非空，不超过 20 字节
    string cls;     // 班级：非空，不超过 30 字节
    int month;      // 月：1~12，主键 2
    int day;        // 日：1~该月天数，主键 2
    int status;     // 状态码：0~5
};

/* --------------------------- 内存中的表 ---------------------------------- */
/* 全局变量，定义在 record.cpp 里，其他文件只要 include 本头文件就能用 */
extern Record records[MAX_RECORDS];
extern int recordCount;

/* ------------------------------ 校验 ------------------------------------- */
int  daysInMonth(int month);                    // 返回某月天数，月份非法返回 0
bool validId(const string &id);                 // 学号：6~12 位纯数字
bool validName(const string &name);             // 姓名：非空、≤20 字节、不含 '|'
bool validClass(const string &cls);             // 班级：非空、≤30 字节、不含 '|'
bool validDate(int month, int day);             // 日期：月 1~12、日 1~该月天数
bool validStatus(int status);                   // 状态码：0~5

/* 整行校验：全部合法返回 true；否则返回 false，并把原因写进 err */
bool validRecord(const Record &r, string &err);

/* ---------------------------- 增删改查 ----------------------------------- */
/* 新增：先校验，再按 (学号,日期) 查重。返回值 OK / ERR_INVALID / ERR_DUP / ERR_FULL */
int addRecord(const Record &r, string &err);

/* 修改第 index 条：校验新值，若主键被改动则需查重（排除自己）。
 * 返回 OK / ERR_NOTFOUND / ERR_INVALID / ERR_DUP */
int updateRecord(int index, const Record &r, string &err);

/* 删除第 index 条，后面的记录依次前移。返回 OK / ERR_NOTFOUND */
int deleteRecord(int index);

/* 按主键定位，返回下标；找不到返回 -1 */
int findRecord(const string &id, int month, int day);

/* 以下三个是“一对多”查找：把命中的下标写进 idx 数组，返回命中条数（最多 max 个） */
int findById(const string &id, int idx[], int max);
int findByName(const string &name, int idx[], int max);
int findByDate(int month, int day, int idx[], int max);

/* ------------------------------ 文件 ------------------------------------- */
/* 从文本文件读入，成功返回 OK，文件打不开返回 ERR_FILE */
int loadData(const char *path);

/* 保存到文本文件，成功返回 OK，写不了返回 ERR_FILE */
int saveData(const char *path);

/* ------------------------------ 显示 ------------------------------------- */
const char *statusSymbol(int status);   // 0 -> "√"，非法 -> "?"
const char *statusName(int status);     // 0 -> "出勤"，非法 -> "未知"

#endif
