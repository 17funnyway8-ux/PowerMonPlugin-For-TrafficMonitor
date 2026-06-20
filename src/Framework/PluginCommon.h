#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

// 全局常量定义
namespace PowerMonCommon
{
    // 监控项索引常量
    constexpr int ITEM_INDEX_CPU = 0;
    constexpr int ITEM_INDEX_GPU = 1;
    constexpr int ITEM_INDEX_BATTERY = 2;
    constexpr int ITEM_INDEX_SMART = 3;
    constexpr int ITEM_COUNT = 4;

    // 默认值常量
    constexpr double DEFAULT_POWER_VALUE = 0.0;
    
    // 定时器/刷新率定义
    constexpr DWORD SYSTEM_POWER_CHECK_INTERVAL_MS = 1000;
}
