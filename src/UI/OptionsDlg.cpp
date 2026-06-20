#include "OptionsDlg.h"
#include "resource.h"
#include "../Core/ConfigManager.h"
#include <commctrl.h>

// 获取当前模块的实例句柄
extern HMODULE GetCurrentModuleHandle();

// 为了支持无 MFC 编译，我们需要在此获取 DLL 的实例句柄
static HMODULE g_hModule = nullptr;

// DllMain 中或通过 GetModuleHandleInstance 获得句柄。
// 这里通过 GetModuleHandleEx 动态获取自身 HMODULE。
HMODULE GetCurrentModuleHandle()
{
    if (g_hModule == nullptr)
    {
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&GetCurrentModuleHandle, &g_hModule);
    }
    return g_hModule;
}

OptionsDlg::OptionsDlg()
{
    // 获取当前配置，深拷贝至临时变量中
    m_temp_setting = ConfigManager::Instance().GetSetting();
}

OptionsDlg::~OptionsDlg()
{
}

int OptionsDlg::Show(HWND parent_hwnd)
{
    // 使用 DialogBoxParam 创建模态对话框
    INT_PTR ret = DialogBoxParamW(
        GetCurrentModuleHandle(),
        MAKEINTRESOURCEW(IDD_OPTIONS_DLG),
        parent_hwnd,
        DlgProc,
        reinterpret_cast<LPARAM>(this)
    );
    return static_cast<int>(ret);
}

void OptionsDlg::OnInitDialog(HWND hwnd)
{
    m_hwnd = hwnd;

    // 初始化按钮勾选状态
    CheckDlgButton(hwnd, IDC_CHECK_CPU_MON, m_temp_setting.cpu_mon_enable ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CHECK_GPU_MON, m_temp_setting.gpu_mon_enable ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CHECK_BATTERY_MON, m_temp_setting.battery_mon_enable ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hwnd, IDC_CHECK_SMART_MODE, m_temp_setting.smart_mode ? BST_CHECKED : BST_UNCHECKED);

    // 设置编辑框与 Spin 控件范围
    // 数字长度 0 - 3
    SendDlgItemMessageW(hwnd, IDC_SPIN_DIGIT_LENGTH, UDM_SETRANGE32, 0, 3);
    SetDlgItemInt(hwnd, IDC_EDIT_DIGIT_LENGTH, m_temp_setting.digit_length, FALSE);

    // 间距控制 0 - 5
    SendDlgItemMessageW(hwnd, IDC_SPIN_SPACING_CONTROL, UDM_SETRANGE32, 0, 5);
    SetDlgItemInt(hwnd, IDC_EDIT_SPACING_CONTROL, m_temp_setting.spacing_control, FALSE);

    // 单位字符串
    SetDlgItemTextW(hwnd, IDC_EDIT_UNIT_STRING, m_temp_setting.unit_string);
}

void OptionsDlg::OnOK(HWND hwnd)
{
    // 获取各开关状态
    m_temp_setting.cpu_mon_enable = (IsDlgButtonChecked(hwnd, IDC_CHECK_CPU_MON) == BST_CHECKED);
    m_temp_setting.gpu_mon_enable = (IsDlgButtonChecked(hwnd, IDC_CHECK_GPU_MON) == BST_CHECKED);
    m_temp_setting.battery_mon_enable = (IsDlgButtonChecked(hwnd, IDC_CHECK_BATTERY_MON) == BST_CHECKED);
    m_temp_setting.smart_mode = (IsDlgButtonChecked(hwnd, IDC_CHECK_SMART_MODE) == BST_CHECKED);

    // 获取数字长度与间距
    BOOL translated = FALSE;
    UINT val = GetDlgItemInt(hwnd, IDC_EDIT_DIGIT_LENGTH, &translated, FALSE);
    if (translated && val <= 3)
    {
        m_temp_setting.digit_length = val;
    }
    
    val = GetDlgItemInt(hwnd, IDC_EDIT_SPACING_CONTROL, &translated, FALSE);
    if (translated && val <= 5)
    {
        m_temp_setting.spacing_control = val;
    }

    // 获取单位字符串
    GetDlgItemTextW(hwnd, IDC_EDIT_UNIT_STRING, m_temp_setting.unit_string, 16);

    // 保存至 ConfigManager 缓存并持久化写入 .ini 文件
    ConfigManager::Instance().UpdateSetting(m_temp_setting);
}

INT_PTR CALLBACK OptionsDlg::DlgProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    // 获取 OptionsDlg 的实例指针
    OptionsDlg* pThis = nullptr;
    if (message == WM_INITDIALOG)
    {
        pThis = reinterpret_cast<OptionsDlg*>(lparam);
        SetWindowLongPtrW(hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));
        pThis->OnInitDialog(hwnd);
        return TRUE;
    }
    else
    {
        pThis = reinterpret_cast<OptionsDlg*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    }

    if (pThis != nullptr)
    {
        switch (message)
        {
        case WM_COMMAND:
            {
                WORD id = LOWORD(wparam);
                if (id == IDOK)
                {
                    pThis->OnOK(hwnd);
                    EndDialog(hwnd, IDOK);
                    return TRUE;
                }
                else if (id == IDCANCEL)
                {
                    EndDialog(hwnd, IDCANCEL);
                    return TRUE;
                }
            }
            break;
        }
    }

    return FALSE;
}
