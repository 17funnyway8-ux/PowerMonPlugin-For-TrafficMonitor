#include "BatteryReader.h"
#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <devguid.h>
#include <poclass.h>
#include <comdef.h>
#include <WbemIdl.h>
#include <cmath>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wbemuuid.lib")

BatteryReader BatteryReader::m_instance;

BatteryReader::BatteryReader()
{
}

BatteryReader::~BatteryReader()
{
}

BatteryReader& BatteryReader::Instance()
{
    return m_instance;
}

HANDLE BatteryReader::GetBatteryHandle()
{
    HDEVINFO hDevInfo = SetupDiGetClassDevsW(&GUID_DEVICE_BATTERY, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE)
    {
        return INVALID_HANDLE_VALUE;
    }

    SP_DEVICE_INTERFACE_DATA did{};
    did.cbSize = sizeof(did);

    HANDLE hBattery = INVALID_HANDLE_VALUE;

    // 枚举第一个找到的电池设备
    if (SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVICE_BATTERY, 0, &did))
    {
        DWORD cbRequired = 0;
        // 获取所需的缓冲区大小
        SetupDiGetDeviceInterfaceDetailW(hDevInfo, &did, nullptr, 0, &cbRequired, nullptr);
        if (cbRequired > 0)
        {
            auto* pdidDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(cbRequired);
            if (pdidDetail != nullptr)
            {
                pdidDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
                if (SetupDiGetDeviceInterfaceDetailW(hDevInfo, &did, pdidDetail, cbRequired, nullptr, nullptr))
                {
                    hBattery = CreateFileW(pdidDetail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                }
                free(pdidDetail);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return hBattery;
}

bool BatteryReader::GetBatteryDischargeRate(double& out_power_w)
{
    out_power_w = 0.0;
    HANDLE hBattery = GetBatteryHandle();
    if (hBattery == INVALID_HANDLE_VALUE)
    {
        // 尝试通过 WMI 回退
        return GetBatteryDischargeRateWmi(out_power_w);
    }

    bool success = false;
    DWORD dwBytesReturned = 0;

    // 1. 获取电池 Query Tag
    BATTERY_QUERY_INFORMATION bqi{};
    bqi.InformationLevel = BatteryInformation;
    
    BATTERY_INFORMATION bi{};
    // 先查询 Battery Information 获取 Capabilities
    ULONG batteryTag = 0;
    BATTERY_WAIT_STATUS bws{};
    
    // 获取电池的 Tag，很多 IOCTL 操作需要该 Tag
    if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_TAG, &bws.Timeout, sizeof(bws.Timeout),
        &batteryTag, sizeof(batteryTag), &dwBytesReturned, nullptr) && batteryTag != 0)
    {
        bqi.BatteryTag = batteryTag;
        // 获取电池基本信息
        if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi, sizeof(bqi),
            &bi, sizeof(bi), &dwBytesReturned, nullptr))
        {
            // 查询电池实时状态
            BATTERY_STATUS bs{};
            bws.BatteryTag = batteryTag;
            if (DeviceIoControl(hBattery, IOCTL_BATTERY_QUERY_STATUS, &bws, sizeof(bws),
                &bs, sizeof(bs), &dwBytesReturned, nullptr))
            {
                // Rate 大于 0 且处于放电状态
                // 状态位：BATTERY_DISCHARGING 表示放电
                if ((bs.PowerState & BATTERY_DISCHARGING) && bs.Rate != BATTERY_UNKNOWN_RATE)
                {
                    // Rate 在放电时可能为负值（根据某些 ACPI 规范，也可能为正值，所以取绝对值）
                    double rate = std::abs(static_cast<double>(bs.Rate));
                    // 判断单位是 mW 还是 mA
                    // 根据 MSDN：如果 Capabilities 没有设置 BATTERY_CAPACITY_RELATIVE，单位是 mW。
                    // 否则，如果是相对值，单位是 mA，需乘以当前电压来转换成 mW。
                    if (bi.Capabilities & BATTERY_CAPACITY_RELATIVE)
                    {
                        // 值为 mA，转换：mW = mA * (Voltage / 1000)
                        if (bs.Voltage != BATTERY_UNKNOWN_VOLTAGE && bs.Voltage > 0)
                        {
                            rate = rate * (static_cast<double>(bs.Voltage) / 1000.0);
                        }
                    }
                    out_power_w = rate / 1000.0; // 转换为 W
                    success = true;
                }
                else
                {
                    // 未在放电，则放电功耗计为 0.0W
                    out_power_w = 0.0;
                    success = true;
                }
            }
        }
    }

    CloseHandle(hBattery);

    if (!success)
    {
        // 尝试 WMI 回退
        return GetBatteryDischargeRateWmi(out_power_w);
    }

    return success;
}

bool BatteryReader::GetBatteryDischargeRateWmi(double& out_power_w)
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

        // 连接 root\WMI 命名空间
        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc);
        if (FAILED(hr)) break;

        // 设置安全级别
        hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) break;

        // 查询 BatteryStatus 
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t("SELECT DischargeRate, ChargeRate, Voltage, Discharging FROM BatteryStatus"),
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
                VARIANT varDischargeRate;
                VARIANT varDischarging;
                VARIANT varVoltage;
                
                VariantInit(&varDischargeRate);
                VariantInit(&varDischarging);
                VariantInit(&varVoltage);

                HRESULT hr1 = pclsObj->Get(L"DischargeRate", 0, &varDischargeRate, nullptr, nullptr);
                HRESULT hr2 = pclsObj->Get(L"Discharging", 0, &varDischarging, nullptr, nullptr);
                HRESULT hr3 = pclsObj->Get(L"Voltage", 0, &varVoltage, nullptr, nullptr);

                if (SUCCEEDED(hr1) && SUCCEEDED(hr2) && varDischarging.vt == VT_BOOL && varDischarging.boolVal)
                {
                    double rate = 0.0;
                    if (varDischargeRate.vt == VT_I4) rate = varDischargeRate.lVal;
                    else if (varDischargeRate.vt == VT_UI4) rate = varDischargeRate.ulVal;
                    
                    // 部分 WMI 返回毫瓦，部分返回毫安。
                    // 启发式逻辑：通常在 BatteryStatus 中 DischargeRate 单位是毫瓦。
                    // 如果存在 Voltage，且值极大，判断是否需要根据电压换算。
                    // 统一折算：WMI 的 DischargeRate 一般是毫瓦(mW)
                    out_power_w = rate / 1000.0;
                    success = true;
                }
                else
                {
                    // 未放电，置 0.0W
                    out_power_w = 0.0;
                    success = true;
                }

                VariantClear(&varDischargeRate);
                VariantClear(&varDischarging);
                VariantClear(&varVoltage);
                pclsObj->Release();
            }
        }
    } while (false);

    if (pEnumerator) pEnumerator->Release();
    if (pSvc) pSvc->Release();
    if (pLoc) pLoc->Release();
    if (coInitialized) CoUninitialize();

    return success;
}
