/* ============================================================
 *  AttendanceMFC.cpp —— 大学生考勤系统 · MFC 图形界面（丙）
 *
 *  三层结构（与原控制台版完全一致）：
 *      甲 record.cpp        数据层：校验、增删改查、文件读写
 *      乙 sort_report.cpp   统计层：三种排序 + 五张报表
 *      丙 本文件            界面层：串联功能、接收输入、列表分页显示
 *
 *  本文件只做“界面壳”：
 *      控件文本 <-> std::string 的转换（CStringToUtf8 / Utf8ToCString）
 *      按钮消息处理（ON_BN_CLICKED）
 *  所有业务逻辑都在 attendance_core.cpp（书内层）里，由本文件转发调用。
 * ============================================================ */

#include "resource.h"

/* 甲、乙的接口（一行未改） */
#include "attendance_core.h"

#include <afxwin.h>
#include <afxcmn.h>

#include <string>

/* 现代风格的列表控件外观 */
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

/* 对话记录列表的列数（唯一的多余结构：用来解耦界面和数据）
 * 甲没有“每页 12 条”这样的需求，所以由顶层持有下标数组 */
const int kMaxRows = 1000;

using namespace std;

/* ============================================================
 *  一、字符串转换（MFC 的 CString <-> 甲的 std::string）
 *
 *  甲的 Record 字段是 std::string（UTF-8 编码），
 *  MFC 控件只认 CString（宽字符），所以跨界处必须转换。
 * ============================================================ */

static CString Utf8ToCString(const string& s)
{
    if (s.empty()) {
        return CString();
    }

    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                        static_cast<int>(s.size()), NULL, 0);
    if (len <= 0) {
        return CString();
    }

    CString str;
    LPTSTR buf = str.GetBuffer(len);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), buf, len);
    str.ReleaseBuffer(len);
    return str;
}

static string CStringToUtf8(const CString& s)
{
    if (s.IsEmpty()) {
        return string();
    }

    const int len = WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(),
                                        NULL, 0, NULL, NULL);
    if (len <= 0) {
        return string();
    }

    string out;
    out.resize(static_cast<size_t>(len));
    WideCharToMultiByte(CP_UTF8, 0, s, s.GetLength(),
                        &out[0], len, NULL, NULL);
    return out;
}

/* 去掉首尾空白（代替原丙的 trim，MFC 里用 CString::Trim） */
static string ToTrimmedString(const CString& s)
{
    CString tmp = s;
    tmp.Trim();
    return CStringToUtf8(tmp);
}

/* 严格解析整数：整串都是数字且在 [low, high] 内。
 * 与原丙的 parseRange 等价（此处借助 CString::SpanIncluding 判断数字）。 */
static bool ParseRange(const CString& text, int low, int high, int& value)
{
    CString s = text;
    s.Trim();
    if (s.IsEmpty()) {
        return false;
    }

    const int len = s.GetLength();
    for (int i = 0; i < len; ++i) {
        if (s[i] < _T('0') || s[i] > _T('9')) {
            return false;
        }
    }

    long long result = 0;
    for (int i = 0; i < len; ++i) {
        result = result * 10 + (s[i] - _T('0'));
        if (result > high) {
            return false;
        }
    }
    if (result < low) {
        return false;
    }
    value = static_cast<int>(result);
    return true;
}

/* ============================================================
 *  二、对话框类
 * ============================================================ */

class CAttendanceDlg : public CDialog
{
public:
    explicit CAttendanceDlg(CWnd* pParent = NULL);

    enum { IDD = IDD_ATTENDANCE_DIALOG };

protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();                       // 空实现：回车不关窗口

    afx_msg void OnAdd();
    afx_msg void OnUpdateRecord();
    afx_msg void OnDeleteRecord();
    afx_msg void OnClear();
    afx_msg void OnFind();
    afx_msg void OnShowAll();
    afx_msg void OnPrevPage();
    afx_msg void OnNextPage();
    afx_msg void OnSortChanged();
    afx_msg void OnReport();
    afx_msg void OnSave();
    afx_msg void OnLoad();
    afx_msg void OnListClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnExit();

    DECLARE_MESSAGE_MAP()

private:
    /* ---- 界面辅助 ---- */
    void InitControls();                       // 初始化列、下拉框
    void RefreshList();                        // 按 m_matches + m_page 刷新表格
    void RebuildMatches(bool sortFirst);       // 重新收集要显示的下标
    void LoadRowToEditors(int row);            // 把某一行内容填回输入框
    int  GetSelectedRow() const;               // 选中行号，-1 表示没选
    int  GetSelectedRecordIndex(int row) const;// 选中行对应的甲的下标
    CString GetEditText(int id) const;
    int  GetComboSel(int id) const;
    /* 注意：ShowError 不能加 const —— 它内部要调 MessageBox，
     * 而 CWnd::MessageBox 是非 const 成员函数（会修改窗口状态），
     * 在常成员函数里调用会报 C2662。这正是教材 4.4.2 常成员函数的约束。 */
    void ShowError(const string& reason);

    /* ---- 数据 ---- */
    AttendanceCore m_core;                     // 业务核心（书内层）
    int  m_matches[kMaxRows];                  // 当前显示的下标集合
    int  m_matchCount;                         // 集合大小
    int  m_page;                               // 当前页码（从 0 开始）
};

/* 构造函数：初始化列表（3.4.4）
 * 数据文件路径：本工程在仓库的 MFC 子目录里，data.txt 在上一级（仓库根目录），
 * 用相对路径 ../data.txt。从 VS 按 F5 运行时，工作目录是项目目录，
 * 所以能正确找到 DXSKQXT1\data.txt。 */
CAttendanceDlg::CAttendanceDlg(CWnd* pParent)
    : CDialog(IDD_ATTENDANCE_DIALOG, pParent),
      m_core("../data.txt"),
      m_matchCount(0),
      m_page(0)
{
}

/* 消息映射：按钮编号 -> 处理函数（对应原丙 switch(choice) 的分发） */
BEGIN_MESSAGE_MAP(CAttendanceDlg, CDialog)
    ON_BN_CLICKED(IDC_BTN_ADD,     &CAttendanceDlg::OnAdd)
    ON_BN_CLICKED(IDC_BTN_UPDATE,  &CAttendanceDlg::OnUpdateRecord)
    ON_BN_CLICKED(IDC_BTN_DELETE,  &CAttendanceDlg::OnDeleteRecord)
    ON_BN_CLICKED(IDC_BTN_CLEAR,   &CAttendanceDlg::OnClear)
    ON_BN_CLICKED(IDC_BTN_FIND,    &CAttendanceDlg::OnFind)
    ON_BN_CLICKED(IDC_BTN_SHOWALL, &CAttendanceDlg::OnShowAll)
    ON_BN_CLICKED(IDC_BTN_PREV,    &CAttendanceDlg::OnPrevPage)
    ON_BN_CLICKED(IDC_BTN_NEXT,    &CAttendanceDlg::OnNextPage)
    ON_BN_CLICKED(IDC_BTN_REPORT,  &CAttendanceDlg::OnReport)
    ON_BN_CLICKED(IDC_BTN_SAVE,    &CAttendanceDlg::OnSave)
    ON_BN_CLICKED(IDC_BTN_LOAD,    &CAttendanceDlg::OnLoad)
    ON_BN_CLICKED(IDC_BTN_EXIT,    &CAttendanceDlg::OnExit)
    ON_CBN_SELCHANGE(IDC_COMBO_SORT, &CAttendanceDlg::OnSortChanged)
    ON_NOTIFY(NM_CLICK, IDC_LIST,  &CAttendanceDlg::OnListClick)
END_MESSAGE_MAP()

/* ============================================================
 *  三、初始化
 * ============================================================ */

BOOL CAttendanceDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    InitControls();

    /* 启动时自动读文件（对应原丙 run() 开头的 loadData） */
    if (!m_core.load()) {
        SetDlgItemText(IDC_STATIC_COUNT,
                       _T(" 未找到数据文件 data.txt，可作为新系统开始使用"));
    } else {
        CString info;
        info.Format(_T(" 已从 %s 读入 %d 条记录"),
                    Utf8ToCString(m_core.getPath()),
                    m_core.recordSize());
        SetDlgItemText(IDC_STATIC_COUNT, info);
    }

    RebuildMatches(false);
    return TRUE;
}

void CAttendanceDlg::InitControls()
{
    /* 列表列：对应原丙 printHeader 的列顺序 */
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST);
    pList->SetExtendedStyle(pList->GetExtendedStyle()
                            | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    pList->InsertColumn(0, _T("序号"), LVCFMT_RIGHT, 45);
    pList->InsertColumn(1, _T("学号"), LVCFMT_LEFT, 100);
    pList->InsertColumn(2, _T("姓名"), LVCFMT_LEFT, 90);
    pList->InsertColumn(3, _T("班级"), LVCFMT_LEFT, 150);
    pList->InsertColumn(4, _T("日期"), LVCFMT_LEFT, 80);
    pList->InsertColumn(5, _T("状态"), LVCFMT_LEFT, 100);

    /* 状态下拉框：0~5，文字用甲的状态名 */
    CComboBox* pStatus = (CComboBox*)GetDlgItem(IDC_COMBO_STATUS);
    for (int s = 0; s < STATUS_COUNT; ++s) {
        pStatus->AddString(Utf8ToCString(m_core.statusText(s)));
    }
    pStatus->SetCurSel(0);

    /* 查找方式：对应原丙 menuFind 的三个分支 */
    CComboBox* pFind = (CComboBox*)GetDlgItem(IDC_COMBO_FIND);
    pFind->AddString(_T("按学号"));
    pFind->AddString(_T("按姓名"));
    pFind->AddString(_T("按日期"));
    pFind->SetCurSel(0);

    /* 排序方式：对应原丙 menuList 的四种选择 */
    CComboBox* pSort = (CComboBox*)GetDlgItem(IDC_COMBO_SORT);
    pSort->AddString(_T("按学号排序"));
    pSort->AddString(_T("按姓名排序"));
    pSort->AddString(_T("按日期排序"));
    pSort->AddString(_T("不排序"));
    pSort->SetCurSel(0);

    /* 报表种类：对应原丙 menuStat 的六项 */
    CComboBox* pRep = (CComboBox*)GetDlgItem(IDC_COMBO_REPORT);
    pRep->AddString(_T("考勤明细表"));
    pRep->AddString(_T("考勤日报表"));
    pRep->AddString(_T("考勤汇总表"));
    pRep->AddString(_T("考勤异常表"));
    pRep->AddString(_T("请假汇总表"));
    pRep->AddString(_T("全部显示"));
    pRep->SetCurSel(0);

    GetDlgItem(IDC_EDIT_ID)->SetFocus();
}

void CAttendanceDlg::OnOK()
{
    /* 空实现：按回车不要关闭对话框 */
}

/* ============================================================
 *  四、列表刷新与分页
 * ============================================================ */

/* 重新收集“要显示哪些记录”的下标（对应原丙的 m_matches / showPage 参数） */
void CAttendanceDlg::RebuildMatches(bool sortFirst)
{
    if (sortFirst) {
        const int sel = GetComboSel(IDC_COMBO_SORT);
        if (sel == 0) {
            m_core.sort(SORT_BY_ID);           // 乙
        } else if (sel == 1) {
            m_core.sort(SORT_BY_NAME);         // 乙
        } else if (sel == 2) {
            m_core.sort(SORT_BY_DATE);         // 乙
        }
        /* sel == 3：不排序 */
    }

    m_matchCount = m_core.collectAll(m_matches, kMaxRows);
    m_page = 0;
    RefreshList();
}

/* 只刷新当前页（对应原丙 showPage 的 for 循环 + 换页逻辑） */
void CAttendanceDlg::RefreshList()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST);
    pList->DeleteAllItems();

    const int total = m_matchCount;
    const int pages = m_core.pageCount(total);

    /* 页码越界修正 */
    if (m_page >= pages) {
        m_page = pages - 1;
    }
    if (m_page < 0) {
        m_page = 0;
    }

    const int start = m_core.pageStart(m_page);
    const int stop = m_core.pageStop(m_page, total);

    for (int i = start, row = 0; i < stop; ++i, ++row) {
        const int index = m_matches[i];
        const Record& r = m_core.recordAt(index);

        /* 第 0 列：序号（全局序号，不是页内序号） */
        CString seq;
        seq.Format(_T("%d"), i + 1);
        pList->InsertItem(row, seq);

        pList->SetItemText(row, 1, Utf8ToCString(r.id));
        pList->SetItemText(row, 2, Utf8ToCString(r.name));
        pList->SetItemText(row, 3, Utf8ToCString(r.cls));
        pList->SetItemText(row, 4, Utf8ToCString(m_core.formatDate(r.month, r.day)));
        pList->SetItemText(row, 5, Utf8ToCString(m_core.statusText(r.status)));

        /* 把“甲的下标”藏在每行的数据里，修改/删除时取用 */
        pList->SetItemData(row, static_cast<DWORD_PTR>(index));
    }

    /* 页码与按钮可用性 */
    CString page;
    page.Format(_T("%d/%d"), pages ? m_page + 1 : 0, pages);
    SetDlgItemText(IDC_STATIC_PAGE, page);
    GetDlgItem(IDC_BTN_PREV)->EnableWindow(m_page > 0);
    GetDlgItem(IDC_BTN_NEXT)->EnableWindow(m_page + 1 < pages);

    CString info;
    info.Format(_T(" 当前共 %d 条记录，本页显示第 %d~%d 条（每页 12 条）"),
                total,
                total ? start + 1 : 0,
                stop);
    SetDlgItemText(IDC_STATIC_COUNT, info);
}

void CAttendanceDlg::OnPrevPage()
{
    --m_page;
    RefreshList();
}

void CAttendanceDlg::OnNextPage()
{
    ++m_page;
    RefreshList();
}

void CAttendanceDlg::OnSortChanged()
{
    RebuildMatches(true);                      // 排序后重新收集下标
}

/* ============================================================
 *  五、选中行与输入框的联动
 * ============================================================ */

int CAttendanceDlg::GetSelectedRow() const
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST);
    POSITION pos = pList->GetFirstSelectedItemPosition();
    if (pos == NULL) {
        return -1;
    }
    return pList->GetNextSelectedItem(pos);
}

int CAttendanceDlg::GetSelectedRecordIndex(int row) const
{
    if (row < 0) {
        return -1;
    }
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST);
    return static_cast<int>(pList->GetItemData(row));
}

/* 点击某一行：把内容回填到输入框，方便直接修改 */
void CAttendanceDlg::OnListClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMITEMACTIVATE* item = (NMITEMACTIVATE*)pNMHDR;
    if (item->iItem >= 0) {
        LoadRowToEditors(item->iItem);
    }
    if (pResult != NULL) {
        *pResult = 0;
    }
}

void CAttendanceDlg::LoadRowToEditors(int row)
{
    const int index = GetSelectedRecordIndex(row);
    if (index < 0) {
        return;
    }

    const Record& r = m_core.recordAt(index);
    SetDlgItemText(IDC_EDIT_ID, Utf8ToCString(r.id));
    SetDlgItemText(IDC_EDIT_NAME, Utf8ToCString(r.name));
    SetDlgItemText(IDC_EDIT_CLASS, Utf8ToCString(r.cls));

    CString month, day;
    month.Format(_T("%d"), r.month);
    day.Format(_T("%d"), r.day);
    SetDlgItemText(IDC_EDIT_MONTH, month);
    SetDlgItemText(IDC_EDIT_DAY, day);

    ((CComboBox*)GetDlgItem(IDC_COMBO_STATUS))->SetCurSel(r.status);
}

CString CAttendanceDlg::GetEditText(int id) const
{
    CString text;
    GetDlgItemText(id, text);
    text.Trim();
    return text;
}

int CAttendanceDlg::GetComboSel(int id) const
{
    return ((CComboBox*)GetDlgItem(id))->GetCurSel();
}

void CAttendanceDlg::ShowError(const string& reason)
{
    CString msg = Utf8ToCString(reason.empty() ? string("未知错误") : reason);
    MessageBox(msg, _T("提示"), MB_ICONWARNING | MB_OK);
}

/* ============================================================
 *  六、增、删、改（对应原丙 menuAdd / menuDelete / menuUpdate）
 * ============================================================ */

void CAttendanceDlg::OnAdd()
{
    Record r;

    /* 文本字段：先做和甲一致的格式校验，再交给甲 */
    const int textFields[3] = { AttendanceCore::FIELD_ID,
                                AttendanceCore::FIELD_NAME,
                                AttendanceCore::FIELD_CLASS };
    const int editIds[3] = { IDC_EDIT_ID, IDC_EDIT_NAME, IDC_EDIT_CLASS };
    string texts[3];

    for (int i = 0; i < 3; ++i) {
        texts[i] = ToTrimmedString(GetEditText(editIds[i]));
        string reason;
        if (!m_core.checkText(textFields[i], texts[i], reason)) {
            ShowError(reason);
            GetDlgItem(editIds[i])->SetFocus();
            return;
        }
    }

    /* 数值字段 */
    int month = 0;
    int day = 0;
    if (!ParseRange(GetEditText(IDC_EDIT_MONTH), 1, 12, month)) {
        ShowError(m_core.getInvalidMessage(AttendanceCore::FIELD_MONTH));
        GetDlgItem(IDC_EDIT_MONTH)->SetFocus();
        return;
    }
    if (!ParseRange(GetEditText(IDC_EDIT_DAY), 1, 31, day)) {
        ShowError(m_core.getInvalidMessage(AttendanceCore::FIELD_DAY));
        GetDlgItem(IDC_EDIT_DAY)->SetFocus();
        return;
    }

    const int status = GetComboSel(IDC_COMBO_STATUS);
    if (status < 0) {
        ShowError(m_core.getInvalidMessage(AttendanceCore::FIELD_STATUS));
        return;
    }

    r.id = texts[0];
    r.name = texts[1];
    r.cls = texts[2];
    r.month = month;
    r.day = day;
    r.status = status;

    string reason;
    if (!m_core.add(r, reason)) {              // 甲：校验 + 查重 + 容量
        ShowError(reason);
        return;
    }

    CString info;
    info.Format(_T(" 新增成功！当前共 %d 条记录"), m_core.recordSize());
    SetDlgItemText(IDC_STATIC_COUNT, info);

    RebuildMatches(false);
    OnClear();
}

void CAttendanceDlg::OnUpdateRecord()
{
    const int row = GetSelectedRow();
    if (row < 0) {
        MessageBox(_T("请先在列表里选中要修改的那一行。"), _T("提示"),
                   MB_ICONWARNING | MB_OK);
        return;
    }

    const int index = GetSelectedRecordIndex(row);
    if (index < 0) {
        return;
    }

    Record r;
    r.id = ToTrimmedString(GetEditText(IDC_EDIT_ID));
    r.name = ToTrimmedString(GetEditText(IDC_EDIT_NAME));
    r.cls = ToTrimmedString(GetEditText(IDC_EDIT_CLASS));

    if (!ParseRange(GetEditText(IDC_EDIT_MONTH), 1, 12, r.month)) {
        ShowError(m_core.getInvalidMessage(AttendanceCore::FIELD_MONTH));
        return;
    }
    if (!ParseRange(GetEditText(IDC_EDIT_DAY), 1, 31, r.day)) {
        ShowError(m_core.getInvalidMessage(AttendanceCore::FIELD_DAY));
        return;
    }

    const int status = GetComboSel(IDC_COMBO_STATUS);
    if (status >= 0) {
        r.status = status;
    } else {
        r.status = m_core.recordAt(index).status;
    }

    string reason;
    if (!m_core.update(index, r, reason)) {     // 甲：含“改完是否与别人重复”检查
        ShowError(reason);
        return;
    }

    SetDlgItemText(IDC_STATIC_COUNT, _T(" 修改成功。"));
    RefreshList();
}

void CAttendanceDlg::OnDeleteRecord()
{
    const int row = GetSelectedRow();
    if (row < 0) {
        MessageBox(_T("请先在列表里选中要删除的那一行。"), _T("提示"),
                   MB_ICONWARNING | MB_OK);
        return;
    }

    const int index = GetSelectedRecordIndex(row);
    if (index < 0) {
        return;
    }

    const Record& r = m_core.recordAt(index);
    CString msg;
    msg.Format(_T("确定要删除这条记录吗？\n\n学号：%s\n姓名：%s\n日期：%s"),
               (LPCTSTR)Utf8ToCString(r.id),
               (LPCTSTR)Utf8ToCString(r.name),
               (LPCTSTR)Utf8ToCString(m_core.formatDate(r.month, r.day)));

    /* 二次确认（对应原丙的 askYesNo） */
    if (MessageBox(msg, _T("确认删除"), MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    if (m_core.remove(index)) {
        CString info;
        info.Format(_T(" 删除成功，当前共 %d 条记录"), m_core.recordSize());
        SetDlgItemText(IDC_STATIC_COUNT, info);

        /* 删除会移动后面的记录，所有下标都失效，必须重新收集 */
        RebuildMatches(false);
    } else {
        ShowError("删除失败");
    }
}

void CAttendanceDlg::OnClear()
{
    SetDlgItemText(IDC_EDIT_ID, _T(""));
    SetDlgItemText(IDC_EDIT_NAME, _T(""));
    SetDlgItemText(IDC_EDIT_CLASS, _T(""));
    SetDlgItemText(IDC_EDIT_MONTH, _T(""));
    SetDlgItemText(IDC_EDIT_DAY, _T(""));
    ((CComboBox*)GetDlgItem(IDC_COMBO_STATUS))->SetCurSel(0);
    GetDlgItem(IDC_EDIT_ID)->SetFocus();
}

/* ============================================================
 *  七、查找（对应原丙 menuFind，三种方式全部交给甲）
 * ============================================================ */

void CAttendanceDlg::OnFind()
{
    const CString key = GetEditText(IDC_EDIT_KEY);
    const int mode = GetComboSel(IDC_COMBO_FIND);
    const string keyUtf8 = CStringToUtf8(key);

    int n = 0;

    if (!key.IsEmpty()) {
        if (mode == 0) {
            n = m_core.findById(keyUtf8, m_matches, kMaxRows);      // 甲
        } else if (mode == 1) {
            n = m_core.findByName(keyUtf8, m_matches, kMaxRows);    // 甲
        } else {
            /* 按日期：要求输入“月”
             * 这里用学号以外的输入框承载日期，简化界面：
             * 用户在“查找”框里填月份，在“日”框里填日期 */
            int month = 0;
            int day = 0;
            if (!ParseRange(key, 1, 12, month)) {
                ShowError(m_core.getInvalidMessage(AttendanceCore::FIELD_MONTH));
                return;
            }
            if (!ParseRange(GetEditText(IDC_EDIT_DAY), 1, 31, day)) {
                ShowError("按日期查找时，请在【日】输入框里填写日期（1~31）。");
                return;
            }
            n = m_core.findByDate(month, day, m_matches, kMaxRows); // 甲
        }
    }

    if (!key.IsEmpty() && n == 0) {
        CString msg;
        msg.Format(_T(" 未找到\"%s\"对应的记录"), key);
        SetDlgItemText(IDC_STATIC_COUNT, msg);
        m_matchCount = 0;
        m_page = 0;
        RefreshList();
        return;
    }

    if (key.IsEmpty()) {
        /* 关键字为空：当作“显示全部” */
        RebuildMatches(false);
    } else {
        m_matchCount = n;
        m_page = 0;
        RefreshList();
    }

    CString info;
    info.Format(_T(" 找到 %d 条记录"), m_matchCount);
    SetDlgItemText(IDC_STATIC_COUNT, info);
}

void CAttendanceDlg::OnShowAll()
{
    SetDlgItemText(IDC_EDIT_KEY, _T(""));
    RebuildMatches(true);                      // 按当前排序方式重新显示全部
}

/* ============================================================
 *  八、报表（乙的结果抓成字符串，显示在多行只读编辑框里）
 * ============================================================ */

void CAttendanceDlg::OnReport()
{
    const int which = GetComboSel(IDC_COMBO_REPORT);
    if (which < 0) {
        return;
    }

    const string text = m_core.report(which);  // 内部用 cout.rdbuf 抓乙的输出
    SetDlgItemText(IDC_EDIT_REPORT, Utf8ToCString(text));
}

/* ============================================================
 *  九、文件、退出
 * ============================================================ */

void CAttendanceDlg::OnSave()
{
    if (m_core.save()) {
        CString info;
        info.Format(_T(" 已保存 %d 条记录到 %s"),
                    m_core.recordSize(), Utf8ToCString(m_core.getPath()));
        SetDlgItemText(IDC_STATIC_COUNT, info);
    } else {
        ShowError("保存失败：无法写入 " + m_core.getPath());
    }
}

void CAttendanceDlg::OnLoad()
{
    if (MessageBox(_T("从文件读取会覆盖当前内存中的记录，继续吗？"),
                   _T("确认"), MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    if (m_core.load()) {
        CString info;
        info.Format(_T(" 读取成功，当前共 %d 条记录"), m_core.recordSize());
        SetDlgItemText(IDC_STATIC_COUNT, info);
        RebuildMatches(false);
    } else {
        ShowError("读取失败：找不到文件 " + m_core.getPath());
    }
}

void CAttendanceDlg::OnExit()
{
    OnCancel();
}

/* ============================================================
 *  十、应用程序类
 * ============================================================ */

class CAttendanceApp : public CWinApp
{
public:
    virtual BOOL InitInstance();
};

CAttendanceApp theApp;

BOOL CAttendanceApp::InitInstance()
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    CAttendanceDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    return FALSE;
}
