#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include "SettingData.h"

class ConfigManager
{
private:
    static ConfigManager m_instance;
    SettingData m_setting;
    std::wstring m_config_path;

    ConfigManager();
    ~ConfigManager();

    // 禁用拷贝与赋值
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::wstring GetDefaultConfigPath();

public:
    static ConfigManager& Instance();

    /**
     * @brief 加载 .ini 格式配置文件。
     *        如果文件不存在，将创建包含默认值的配置文件。
     */
    void LoadConfig();

    /**
     * @brief 保存当前配置至 .ini 文件。
     */
    void SaveConfig();

    /**
     * @brief 获取当前内存中缓存的配置。
     */
    const SettingData& GetSetting() const;

    /**
     * @brief 更新缓存中的配置。
     */
    void UpdateSetting(const SettingData& setting);
};
