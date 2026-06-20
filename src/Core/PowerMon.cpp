#include "PowerMon.h"
#include "ConfigManager.h"
#include "../Hardware/BatteryReader.h"
#include "../Hardware/PdhPowerReader.h"
#include "../Hardware/GpuPowerReader.h"
#include "../UI/OptionsDlg.h"
#include "../Framework/PluginCommon.h"

// 全局静态单例的正确定义方式
PowerMon PowerMon::m_instance;

PowerMon* PowerMon::Instance()
{
    return &m_instance;
}

PowerMon::PowerMon()
{
    // 创建四个子项目
    // 1. CPU 功耗
    m_items.push_back(new PowerMonItem(L"CPU 功耗", L"cpu_power", L"CPU:", L"999.9 W"));
    // 2. GPU 功耗
    m_items.push_back(new PowerMonItem(L"GPU 功耗", L"gpu_power", L"GPU:", L"999.9 W"));
    // 3. 电池放电功率
    m_items.push_back(new PowerMonItem(L"电池功耗", L"battery_power", L"电池:", L"999.9 W"));
    // 4. 智能合并项 (智能模式展示)
    m_items.push_back(new PowerMonItem(L"整机功耗", L"smart_power", L"功耗:", L"999.9 W"));
}

PowerMon::~PowerMon()
{
    for (auto* item : m_items)
    {
        delete item;
    }
    m_items.clear();

    // 释放资源
    PdhPowerReader::Instance().Close();
    GpuPowerReader::Instance().Close();
}

int PowerMon::GetAPIVersion() const
{
    return 7; // 最新支持接口 API 版本 7
}

IPluginItem* PowerMon::GetItem(int index)
{
    if (index >= 0 && index < static_cast<int>(m_items.size()))
    {
        const SettingData& setting = ConfigManager::Instance().GetSetting();
        // 智能模式过滤机制：
        // 若开启智能模式，我们在列表中只向主程序暴露“整机功耗”的 Item，或者是将其他项屏蔽
        // 此时，TrafficMonitor 会依据我们暴露的 Item 接口来进行勾选与渲染。
        // 为了兼容性和完整性，我们按索引暴露所有项，但在 DataRequired() 里根据设置清空不可视的项数值。
        return m_items[index];
    }
    return nullptr;
}

void PowerMon::OnInitialize(ITrafficMonitor* pApp)
{
    // 加载配置
    ConfigManager::Instance().LoadConfig();

    // 异步或直接初始化 PDH 和 GPU Reader
    PdhPowerReader::Instance().Init();
    GpuPowerReader::Instance().Init();
}

void PowerMon::DataRequired()
{
    // 1. 获取配置数据
    const SettingData& setting = ConfigManager::Instance().GetSetting();

    // 2. 判断供电状态
    SYSTEM_POWER_STATUS powerStatus{};
    bool isDcMode = false;
    if (GetSystemPowerStatus(&powerStatus))
    {
        isDcMode = (powerStatus.ACLineStatus == 0); // 0 为电池供电，1 为交流电供电
    }

    double cpuPower = 0.0;
    double gpuPower = 0.0;
    double batteryPower = 0.0;
    double smartPower = 0.0;

    // 3. 执行硬件读取
    if (isDcMode) // 电池供电状态 (DC)
    {
        if (setting.battery_mon_enable)
        {
            BatteryReader::Instance().GetBatteryDischargeRate(batteryPower);
        }

        if (setting.smart_mode)
        {
            smartPower = batteryPower;
        }
    }
    else // AC 供电状态
    {
        if (setting.cpu_mon_enable)
        {
            PdhPowerReader::Instance().GetCpuPower(cpuPower);
        }
        if (setting.gpu_mon_enable)
        {
            GpuPowerReader::Instance().GetGpuPower(gpuPower);
        }

        if (setting.smart_mode)
        {
            smartPower = cpuPower + gpuPower;
        }
    }

    // 4. 更新子显示项的数据
    // 在智能模式开启时，屏蔽各分项显示（或只保留智能项），这里对被屏蔽项赋值为 0.0 或空
    if (setting.smart_mode)
    {
        // 智能模式：更新智能合并项数值，将分项清零/置空
        m_items[PowerMonCommon::ITEM_INDEX_CPU]->UpdateValue(0.0, setting);
        m_items[PowerMonCommon::ITEM_INDEX_GPU]->UpdateValue(0.0, setting);
        m_items[PowerMonCommon::ITEM_INDEX_BATTERY]->UpdateValue(0.0, setting);
        m_items[PowerMonCommon::ITEM_INDEX_SMART]->UpdateValue(smartPower, setting);
    }
    else
    {
        // 普通模式：分别更新并显示各子项数值，智能合并项置 0
        m_items[PowerMonCommon::ITEM_INDEX_CPU]->UpdateValue(cpuPower, setting);
        m_items[PowerMonCommon::ITEM_INDEX_GPU]->UpdateValue(gpuPower, setting);
        m_items[PowerMonCommon::ITEM_INDEX_BATTERY]->UpdateValue(batteryPower, setting);
        m_items[PowerMonCommon::ITEM_INDEX_SMART]->UpdateValue(0.0, setting);
    }
}

ITMPlugin::OptionReturn PowerMon::ShowOptionsDialog(void* hParent)
{
    OptionsDlg dlg;
    int ret = dlg.Show(static_cast<HWND>(hParent));
    if (ret == IDOK)
    {
        return OR_OPTION_CHANGED;
    }
    return OR_OPTION_UNCHANGED;
}

const wchar_t* PowerMon::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"功耗监控插件";
    case TMI_DESCRIPTION:
        return L"原生 C++ 重构的系统功耗监控插件，支持 CPU、GPU 功耗与电池放电功率。";
    case TMI_AUTHOR:
        return L"Alex";
    case TMI_COPYRIGHT:
        return L"Copyright (C) 2026";
    case TMI_VERSION:
        return L"2.0.0";
    case TMI_URL:
        return L"https://github.com/AzulEterno/PowerMonPlugin-For-TrafficMonitor";
    default:
        return L"";
    }
}

const wchar_t* PowerMon::GetTooltipInfo()
{
    // 获取缓存配置，决定 Tooltip 详细文本
    const SettingData& setting = ConfigManager::Instance().GetSetting();
    static std::wstring tooltip;
    
    SYSTEM_POWER_STATUS powerStatus{};
    bool isDcMode = false;
    if (GetSystemPowerStatus(&powerStatus))
    {
        isDcMode = (powerStatus.ACLineStatus == 0);
    }

    if (isDcMode)
    {
        tooltip = L"供电模式：电池供电 (DC)\n电池放电功率: " + 
                  std::wstring(m_items[PowerMonCommon::ITEM_INDEX_BATTERY]->GetItemValueText());
    }
    else
    {
        tooltip = L"供电模式：外部电源 (AC)\n"
                  L"CPU 功耗: " + std::wstring(m_items[PowerMonCommon::ITEM_INDEX_CPU]->GetItemValueText()) + L"\n" +
                  L"GPU 功耗: " + std::wstring(m_items[PowerMonCommon::ITEM_INDEX_GPU]->GetItemValueText());
    }
    return tooltip.c_str();
}
