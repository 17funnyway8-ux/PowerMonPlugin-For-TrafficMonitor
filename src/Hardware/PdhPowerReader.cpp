#include "PdhPowerReader.h"
#include <comdef.h>
#include <WbemIdl.h>
#include <string>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wbemuuid.lib")

PdhPowerReader PdhPowerReader::m_instance;

PdhPowerReader::PdhPowerReader()
{
}

PdhPowerReader::~PdhPowerReader()
{
    Close();
}

PdhPowerReader& PdhPowerReader::Instance()
{
    return m_instance;
}

bool PdhPowerReader::Init()
{
    if (m_pdh_initialized)
    {
        return true;
    }

    PDH_STATUS status = PdhOpenQueryW(nullptr, 0, &m_query);
    if (status != ERROR_SUCCESS)
    {
        m_query = nullptr;
        return false;
    }

    // 优先尝试基于数字索引的英文中立路径，避免语言环境导致的无法识别路径。
    // \Processor Information(_Total)\Processor Power 的数字路径对应为：
    // \238(_Total)\6 （根据特定系统可能略有不同）
    // 我们在此首先添加标准字符串路径，若失败则尝试使用索引路径
    
    // 路径 1: 英文中立/标准路径
    status = PdhAddCounterW(m_query, L"\\Processor Information(_Total)\\Processor Power", 0, &m_cpu_power_counter);
    if (status != ERROR_SUCCESS)
    {
        // 路径 2: 某些老系统或特殊系统可能是 \Processor(_Total)\Processor Power
        status = PdhAddCounterW(m_query, L"\\Processor(_Total)\\Processor Power", 0, &m_cpu_power_counter);
    }
    
    if (status != ERROR_SUCCESS)
    {
        // 路径 3: 尝试通过数字索引添加。238 对应 "Processor Information"，6 对应 "Processor Power"
        status = PdhAddCounterW(m_query, L"\\238(_Total)\\6", 0, &m_cpu_power_counter);
    }

    if (status != ERROR_SUCCESS)
    {
        PdhCloseQuery(m_query);
        m_query = nullptr;
        m_cpu_power_counter = nullptr;
        return false;
    }

    // 预收集一次以建立数据底座
    PdhCollectQueryData(m_query);

    m_pdh_initialized = true;
    return true;
}

bool PdhPowerReader::GetCpuPower(double& out_power_w)
{
    out_power_w = 0.0;
    
    if (!m_pdh_initialized)
    {
        if (!Init())
        {
            // 初始化失败，走 WMI 回退
            return GetCpuPowerFallbackWmi(out_power_w);
        }
    }

    PDH_STATUS status = PdhCollectQueryData(m_query);
    if (status != ERROR_SUCCESS)
    {
        return GetCpuPowerFallbackWmi(out_power_w);
    }

    DWORD counterType = 0;
    PDH_FMT_COUNTERVALUE fmtValue{};
    status = PdhGetFormattedCounterValue(m_cpu_power_counter, PDH_FMT_DOUBLE, &counterType, &fmtValue);
    if (status != ERROR_SUCCESS || (fmtValue.CStatus != PDH_CSTATUS_VALID_DATA && fmtValue.CStatus != PDH_CSTATUS_NEW_DATA))
    {
        return GetCpuPowerFallbackWmi(out_power_w);
    }

    out_power_w = fmtValue.doubleValue;
    // 防御性：功耗不应为负数
    if (out_power_w < 0.0) out_power_w = 0.0;

    return true;
}

bool PdhPowerReader::GetCpuPowerFallbackWmi(double& out_power_w)
{
    out_power_w = 0.0;

    // 初始化 COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    IEnumWbemClassObject* pEnumerator = nullptr;
    bool success = false;

    do
    {
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
        if (FAILED(hr)) break;

        // 连接 root\cimv2
        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
        if (FAILED(hr)) break;

        hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) break;

        // 执行查询以获取 CPU 功率
        // 尝试查询 Win32_PerfFormattedData_PerfOS_Processor
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT ProcessorPower FROM Win32_PerfFormattedData_PerfOS_Processor WHERE Name='_Total'"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &pEnumerator
        );
        if (FAILED(hr)) break;

        IWbemClassObject* pclsObj = nullptr;
        ULONG uReturn = 0;

        if (pEnumerator)
        {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (SUCCEEDED(hr) && uReturn > 0)
            {
                VARIANT varPower;
                VariantInit(&varPower);
                hr = pclsObj->Get(L"ProcessorPower", 0, &varPower, nullptr, nullptr);
                if (SUCCEEDED(hr))
                {
                    double p = 0.0;
                    if (varPower.vt == VT_I4) p = varPower.lVal;
                    else if (varPower.vt == VT_UI4) p = varPower.ulVal;
                    else if (varPower.vt == VT_BSTR) p = _wtof(varPower.bstrVal);
                    
                    out_power_w = p;
                    success = true;
                }
                VariantClear(&varPower);
                pclsObj->Release();
            }
        }
    } while (false);

    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    if (coInitialized) CoUninitialize();

    if (!success)
    {
        // 如果 WMI 也失败，走最后一层静态估算回退
        return GetCpuPowerStaticEstimation(out_power_w);
    }

    return success;
}

bool PdhPowerReader::GetCpuPowerStaticEstimation(double& out_power_w)
{
    // 获取 CPU 核心数
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    DWORD numProcessors = sysInfo.dwNumberOfProcessors;

    // 基于 CPU 使用率的静态估算
    FILETIME idleTime, kernelTime, userTime;
    static FILETIME prevIdleTime{ 0, 0 }, prevKernelTime{ 0, 0 }, prevUserTime{ 0, 0 };

    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
    {
        out_power_w = 15.0; // 默认缺省值
        return true;
    }

    ULONGLONG idle = ((ULONGLONG)idleTime.dwHighDateTime << 32) | idleTime.dwLowDateTime;
    ULONGLONG kernel = ((ULONGLONG)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
    ULONGLONG user = ((ULONGLONG)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;

    ULONGLONG prevIdle = ((ULONGLONG)prevIdleTime.dwHighDateTime << 32) | prevIdleTime.dwLowDateTime;
    ULONGLONG prevKernel = ((ULONGLONG)prevKernelTime.dwHighDateTime << 32) | prevKernelTime.dwLowDateTime;
    ULONGLONG prevUser = ((ULONGLONG)prevUserTime.dwHighDateTime << 32) | prevUserTime.dwLowDateTime;

    prevIdleTime = idleTime;
    prevKernelTime = kernelTime;
    prevUserTime = userTime;

    double cpuUsage = 0.0;
    if (prevIdle != 0)
    {
        ULONGLONG idleDiff = idle - prevIdle;
        ULONGLONG kernelDiff = kernel - prevKernel;
        ULONGLONG userDiff = user - prevUser;
        ULONGLONG totalDiff = kernelDiff + userDiff;

        if (totalDiff > 0)
        {
            cpuUsage = (double)(totalDiff - idleDiff) / totalDiff;
            if (cpuUsage < 0.0) cpuUsage = 0.0;
            if (cpuUsage > 1.0) cpuUsage = 1.0;
        }
    }

    // 默认 TDP 计算规则：每个逻辑核心按约 6W TDP 默认评估。总 TDP = 核心数 * 6。
    // TDP 不低于 15W，不超过 125W
    double defaultTDP = static_cast<double>(numProcessors) * 6.0;
    if (defaultTDP < 15.0) defaultTDP = 15.0;
    if (defaultTDP > 125.0) defaultTDP = 125.0;

    // 静态功耗公式：Power = TDP_default * (0.1 + 0.9 * CpuUsage)
    out_power_w = defaultTDP * (0.1 + 0.9 * cpuUsage);
    return true;
}

void PdhPowerReader::Close()
{
    if (m_query != nullptr)
    {
        PdhCloseQuery(m_query);
        m_query = nullptr;
    }
    m_cpu_power_counter = nullptr;
    m_pdh_initialized = false;
}
