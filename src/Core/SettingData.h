#pragma once

#include <wchar.h>

// 与原版 SettingData 二进制兼容的纯 C++ 结构体
// 注意字节对齐及字段顺序
#pragma pack(push, 8)
struct SettingData
{
    bool cpu_mon_enable{ true };
    bool gpu_mon_enable{ true };
    bool battery_mon_enable{ true };
    int digit_length{ 1 };          // 默认保留一位小数
    int spacing_control{ 1 };       // 默认包含一个空格
    wchar_t unit_string[16]{ L"W" }; // 默认单位为 W
    bool smart_mode{ true };        // 默认开启智能模式
};
#pragma pack(pop)
