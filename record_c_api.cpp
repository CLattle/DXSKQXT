// 文件名   : record_c_api.cpp
// 模块作者 : 乙（集成适配，供丙的 main 调用）
// 模块功能 : 把甲的 record.h / record.cpp（C++ 接口）和乙的 sort_report.h /
//            sort_report.cpp 包装成一组 C 风格函数，供丙的 1-9整合.cpp 使用。
//
//            丙的主程序是纯 C 写法（printf / char 缓冲），而数据层和报表层都用了
//            std::string / std::vector，没法在 C 里直接包含。所以这里：
//              · 用 extern "C" 导出 C 链接的名字；
//              · 把 Record 的字段拆成 char 缓冲 + int 输出；
//              · 把甲的错误码翻译成丙在头文件注释里约定的 0~5 正数。
//
// 编码约定 : 程序用 /utf-8 编译，所有中文字面量都是 UTF-8；这里在启动时把控制台
//            输入/输出代码页切到 UTF-8，避免中文乱码（本文件的静态对象在 main 之前执行）。

#include "record.h"
#include "sort_report.h"

#include <windows.h>

#include <cstring>
#include <string>

namespace {

// 控制台代码页初始化：静态对象，在 main 之前构造
struct ConsoleCodePage {
    ConsoleCodePage()
    {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
};

ConsoleCodePage g_consoleCodePage;

// 甲的错误码（0 / -1 ~ -5）→ 丙约定的返回码（0 / 1 ~ 5）
int toMainResult(int code)
{
    switch (code) {
    case OK:           return 0;   // 成功
    case ERR_INVALID:  return 1;   // 字段不合法
    case ERR_DUP:      return 2;   // 学号 + 日期重复
    case ERR_FULL:     return 3;   // 表已满
    case ERR_NOTFOUND: return 4;   // 记录不存在
    case ERR_FILE:     return 5;   // 文件打不开
    default:           return 1;
    }
}

// 把 std::string 拷进定长缓冲，保证以 '\0' 结尾
void copyText(char *destination, int size, const std::string &text)
{
    if (destination == nullptr || size <= 0) {
        return;
    }
    const std::size_t maxLength = static_cast<std::size_t>(size - 1);
    const std::size_t length = text.size() < maxLength ? text.size() : maxLength;
    std::memcpy(destination, text.data(), length);
    destination[length] = '\0';
}

// 把丙传入的 C 字符串安全地转成 std::string
std::string toText(const char *text)
{
    return text == nullptr ? std::string() : std::string(text);
}

}  // namespace

extern "C" {

// ------------------------- 数据层包装（甲） -------------------------

// 当前记录条数
int record_count(void)
{
    return recordCount;
}

// 取第 index 条的字段；成功返回 0，下标越界返回 4
int record_get(int index, char *id, int idSize,
               char *name, int nameSize, char *cls, int clsSize,
               int *month, int *day, int *status)
{
    if (index < 0 || index >= recordCount) {
        return 4;
    }
    const Record &record = records[index];
    copyText(id, idSize, record.id);
    copyText(name, nameSize, record.name);
    copyText(cls, clsSize, record.cls);
    if (month != nullptr) {
        *month = record.month;
    }
    if (day != nullptr) {
        *day = record.day;
    }
    if (status != nullptr) {
        *status = record.status;
    }
    return 0;
}

// 新增：成功 0；1 非法、2 重复、3 已满
int record_add(const char *id, const char *name, const char *cls,
               int month, int day, int status, char *err, int errSize)
{
    Record record;
    record.id = toText(id);
    record.name = toText(name);
    record.cls = toText(cls);
    record.month = month;
    record.day = day;
    record.status = status;

    std::string message;
    const int code = addRecord(record, message);
    copyText(err, errSize, message);
    return toMainResult(code);
}

// 修改：成功 0；4 不存在、1 非法、2 重复
int record_update(int index, const char *id, const char *name, const char *cls,
                  int month, int day, int status, char *err, int errSize)
{
    Record record;
    record.id = toText(id);
    record.name = toText(name);
    record.cls = toText(cls);
    record.month = month;
    record.day = day;
    record.status = status;

    std::string message;
    const int code = updateRecord(index, record, message);
    copyText(err, errSize, message);
    return toMainResult(code);
}

// 删除：成功 0；4 不存在
int record_delete(int index)
{
    return toMainResult(deleteRecord(index));
}

// 按学号 + 日期定位，返回下标，找不到返回 -1
int record_find(const char *id, int month, int day)
{
    return findRecord(toText(id), month, day);
}

// 从文本文件读入：成功 0；5 文件错误
int record_load(const char *path)
{
    return toMainResult(loadData(path));
}

// 保存到文本文件：成功 0；5 文件错误
int record_save(const char *path)
{
    return toMainResult(saveData(path));
}

// 排序：0 学号、1 姓名、2 日期（与丙的 SORT_BY_* 一致）
void record_sort(int key)
{
    SortKey sortKey = SORT_BY_ID;
    if (key == SORT_BY_NAME) {
        sortKey = SORT_BY_NAME;
    } else if (key == SORT_BY_DATE) {
        sortKey = SORT_BY_DATE;
    }
    sortRecords(records, recordCount, sortKey);
}

// ------------------------- 报表层包装（乙） -------------------------

// 五张报表：直接对全局表输出到控制台
void report_detail(void)
{
    reportDetail(records, recordCount);
}

void report_daily(void)
{
    reportDaily(records, recordCount);
}

void report_summary(void)
{
    reportSummary(records, recordCount);
}

void report_abnormal(void)
{
    reportAbnormal(records, recordCount);
}

void report_leave(void)
{
    reportLeave(records, recordCount);
}

}  // extern "C"
