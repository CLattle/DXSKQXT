/* ============================================================================
 *  record_test.cpp —— 数据层自测程序（甲）
 *
 *  不依赖菜单，单独运行，用来验证 record.cpp 的每条规则，
 *  输出可以直接截图放进课程设计报告的“测试用例与结果”一节。
 *
 *  编译：cl /nologo /EHsc /utf-8 record.cpp record_test.cpp /Fe:record_test.exe
 * ========================================================================== */

#include "record.h"

#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

static int g_tests = 0;     // 一共测了几项
static int g_pass = 0;      // 通过几项

/* 输出一项测试结果 */
static void check(const string &name, bool ok)
{
    ++g_tests;
    if (ok) {
        ++g_pass;
    }

    cout << "[";
    if (g_tests < 10) {
        cout << '0';
    }
    cout << g_tests << "] " << name << "：" << (ok ? "通过" : "失败") << "\n";
}

int main()
{
#ifdef _WIN32
    /* 控制台默认是 GBK（936），而本程序的字符串是 UTF-8，
     * 所以先把输出代码页切成 UTF-8，否则中文会乱码。
     * 用 API 静默切换，不会像 system("chcp") 那样多打印一行。 */
    SetConsoleOutputCP(CP_UTF8);
#endif

    cout << "============================================\n";
    cout << "  考勤系统 · 数据层自测程序（甲）\n";
    cout << "============================================\n\n";

    /* ---------------- 准备三条记录 ---------------- */
    Record r1;
    r1.id = "202301010101";
    r1.name = "张明";
    r1.cls = "人文2301";
    r1.month = 3;
    r1.day = 2;
    r1.status = PRESENT;

    Record r2 = r1;
    r2.day = 3;                     // 同一学生，换一天
    r2.status = LATE;

    Record r4;
    r4.id = "202301010102";
    r4.name = "李静";
    r4.cls = "人文2302";
    r4.month = 3;
    r4.day = 2;
    r4.status = ABSENT;

    string err;

    /* ---------------- 新增与查重 ---------------- */
    check("新增一条合法记录", addRecord(r1, err) == OK);
    check("记录数为 1", recordCount == 1);
    check("同一天重复录入被拒绝", addRecord(r1, err) == ERR_DUP);
    check("同学号换一天可以新增", addRecord(r2, err) == OK);
    check("新增第二名学生的记录", addRecord(r4, err) == OK);
    check("记录数为 3", recordCount == 3);

    /* ---------------- 非法数据 ---------------- */
    Record bad = r1;
    bad.id = "20230101A101";
    check("学号含字母被拒绝", addRecord(bad, err) == ERR_INVALID);

    bad = r1;
    bad.month = 2;
    bad.day = 30;
    check("2 月 30 日被拒绝", addRecord(bad, err) == ERR_INVALID);

    bad = r1;
    bad.day = 5;
    bad.status = 6;
    check("状态码 6 被拒绝", addRecord(bad, err) == ERR_INVALID);

    /* ---------------- 查找 ---------------- */
    int idx[100];

    check("按学号查到 2 条", findById("202301010101", idx, 100) == 2);
    check("按姓名查到 2 条", findByName("张明", idx, 100) == 2);
    check("按日期 3-2 查到 2 条", findByDate(3, 2, idx, 100) == 2);
    check("查不存在的学号返回 0 条", findById("999999", idx, 100) == 0);

    /* ---------------- 修改 ---------------- */
    Record edited = records[0];
    edited.status = SICK_LEAVE;
    check("修改状态码成功", updateRecord(0, edited, err) == OK);
    check("修改后状态是病假", records[0].status == SICK_LEAVE);

    Record dup = records[0];
    dup.day = 3;                    // 改成和第二条一样的主键
    check("改成重复的主键被拒绝", updateRecord(0, dup, err) == ERR_DUP);

    /* ---------------- 删除 ---------------- */
    check("删除第 2 条成功", deleteRecord(1) == OK);
    check("删除后记录数为 2", recordCount == 2);
    check("被删的记录已经查不到", findRecord("202301010101", 3, 3) == -1);

    /* ---------------- 文件 ---------------- */
    check("保存到文件成功", saveData("test_save.txt") == OK);

    recordCount = 0;                // 先清空，验证确实是从文件读回来的
    check("从文件读回 2 条", loadData("test_save.txt") == OK && recordCount == 2);
    remove("test_save.txt");

    /* ---------------- 载入样例数据 ---------------- */
    int sample = loadData("data.txt");
    cout << "\n载入样例 data.txt：";
    if (sample == OK) {
        cout << recordCount << " 条记录\n";
        for (int i = 0; i < 3 && i < recordCount; ++i) {
            cout << "  " << records[i].id << "  " << records[i].name
                 << "  " << records[i].cls << "  "
                 << records[i].month << "-" << records[i].day << "  "
                 << statusSymbol(records[i].status)
                 << statusName(records[i].status) << "\n";
        }
    } else {
        cout << "没有找到 data.txt（把程序和 data.txt 放在同一目录）\n";
    }

    /* ---------------- 汇总 ---------------- */
    cout << "\n============================================\n";
    cout << "  测试结果：" << g_pass << " 项通过 / 共 " << g_tests << " 项\n";
    cout << "============================================\n";

    return (g_pass == g_tests) ? 0 : 1;
}
