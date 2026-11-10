/* ============================================================================
 *  main.c —— 大学生考勤系统 · 主控程序（丙 负责）
 *
 *  功能：
 *    · 主菜单 1~9 循环，一次运行可反复操作；
 *    · 统一接收输入，非法输入给出提示并可重试，不会把程序搞崩；
 *    · 记录列表支持分页显示（可显示部分或全部）。
 *
 *  说明：本文件是 C 程序，而 record.cpp / sort_report.cpp 是 C++ 实现，
 *        两者的头文件差异用下面的 extern "C" 处理：
 *        · 用 C++ 编译器（cl record.cpp sort_report.cpp main.c）整体编译时，
 *          链接名会自动统一，不需要特殊处理；
 *        · 若用 C 方式单独编译 main.c，请保证它和 C++ 目标文件链接时
 *          头文件里的声明是 extern "C"（见下方 DSH_EXTERN_C 说明）。
 *
 *  编译（VS2022 开发者命令行，与甲、乙源码同目录）：
 *      cl /nologo /EHsc /utf-8 main.c record.cpp sort_report.cpp /Fe:attendance.exe
 *
 *  运行：attendance.exe（数据文件默认 attendance_data.txt，可用命令行参数指定）
 * ========================================================================== */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 /* --------------------------------------------------------------------------
  *  链接约定
  *
  *  record.h 和 sort_report.h 都用 C++ 写成（用到 std::string），且 record.h
  *  直接 using namespace std，无法在 C 里包含。因此这里重新声明本程序要用到的
  *  接口，并包在 extern "C" 中：C++ 编译器看到时按 C 链接，不会产生名字修饰不匹配。
  *
  *  这两个声明块与 record.h / sort_report.h 的签名必须保持一致。
  * -------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif
    /* ---- record.h：常量 ---- */
#define MAX_RECORDS 1000
/* 返回码（与 record.h 的 enum 一致） */
#define OK           0
#define ERR_INVALID  1
#define ERR_DUP      2
#define ERR_FULL     3
#define ERR_NOTFOUND 4
#define ERR_FILE     5
/* 状态码（与 record.h 的 enum 一致） */
#define PRESENT        0
#define ABSENT         1
#define PERSONAL_LEAVE 2
#define SICK_LEAVE     3
#define LATE           4
#define EARLY          5
#define STATUS_COUNT   6
/* ---- record.h：数据表与接口 ----
 * 注意：Record 含 std::string，C 无法直接构造，所以增删改操作统一用
 * "字段指针" 形式的包装函数（见 record_c_api.cpp），避免在 C 里拼 Record。 */
    int  record_count(void);                    /* 当前记录条数 */
    int  record_get(int index, char* id, int idSize,
        char* name, int nameSize, char* cls, int clsSize,
        int* month, int* day, int* status);
    /* 取第 index 条的字段，成功返回 OK */
    int  record_add(const char* id, const char* name, const char* cls,
        int month, int day, int status, char* err, int errSize);
    int  record_update(int index, const char* id, const char* name, const char* cls,
        int month, int day, int status, char* err, int errSize);
    int  record_delete(int index);
    int  record_find(const char* id, int month, int day);   /* 返回下标或 -1 */
    int  record_load(const char* path);
    int  record_save(const char* path);
    /* ---- sort_report.h：排序与报表 ---- */
#define SORT_BY_ID   0
#define SORT_BY_NAME 1
#define SORT_BY_DATE 2
    void report_detail(void);
    void report_daily(void);
    void report_summary(void);
    void report_abnormal(void);
    void report_leave(void);
#ifdef __cplusplus
} /* extern "C" */
#endif
/* 注意：record.h 里 sortRecords(Record*, int, SortKey) 需要 C++ 的 Record 数组，
 * 排序功能由下列包装函数在 C++ 侧完成，这里只声明。 */
#ifdef __cplusplus
extern "C" {
#endif
    void record_sort(int key);                  /* 按 SORT_BY_* 原地排序 */
#ifdef __cplusplus
}
#endif
/* ==========================================================================
 *  一、输入辅助：统一的、防御式的输入接收
 * ========================================================================== */
#define LINE_MAX_LEN 256
 /* 读一行到 buf，去掉行尾换行；成功返回 1。
  * 返回 0 表示 EOF 或读入失败（例如 stdin 已结束）。
  * 说明：用 fgets 而不是 scanf("%s")，避免输入过长时缓冲区溢出，
  *       也能正确处理「输入里有空格」的姓名/班级。 */
static int read_line(char* buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) {
        return 0;
    }
    size_t len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
    /* 行太长时把剩余部分丢弃，防止影响下一次输入 */
    if (len == (size_t)(size - 1) && buf[size - 2] != '\n') {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* 丢弃多余字符 */
        }
    }
    return 1;
}
/* 去掉首尾空白 */
static void trim(char* s)
{
    char* p = s;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        ++p;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}
/* 判断整串是否全为数字 */
static int all_digits(const char* s)
{
    if (*s == '\0') {
        return 0;
    }
    for (; *s != '\0'; ++s) {
        if (!isdigit((unsigned char)*s)) {
            return 0;
        }
    }
    return 1;
}
/* 读一个整数到 out。提示语 prompt 会先输出。
 *
 * 返回值：
 *   1 —— 成功，*out 已填写；
 *   0 —— 这次输入不合法（已打印提示），调用方应重新输入；
 *  -1 —— 输入流结束（EOF）或读入失败，调用方应退出，避免死循环。 */
static int read_int(const char* prompt, int lo, int hi, int* out)
{
    char buf[LINE_MAX_LEN];
    printf("%s", prompt);
    fflush(stdout);
    if (!read_line(buf, sizeof(buf))) {
        return -1;
    }
    trim(buf);
    if (!all_digits(buf)) {
        printf("  [!] 输入无效：请输入 %d~%d 之间的整数。\n", lo, hi);
        return 0;
    }
    long v = strtol(buf, NULL, 10);
    if (v < lo || v > hi) {
        printf("  [!] 输入无效：请输入 %d~%d 之间的整数。\n", lo, hi);
        return 0;
    }
    *out = (int)v;
    return 1;
}
/* 反复提示，直到读到 [lo, hi] 内的整数为止。
 * 成功返回 1；输入流结束返回 0（此时调用方应放弃当前操作或退出程序）。
 * 主菜单和各子菜单都用它取选项，这样输错一个字符不会导致后续选项全部错位。 */
static int read_choice(const char* prompt, int lo, int hi, int* out)
{
    for (;;) {
        int rc = read_int(prompt, lo, hi, out);
        if (rc == 1) {
            return 1;
        }
        if (rc == -1) {
            return 0;
        }
        /* rc == 0：已经打印过提示，继续询问 */
    }
}
/* 读一个非空字符串，允许含空格 */
static int read_text(const char* prompt, char* out, int size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!read_line(out, size)) {
        return 0;
    }
    trim(out);
    return 1;
}
/* 询问「是否继续」，返回 1 表示是。只接受 y/n（大小写均可），其它输入重新询问。 */
static int ask_yes_no(const char* prompt)
{
    char buf[LINE_MAX_LEN];
    for (;;) {
        printf("%s", prompt);
        fflush(stdout);
        if (!read_line(buf, sizeof(buf))) {
            return 0;
        }
        trim(buf);
        if (strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0) {
            return 1;
        }
        if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0) {
            return 0;
        }
        printf("  [!] 请输入 y 或 n。\n");
    }
}
/* ==========================================================================
 *  二、分页显示
 * ========================================================================== */
#define PAGE_SIZE 12
 /* 按页显示记录。start/stop 为显示的起止下标；每页页尾等待用户按键：
  *   回车 → 下一页；q → 提前结束；其它 → 同样视为下一页。 */
static void show_page_range(int start, int stop)
{
    const char* symbols[STATUS_COUNT] = { "√", "X", "△", "○", "+", "–" };
    const char* names[STATUS_COUNT] = { "出勤", "旷课", "事假", "病假", "迟到", "早退" };
    if (start < 0) start = 0;
    if (stop > record_count()) stop = record_count();
    if (start >= stop) {
        printf("  没有可显示的记录。\n");
        return;
    }
    int total = stop - start;
    int pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    int index = start;
    printf("\n  序号  学号          姓名        班级        日期    状态\n");
    printf("  ------------------------------------------------------------------\n");
    int shown = 0;
    int page = 1;
    for (; index < stop; ++index) {
        char id[32], name[64], cls[64];
        int month = 0, day = 0, status = 0;
        if (record_get(index, id, sizeof(id), name, sizeof(name), cls, sizeof(cls),
            &month, &day, &status) != OK) {
            continue;
        }
        const char* sym = (status >= 0 && status < STATUS_COUNT) ? symbols[status] : "?";
        const char* nm = (status >= 0 && status < STATUS_COUNT) ? names[status] : "未知";
        printf("  %4d  %-12s  %-10s  %-10s  %02d-%02d   %s %s\n",
            shown + 1, id, name, cls, month, day, nm, sym);
        ++shown;
        if (shown % PAGE_SIZE == 0 && index + 1 < stop) {
            printf("  ------------------------------------------------------------------\n");
            printf("  第 %d/%d 页已显示，回车继续，输入 q 退出：", page, pages);
            fflush(stdout);
            char buf[LINE_MAX_LEN];
            read_line(buf, sizeof(buf));
            trim(buf);
            if (strcmp(buf, "q") == 0 || strcmp(buf, "Q") == 0) {
                printf("  已提前结束显示。\n");
                return;
            }
            ++page;
            printf("\n  序号  学号          姓名        班级        日期    状态\n");
            printf("  ------------------------------------------------------------------\n");
        }
    }
    printf("  ------------------------------------------------------------------\n");
    printf("  共显示 %d 条记录（第 %d 条~第 %d 条，每页 %d 条）。\n",
        shown, start + 1, start + shown, PAGE_SIZE);
}
/* 显示全部记录（分页） */
static void show_all(void)
{
    show_page_range(0, record_count());
    if (record_count() == 0) {
        printf("  提示：当前没有记录，可先使用【1 新增记录】或【8 读取文件】。\n");
    }
}
/* ==========================================================================
 *  三、各功能子菜单
 * ========================================================================== */
 /* 1 新增记录 */
static void menu_add(void)
{
    printf("\n=== 1 新增考勤记录 ===\n");
    char id[32], name[64], cls[64];
    if (!read_text("  学号（6~12 位数字）：", id, sizeof(id))) return;
    if (!read_text("  姓名（不超过 20 字节）：", name, sizeof(name))) return;
    if (!read_text("  班级（不超过 30 字节）：", cls, sizeof(cls))) return;
    int month = 0, day = 0, status = 0;
    if (!read_choice("  月份（1~12）：", 1, 12, &month)) return;
    if (!read_choice("  日期（1~31，按月份自动校验）：", 1, 31, &day)) return;
    printf("  状态码：0出勤 1旷课 2事假 3病假 4迟到 5早退\n");
    if (!read_choice("  状态码（0~5）：", 0, STATUS_COUNT - 1, &status)) return;
    char err[256] = { 0 };
    int rc = record_add(id, name, cls, month, day, status, err, sizeof(err));
    if (rc == OK) {
        printf("  新增成功！当前共 %d 条记录。\n", record_count());
    }
    else {
        printf("  新增失败：%s\n", err[0] ? err : "未知错误");
    }
}
/* 2 查找记录（按学号 / 姓名 / 日期） */
static void menu_find(void)
{
    printf("\n=== 2 查找考勤记录 ===\n");
    printf("  1) 按学号查找\n  2) 按姓名查找\n  3) 按日期查找\n  0) 返回上级菜单\n");
    int choice = 0;
    if (!read_choice("  请选择（0~3）：", 0, 3, &choice)) return;
    if (choice == 0) return;
    if (choice == 1) {
        char id[32];
        if (!read_text("  请输入学号：", id, sizeof(id))) return;
        int found = 0;
        int n = record_count();
        for (int i = 0; i < n; ++i) {
            char rid[32], rname[64], rcls[64];
            int m = 0, d = 0, st = 0;
            record_get(i, rid, sizeof(rid), rname, sizeof(rname), rcls, sizeof(rcls), &m, &d, &st);
            if (strcmp(rid, id) == 0) {
                if (found == 0) printf("\n  找到以下记录：\n");
                ++found;
                printf("  第 %d 条：%s %s %s %02d-%02d 状态码 %d\n", i + 1, rid, rname, rcls, m, d, st);
            }
        }
        if (found == 0) printf("  未找到学号为 %s 的记录。\n", id);
        else printf("  共 %d 条。\n", found);
    }
    else if (choice == 2) {
        char name[64];
        if (!read_text("  请输入姓名：", name, sizeof(name))) return;
        int found = 0;
        int n = record_count();
        for (int i = 0; i < n; ++i) {
            char rid[32], rname[64], rcls[64];
            int m = 0, d = 0, st = 0;
            record_get(i, rid, sizeof(rid), rname, sizeof(rname), rcls, sizeof(rcls), &m, &d, &st);
            if (strcmp(rname, name) == 0) {
                if (found == 0) printf("\n  找到以下记录：\n");
                ++found;
                printf("  第 %d 条：%s %s %s %02d-%02d 状态码 %d\n", i + 1, rid, rname, rcls, m, d, st);
            }
        }
        if (found == 0) printf("  未找到姓名为 %s 的记录。\n", name);
        else printf("  共 %d 条。\n", found);
    }
    else {
        int month = 0, day = 0;
        if (!read_choice("  月份（1~12）：", 1, 12, &month)) return;
        if (!read_choice("  日期（1~31）：", 1, 31, &day)) return;
        int found = 0;
        int n = record_count();
        for (int i = 0; i < n; ++i) {
            char rid[32], rname[64], rcls[64];
            int m = 0, d = 0, st = 0;
            record_get(i, rid, sizeof(rid), rname, sizeof(rname), rcls, sizeof(rcls), &m, &d, &st);
            if (m == month && d == day) {
                if (found == 0) printf("\n  找到以下记录：\n");
                ++found;
                printf("  第 %d 条：%s %s %s %02d-%02d 状态码 %d\n", i + 1, rid, rname, rcls, m, d, st);
            }
        }
        if (found == 0) printf("  未找到 %02d-%02d 的记录。\n", month, day);
        else printf("  共 %d 条。\n", found);
    }
}
/* 3 修改记录：先按学号+日期定位，再逐项输入新值 */
static void menu_update(void)
{
    printf("\n=== 3 修改考勤记录 ===\n");
    if (record_count() == 0) {
        printf("  当前没有记录。\n");
        return;
    }
    char id[32];
    if (!read_text("  请输入要修改记录的学号：", id, sizeof(id))) return;
    int month = 0, day = 0;
    if (!read_choice("  原记录月份（1~12）：", 1, 12, &month)) return;
    if (!read_choice("  原记录日期（1~31）：", 1, 31, &day)) return;
    int index = record_find(id, month, day);
    if (index < 0) {
        printf("  未找到该学号在该日期的记录。\n");
        return;
    }
    char rid[32], rname[64], rcls[64];
    int rm = 0, rd = 0, rst = 0;
    record_get(index, rid, sizeof(rid), rname, sizeof(rname), rcls, sizeof(rcls), &rm, &rd, &rst);
    printf("  当前内容：%s %s %s %02d-%02d 状态码 %d\n", rid, rname, rcls, rm, rd, rst);
    printf("  直接回车表示保留原值。\n");
    char input[LINE_MAX_LEN];
    char newid[32], newname[64], newcls[64];
    snprintf(newid, sizeof(newid), "%s", rid);
    snprintf(newname, sizeof(newname), "%s", rname);
    snprintf(newcls, sizeof(newcls), "%s", rcls);
    read_text("  学号（回车保留）：", input, sizeof(input));
    if (input[0] != '\0') snprintf(newid, sizeof(newid), "%s", input);
    read_text("  姓名（回车保留）：", input, sizeof(input));
    if (input[0] != '\0') snprintf(newname, sizeof(newname), "%s", input);
    read_text("  班级（回车保留）：", input, sizeof(input));
    if (input[0] != '\0') snprintf(newcls, sizeof(newcls), "%s", input);
    int newm = rm, newd = rd, newst = rst;
    read_text("  月份（回车保留）：", input, sizeof(input));
    if (input[0] != '\0') {
        if (all_digits(input) && atoi(input) >= 1 && atoi(input) <= 12) newm = atoi(input);
        else printf("  [!] 月份无效，保留原值 %d。\n", rm);
    }
    read_text("  日期（回车保留）：", input, sizeof(input));
    if (input[0] != '\0') {
        if (all_digits(input) && atoi(input) >= 1 && atoi(input) <= 31) newd = atoi(input);
        else printf("  [!] 日期无效，保留原值 %d。\n", rd);
    }
    printf("  状态码：0出勤 1旷课 2事假 3病假 4迟到 5早退\n");
    read_text("  状态码（回车保留）：", input, sizeof(input));
    if (input[0] != '\0') {
        if (all_digits(input) && atoi(input) >= 0 && atoi(input) < STATUS_COUNT) newst = atoi(input);
        else printf("  [!] 状态码无效，保留原值 %d。\n", rst);
    }
    char err[256] = { 0 };
    int rc = record_update(index, newid, newname, newcls, newm, newd, newst, err, sizeof(err));
    if (rc == OK) {
        printf("  修改成功。\n");
    }
    else {
        printf("  修改失败：%s\n", err[0] ? err : "未知错误");
    }
}
/* 4 删除记录 */
static void menu_delete(void)
{
    printf("\n=== 4 删除考勤记录 ===\n");
    if (record_count() == 0) {
        printf("  当前没有记录。\n");
        return;
    }
    char id[32];
    if (!read_text("  请输入要删除记录的学号：", id, sizeof(id))) return;
    int month = 0, day = 0;
    if (!read_choice("  记录月份（1~12）：", 1, 12, &month)) return;
    if (!read_choice("  记录日期（1~31）：", 1, 31, &day)) return;
    int index = record_find(id, month, day);
    if (index < 0) {
        printf("  未找到该记录。\n");
        return;
    }
    if (!ask_yes_no("  确认删除该记录？(y/n)：")) {
        printf("  已取消。\n");
        return;
    }
    int rc = record_delete(index);
    if (rc == OK) printf("  删除成功，当前共 %d 条记录。\n", record_count());
    else printf("  删除失败。\n");
}
/* 5 显示列表（按排序方式） */
static void menu_list(void)
{
    printf("\n=== 5 显示考勤记录列表 ===\n");
    printf("  排序方式：\n");
    printf("  1) 按学号（学号→日期→姓名）\n");
    printf("  2) 按姓名（姓名→学号→日期）\n");
    printf("  3) 按日期（日期→学号）\n");
    printf("  0) 不排序，按录入顺序\n");
    int choice = 0;
    if (!read_choice("  请选择（0~3）：", 0, 3, &choice)) return;
    if (choice == 1) record_sort(SORT_BY_ID);
    else if (choice == 2) record_sort(SORT_BY_NAME);
    else if (choice == 3) record_sort(SORT_BY_DATE);
    show_all();
}
/* 6 统计报表 */
static void menu_stat(void)
{
    printf("\n=== 6 数据统计（报表） ===\n");
    printf("  1) 考勤明细表\n");
    printf("  2) 考勤日报表\n");
    printf("  3) 考勤汇总表\n");
    printf("  4) 考勤异常表\n");
    printf("  5) 请假汇总表\n");
    printf("  6) 全部显示\n");
    printf("  0) 返回上级菜单\n");
    int choice = 0;
    if (!read_choice("  请选择（0~6）：", 0, 6, &choice)) return;
    switch (choice) {
    case 1: report_detail(); break;
    case 2: report_daily(); break;
    case 3: report_summary(); break;
    case 4: report_abnormal(); break;
    case 5: report_leave(); break;
    case 6:
        report_detail();
        report_daily();
        report_summary();
        report_abnormal();
        report_leave();
        break;
    default: break;
    }
}
/* 7 保存数据 */
static void menu_save(const char* path)
{
    printf("\n=== 7 保存数据到文件 ===\n");
    int rc = record_save(path);
    if (rc == OK) printf("  已保存 %d 条记录到 %s\n", record_count(), path);
    else printf("  保存失败：无法写入 %s\n", path);
}
/* 8 从文件读入 */
static void menu_load(const char* path)
{
    printf("\n=== 8 从文件读入数据 ===\n");
    int rc = record_load(path);
    if (rc == OK) printf("  读取成功，当前共 %d 条记录。\n", record_count());
    else if (rc == ERR_FILE) printf("  读取失败：找不到文件 %s\n", path);
    else printf("  读取失败（错误码 %d）。\n", rc);
}
/* 9 退出 */
static void save_before_exit(const char* path)
{
    printf("\n=== 9 退出系统 ===\n");
    if (record_count() > 0) {
        if (ask_yes_no("  退出前要先保存到数据文件吗？(y/n)：")) {
            int rc = record_save(path);
            if (rc == OK) printf("  已保存 %d 条记录到 %s\n", record_count(), path);
            else printf("  保存失败：无法写入 %s\n", path);
        }
    }
    printf("  感谢使用大学生考勤系统，再见！\n");
}
/* ==========================================================================
 *  四、主菜单
 * ========================================================================== */
static void print_menu(void)
{
    printf("\n");
    printf("  ==================================================================\n");
    printf("                    大 学 生 考 勤 系 统\n");
    printf("  ==================================================================\n");
    printf("     1. 新增考勤记录          2. 查找考勤记录\n");
    printf("     3. 修改考勤记录          4. 删除考勤记录\n");
    printf("     5. 显示记录列表          6. 统计报表\n");
    printf("     7. 保存数据到文件        8. 从文件读入数据\n");
    printf("     9. 退出系统\n");
    printf("  ==================================================================\n");
    printf("  当前记录数：%d 条\n", record_count());
}
int main(int argc, char** argv)
{
    /* 数据文件路径：命令行参数优先，否则用默认文件名 */
    const char* path = (argc >= 2) ? argv[1] : "attendance_data.txt";
#ifdef _WIN32
    /* 控制台中文乱码处理：把代码页切到 UTF-8（与源码编码一致） */
    system("chcp 65001 > nul");
#endif
    /* 启动时尝试自动读入数据文件；文件不存在属于正常情况，不报错 */
    if (record_load(path) != OK) {
        printf("  提示：未找到数据文件 %s，可作为新系统开始使用。\n", path);
    }
    else {
        printf("  已从 %s 读入 %d 条记录。\n", path, record_count());
    }
    for (;;) {
        print_menu();
        int choice = 0;
        if (!read_choice("  请选择功能（1~9）：", 1, 9, &choice)) {
            /* read_choice 只在输入流结束时返回 0；非法输入会在这里被重新询问 */
            printf("\n  输入已结束，程序退出。\n");
            return 0;
        }
        switch (choice) {
        case 1: menu_add(); break;
        case 2: menu_find(); break;
        case 3: menu_update(); break;
        case 4: menu_delete(); break;
        case 5: menu_list(); break;
        case 6: menu_stat(); break;
        case 7: menu_save(path); break;
        case 8: menu_load(path); break;
        case 9:
            save_before_exit(path);
            return 0;
        default:
            printf("  [!] 无效选项，请输入 1~9。\n");
            break;
        }
    }
}