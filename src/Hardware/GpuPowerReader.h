#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef LONG NTSTATUS;
#include <d3dkmthk.h>

class GpuPowerReader
{
private:
    static GpuPowerReader m_instance;

    HMODULE m_nvml_lib{ nullptr };
    HMODULE m_adl_lib{ nullptr };
    
    bool m_nvml_initialized{ false };
    bool m_adl_initialized{ false };

    // NVIDIA 显卡相关上下文
    void* m_nvml_device{ nullptr }; 

    // AMD 显卡相关上下文
    int m_adl_adapter_index{ -1 };

    // 防频繁唤醒独显机制
    ULONGLONG m_last_poll_time{ 0 };
    double m_cached_gpu_power{ 0.0 };
    bool m_gpu_sleeping{ false };

    GpuPowerReader();
    ~GpuPowerReader();

    // 禁用拷贝与赋值
    GpuPowerReader(const GpuPowerReader&) = delete;
    GpuPowerReader& operator=(const GpuPowerReader&) = delete;

    // 分项读取
    bool GetNvidiaPower(double& out_power_w);
    bool GetAmdPower(double& out_power_w);
    bool GetD3dKmtPower(double& out_power_w);

    // 检测当前是否为直流 (DC) 电池供电状态
    bool IsDcMode();

public:
    static GpuPowerReader& Instance();

    /**
     * @brief 初始化 GPU 接口驱动库（动态加载 NVML/ADL）
     * @return 成功返回 true，失败返回 false
     */
    bool Init();

    /**
     * @brief 获取 GPU 实时功耗
     * @param out_power_w 接收输出功耗的引用
     * @return 成功返回 true，失败返回 false
     */
    bool GetGpuPower(double& out_power_w);

    /**
     * @brief 清理并释放加载的动态库
     */
    void Close();
};
