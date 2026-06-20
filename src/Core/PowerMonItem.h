#pragma once

#include "../Framework/ITMPlugin.h"
#include "SettingData.h"
#include <string>

class PowerMonItem : public IPluginItem
{
private:
    std::wstring m_name;
    std::wstring m_id;
    std::wstring m_label;
    std::wstring m_value;
    std::wstring m_sample_text;
    
    // 缓存数据的值，用于宽度计算或状态维护
    double m_raw_value{ 0.0 };

public:
    PowerMonItem(std::wstring name, std::wstring id, std::wstring label, std::wstring sample);
    virtual ~PowerMonItem();

    // IPluginItem 接口覆写
    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    int GetItemWidth() const override;

    // 自定义更新数值的方法，内部执行格式化
    void UpdateValue(double value, const SettingData& setting);
};
