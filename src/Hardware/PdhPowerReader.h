#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

class PdhPowerReader
{
private:
    static PdhPowerReader m_instance;
    
    PDH_HQUERY m_query{ nullptr };
    PDH_HCOUNTER m_cpu_power_counter{ nullptr };
    bool m_pdh_initialized{ false };
    
    // WMI 所需的 COM 指针与状态
    bool m_wmi_async_inited{ false };

    PdhPowerReader();
    ~PdhPowerReader();

    // 禁用拷贝与赋值
    PdhPowerReader(const PdhPowerReader&) = delete;
    PdhPowerReader& operator=(const PdhPowerReader&) = delete;

    // 回退机制实现
    bool GetCpuPowerFallbackWmi(double& out_power_w);
    bool GetCpuPowerStaticEstimation(double& out_power_w);

public:
    static PdhPowerReader& Instance();

    /**
     * @brief 初始化 PDH 查询与 CPU 计数器路径
     * @return 成功返回 true，失败返回 false
     */
    bool Init();

    /**
     * @brief 获取 CPU Package 功耗
     * @param out_power_w 接收输出功耗的引用
     * @return 成功返回 true，失败返回 false
     */
    bool GetCpuPower(double& out_power_w);

    /**
     * @brief 清理并关闭 PDH 查询句柄
     */
    void Close();
};
