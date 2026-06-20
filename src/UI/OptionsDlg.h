#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "../Core/SettingData.h"

class OptionsDlg
{
private:
    HWND m_hwnd{ nullptr };
    SettingData m_temp_setting;

    // 对话框的初始化与数据装载
    void OnInitDialog(HWND hwnd);

    // 从控件收集修改后的配置数据
    void OnOK(HWND hwnd);

public:
    OptionsDlg();
    ~OptionsDlg();

    // 核心的 DlgProc 对话框过程
    static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

    /**
     * @brief 弹出配置对话框
     * @param parent_hwnd 父窗口句柄
     * @return 返回 IDOK 或 IDCANCEL
     */
    int Show(HWND parent_hwnd);
};
