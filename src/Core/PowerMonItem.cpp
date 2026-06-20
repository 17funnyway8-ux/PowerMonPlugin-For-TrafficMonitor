#include "PowerMonItem.h"
#include <vector>

PowerMonItem::PowerMonItem(std::wstring name, std::wstring id, std::wstring label, std::wstring sample)
    : m_name(name), m_id(id), m_label(label), m_sample_text(sample)
{
    m_value = L"0.0 W";
}

PowerMonItem::~PowerMonItem()
{
}

const wchar_t* PowerMonItem::GetItemName() const
{
    return m_name.c_str();
}

const wchar_t* PowerMonItem::GetItemId() const
{
    return m_id.c_str();
}

const wchar_t* PowerMonItem::GetItemLableText() const
{
    return m_label.c_str();
}

const wchar_t* PowerMonItem::GetItemValueText() const
{
    return m_value.c_str();
}

const wchar_t* PowerMonItem::GetItemValueSampleText() const
{
    return m_sample_text.c_str();
}

int PowerMonItem::GetItemWidth() const
{
    // TrafficMonitor 主程序基于字符渲染。如果是非 CustomDraw，返回 0 即可。
    // 主程序会自动基于 ValueText 与 ValueSampleText 来测算宽度。
    return 0;
}

void PowerMonItem::UpdateValue(double value, const SettingData& setting)
{
    m_raw_value = value;

    // 格式化输出。遵循：digit_length, spacing_control, unit_string
    // 生成对应的空格间距字符
    std::wstring spacing = std::wstring(setting.spacing_control, L' ');

    // 拼装格式化指令，如 "%.1f" 等，根据 digit_length 限定
    wchar_t valFormat[16];
    swprintf_s(valFormat, L"%%.%df", setting.digit_length);

    wchar_t valBuf[64];
    swprintf_s(valBuf, valFormat, m_raw_value);

    // 拼装：数值 + 间距 + 单位字符串
    m_value = std::wstring(valBuf) + spacing + setting.unit_string;
}
