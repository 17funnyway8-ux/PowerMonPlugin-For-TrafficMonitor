#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

class BatteryReader
{
private:
    static BatteryReader m_instance;
    BatteryReader();
    ~BatteryReader();

    // 禁用拷贝和赋值
    BatteryReader(const BatteryReader&) = delete;
    BatteryReader& operator=(const BatteryReader&) = delete;

    // 获取电池句柄
    HANDLE GetBatteryHandle();
    
    // WMI 备份方案获取电池放电功率
    bool GetBatteryDischargeRateWmi(double& out_power_w);

public:
    static BatteryReader& Instance();

    /**
     * @brief 获取电池实时放电率（W）
     * @param out_power_w 接收输出功耗的引用
     * @return 成功返回 true，失败返回 false
     */
    bool GetBatteryDischargeRate(double& out_power_w);
};
