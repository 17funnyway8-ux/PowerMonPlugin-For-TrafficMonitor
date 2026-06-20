#include "ConfigManager.h"
#include <shlobj.h>

ConfigManager ConfigManager::m_instance;

ConfigManager::ConfigManager()
{
    m_config_path = GetDefaultConfigPath();
}

ConfigManager::~ConfigManager()
{
}

ConfigManager& ConfigManager::Instance()
{
    return m_instance;
}

std::wstring ConfigManager::GetDefaultConfigPath()
{
    // 获取 %APPDATA%
    wchar_t szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, szPath)))
    {
        std::wstring appFolder = std::wstring(szPath) + L"\\TrafficMonitor";
        // 确保文件夹存在
        CreateDirectoryW(appFolder.c_str(), nullptr);
        return appFolder + L"\\power_mon_config.ini";
    }
    return L".\\power_mon_config.ini";
}

void ConfigManager::LoadConfig()
{
    // 使用 Win32 GetPrivateProfileIntW 和 GetPrivateProfileStringW 读配置项
    const wchar_t* appName = L"PowerMonitor";

    m_setting.cpu_mon_enable = GetPrivateProfileIntW(appName, L"CpuMonEnable", 1, m_config_path.c_str()) != 0;
    m_setting.gpu_mon_enable = GetPrivateProfileIntW(appName, L"GpuMonEnable", 1, m_config_path.c_str()) != 0;
    m_setting.battery_mon_enable = GetPrivateProfileIntW(appName, L"BatteryMonEnable", 1, m_config_path.c_str()) != 0;
    m_setting.digit_length = GetPrivateProfileIntW(appName, L"DigitLength", 1, m_config_path.c_str());
    m_setting.spacing_control = GetPrivateProfileIntW(appName, L"SpacingControl", 1, m_config_path.c_str());
    m_setting.smart_mode = GetPrivateProfileIntW(appName, L"SmartMode", 1, m_config_path.c_str()) != 0;

    // 获取单位字符串
    GetPrivateProfileStringW(appName, L"UnitString", L"W", m_setting.unit_string, 16, m_config_path.c_str());
}

void ConfigManager::SaveConfig()
{
    const wchar_t* appName = L"PowerMonitor";

    auto WriteInt = [&](const wchar_t* key, int val) {
        wchar_t buf[32];
        swprintf_s(buf, L"%d", val);
        WritePrivateProfileStringW(appName, key, buf, m_config_path.c_str());
    };

    WriteInt(L"CpuMonEnable", m_setting.cpu_mon_enable ? 1 : 0);
    WriteInt(L"GpuMonEnable", m_setting.gpu_mon_enable ? 1 : 0);
    WriteInt(L"BatteryMonEnable", m_setting.battery_mon_enable ? 1 : 0);
    WriteInt(L"DigitLength", m_setting.digit_length);
    WriteInt(L"SpacingControl", m_setting.spacing_control);
    WriteInt(L"SmartMode", m_setting.smart_mode ? 1 : 0);

    WritePrivateProfileStringW(appName, L"UnitString", m_setting.unit_string, m_config_path.c_str());
}

const SettingData& ConfigManager::GetSetting() const
{
    return m_setting;
}

void ConfigManager::UpdateSetting(const SettingData& setting)
{
    m_setting = setting;
    SaveConfig();
}
