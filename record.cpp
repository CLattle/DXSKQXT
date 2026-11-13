/* ============================================================================
 *  record.cpp —— 考勤系统数据层实现（甲 负责）
 *
 *  包含四部分：
 *    1. 字段校验      对应数据库的 NOT NULL / CHECK
 *    2. 增删改查      对应 INSERT / UPDATE / DELETE / SELECT
 *    3. 文件读写      一行一条记录，用 '|' 分隔
 *    4. 显示用的状态符号
 * ========================================================================== */

#include "record.h"

#include <cstdlib>    // atoi
#include <fstream>    // ifstream / ofstream
#include <sstream>    // istringstream

/* ------------------------- 内存中的表（全局） ---------------------------- */
Record records[MAX_RECORDS];
int recordCount = 0;

/* ==========================================================================
 *  一、校验
 * ========================================================================== */

/* 返回某月天数；月份不在 1~12 时返回 0。
 * 说明：日期只有月和日、没有年份，分不出闰年，所以 2 月按 29 天算。 */
int daysInMonth(int month)
{
    static const int days[13] = { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month < 1 || month > 12) {
        return 0;
    }
    return days[month];
}

/* 学号：非空、长度 6~12、每一位都是数字 */
bool validId(const string &id)
{
    if (id.size() < 6 || id.size() > 12) {
        return false;
    }
    for (size_t i = 0; i < id.size(); ++i) {
        if (id[i] < '0' || id[i] > '9') {
            return false;
        }
    }
    return true;
}

/* 姓名：非空、不超过 20 字节、不能含竖线（会破坏文件格式） */
bool validName(const string &name)
{
    if (name.empty() || name.size() > 20) {
        return false;
    }
    return name.find('|') == string::npos;
}

/* 班级：非空、不超过 30 字节、不能含竖线 */
bool validClass(const string &cls)
{
    if (cls.empty() || cls.size() > 30) {
        return false;
    }
    return cls.find('|') == string::npos;
}

/* 日期：月 1~12，日 1~该月天数 */
bool validDate(int month, int day)
{
    int limit = daysInMonth(month);

    if (limit == 0) {
        return false;
    }
    return day >= 1 && day <= limit;
}

/* 状态码：0~5 */
bool validStatus(int status)
{
    return status >= 0 && status < STATUS_COUNT;
}

/* 整行校验：按字段顺序检查，遇到第一个错误就返回并说明原因。
 * 参数：r   —— 待检查的记录
 *       err —— 输出，失败原因（成功时不改） */
bool validRecord(const Record &r, string &err)
{
    if (!validId(r.id)) {
        err = "学号必须是 6~12 位数字";
        return false;
    }
    if (!validName(r.name)) {
        err = "姓名不能为空、不能超过 20 字节、不能含 |";
        return false;
    }
    if (!validClass(r.cls)) {
        err = "班级不能为空、不能超过 30 字节、不能含 |";
        return false;
    }
    if (!validDate(r.month, r.day)) {
        err = "日期不合法（月份 1~12，日期不能超过该月天数）";
        return false;
    }
    if (!validStatus(r.status)) {
        err = "状态码必须是 0~5";
        return false;
    }
    return true;
}

/* ==========================================================================
 *  二、增删改查
 * ========================================================================== */

/* 新增一条记录。
 * 处理顺序：校验 -> 表是否满 -> 主键查重 -> 追加到表尾。
 * 查重就是“同一个学号同一天不能有两条记录”。 */
int addRecord(const Record &r, string &err)
{
    if (!validRecord(r, err)) {
        return ERR_INVALID;
    }
    if (recordCount >= MAX_RECORDS) {
        err = "记录已满，不能再增加";
        return ERR_FULL;
    }
    if (findRecord(r.id, r.month, r.day) >= 0) {
        err = "该学号在该日期已有记录，不能重复录入";
        return ERR_DUP;
    }

    records[recordCount] = r;
    ++recordCount;
    return OK;
}

/* 修改第 index 条记录。
 * 注意：如果学号或日期被改成了另一条记录的主键，要拒绝；
 *       但如果主键没变、只改状态码，查重时会“查到自己”，所以要排除 index。 */
int updateRecord(int index, const Record &r, string &err)
{
    if (index < 0 || index >= recordCount) {
        err = "记录不存在";
        return ERR_NOTFOUND;
    }
    if (!validRecord(r, err)) {
        return ERR_INVALID;
    }

    int same = findRecord(r.id, r.month, r.day);
    if (same >= 0 && same != index) {
        err = "修改后与另一条记录重复（同学号同日期）";
        return ERR_DUP;
    }

    records[index] = r;
    return OK;
}

/* 删除第 index 条记录，后面的记录整体前移一格。 */
int deleteRecord(int index)
{
    if (index < 0 || index >= recordCount) {
        return ERR_NOTFOUND;
    }
    for (int i = index; i < recordCount - 1; ++i) {
        records[i] = records[i + 1];
    }
    --recordCount;
    return OK;
}

/* 按 (学号, 日期) 查找，返回下标；找不到返回 -1。
 * 修改、删除前都先用它定位。 */
int findRecord(const string &id, int month, int day)
{
    for (int i = 0; i < recordCount; ++i) {
        if (records[i].id == id &&
            records[i].month == month &&
            records[i].day == day) {
            return i;
        }
    }
    return -1;
}

/* 按学号查找，一名学生可能有多天记录，所以返回的是下标数组。 */
//idx[]为输出数组，max为该输出数组的最大容量
int findById(const string &id, int idx[], int max)
{
    int n = 0;

    for (int i = 0; i < recordCount && n < max; ++i) {
        if (records[i].id == id) {
            idx[n] = i;
            ++n;
        }
    }
    return n;
}

/* 按姓名查找（精确匹配）。 */
int findByName(const string &name, int idx[], int max)
{
    int n = 0;

    for (int i = 0; i < recordCount && n < max; ++i) {
        if (records[i].name == name) {
            idx[n] = i;
            ++n;
        }
    }
    return n;
}

/* 按日期查找（某一天全班的考勤情况）。 */
int findByDate(int month, int day, int idx[], int max)
{
    int n = 0;

    for (int i = 0; i < recordCount && n < max; ++i) {
        if (records[i].month == month && records[i].day == day) {
            idx[n] = i;
            ++n;
        }
    }
    return n;
}

/* ==========================================================================
 *  三、文件读写
 *
 *  文件格式：一行一条记录，6 个字段用 '|' 分隔
 *      学号|姓名|班级|月|日|状态码
 *  以 '#' 开头的行是注释（表头），读的时候跳过。
 * ========================================================================== */

/* 从文件读入全部记录。读之前先清空内存里的表。
 * 坏行（字段个数不对、日期非法……）直接跳过，不影响其它记录。 */
int loadData(const char *path)
{
    ifstream in(path);
    if (!in) {
        return ERR_FILE;              // 文件不存在，交给主程序决定怎么处理
    }

    recordCount = 0;

    string line;
    while (getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);          // Windows 换行
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        istringstream iss(line);
        Record r;
        string sm, sd, ss;

        getline(iss, r.id, '|');
        getline(iss, r.name, '|');
        getline(iss, r.cls, '|');
        getline(iss, sm, '|');
        getline(iss, sd, '|');
        getline(iss, ss);
        if (sm.empty() || sd.empty() || ss.empty()) {
            continue;                 // 字段不够 6 个
        }

        r.month = atoi(sm.c_str());
        r.day = atoi(sd.c_str());
        r.status = atoi(ss.c_str());

        string err;
        if (!validRecord(r, err)) {
            continue;                 // 数据非法，跳过
        }
        if (findRecord(r.id, r.month, r.day) >= 0) {
            continue;                 // 文件里有重复记录，跳过
        }
        if (recordCount < MAX_RECORDS) {
            records[recordCount] = r;
            ++recordCount;
        }
    }
    return OK;
}

/* 把整张表写回文件。第一行写 '# 注释' 说明各列含义。 */
int saveData(const char *path)
{
    ofstream out(path);
    if (!out) {
        return ERR_FILE;
    }

    out << "# 学号|姓名|班级|月|日|状态码(0出勤 1旷课 2事假 3病假 4迟到 5早退)\n";
    for (int i = 0; i < recordCount; ++i) {
        out << records[i].id << '|'
            << records[i].name << '|'
            << records[i].cls << '|'
            << records[i].month << '|'
            << records[i].day << '|'
            << records[i].status << '\n';
    }
    return OK;
}

/* ==========================================================================
 *  四、显示用的小函数
 * ========================================================================== */

/* 状态码 -> 考勤符号 */
const char *statusSymbol(int status)
{
    switch (status) {
    case PRESENT:        return "√";
    case ABSENT:         return "X";
    case PERSONAL_LEAVE: return "△";
    case SICK_LEAVE:     return "○";
    case LATE:           return "+";
    case EARLY:          return "-";
    default:             return "?";
    }
}

/* 状态码 -> 中文名称 */
const char *statusName(int status)
{
    switch (status) {
    case PRESENT:        return "出勤";
    case ABSENT:         return "旷课";
    case PERSONAL_LEAVE: return "事假";
    case SICK_LEAVE:     return "病假";
    case LATE:           return "迟到";
    case EARLY:          return "早退";
    default:             return "未知";
    }
}
