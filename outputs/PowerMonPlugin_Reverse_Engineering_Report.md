# TrafficMonitor PowerMonPlugin 逆向工程与重构报告 (ORRI)

根据**逆向工程融合(ORRI)方法论**，对 `AzulEterno/PowerMonPlugin-For-TrafficMonitor` 项目的混合模式（C++/CLI）DLL 二进制文件进行分析与逆向，确认该项目不包含源代码，所有分支与历史记录均已彻底清洗。

以下是完整的逆向架构、工作原理还原及后续二次开发（二开）的落地方案。

---

## 1. ⚙️ 系统架构与数据流图 (Observation & Reverse)

该插件并非纯 Native C++，也非纯 .NET 托管 DLL，而是典型的 **C++/CLI (混合模式)** DLL。它作为桥梁：
- **向下**：调用原生 C++ 以实现 TrafficMonitor 导出的 `ITMPlugin` 和 `IPluginItem` 接口。
- **向上**：使用 .NET CLI 包装器，引入 `LibreHardwareMonitorLib.dll` 来访问 Windows 底层传感器（CPU/GPU 功耗）；引入 WinRT 来查询电池状态。

```
                   +----------------------------------+
                   |       TrafficMonitor (C++)       |
                   +----------------------------------+
                                   |
                         (导出 C 接口或虚表调用)
                                   |
                                   v
======================= PowerMonPlugin.dll =======================
  [Native C++ 边界]
    - 导出接口：extern "C" ITMPlugin* TMPluginGetInstance();
    - 核心实现：class PowerMon : public ITMPlugin;
    - 子项监控：class SmartPowerMeterMonItem : public IPluginItem;
              class CPUPowerMonItem : public IPluginItem;
              class GPUPowerMonItem : public IPluginItem;
              class BatteryPowerMonItem : public IPluginItem;
              (继承自 HardwareSensorMonBase 或 BatteryGrpMonBaseCLS)

                                   ^  (混入 C++/CLI 调用)
                                   v
  [C++/CLI 桥接代理]
    - 内部代理：InterOpLibreHWMon::LibreHWMonInterOpProxy
    - 遍历器  ：InterOpLibreHWMon::PowerSensorUpdateVisitor
==================================================================
                                   |
                         (.NET Assembly Ref)
                                   |
                                   v
               +--------------------------------------+
               |      LibreHardwareMonitorLib.dll     |
               | (C# 开源硬件监控，访问 CPU/GPU 功耗) |
               +--------------------------------------+
```

---

## 2. 📑 核心元数据还原与关键接口规范 (Protocol & Interface)

### 2.1 Native C++ 插件与项接口 (TrafficMonitor 规范)
TrafficMonitor 官方定义的插件接口通常包含 `ITMPlugin` 和 `IPluginItem`。本插件对这些虚函数接口进行了二进制兼容实现：

```cpp
// 监控项目信息索引
enum class PluginInfoIndex {
    NAME = 0,
    AUTHOR = 1,
    DESC = 2,
    VERSION = 3,
    VERSION_CODE = 4, // 插件版本码
};

// 鼠标事件类型
enum class MouseEventType {
    LBUTTONDOWN = 0,
    RBUTTONDOWN = 1,
    LBUTTONDBLCLK = 2,
    // ...
};

// 每一个监控子项
class IPluginItem {
public:
    virtual const wchar_t* GetItemName() = 0;       // 监控项名称
    virtual const wchar_t* GetItemId() = 0;         // 监控项ID (唯一识别)
    virtual const wchar_t* GetItemLableText() = 0;  // 悬浮窗上显示的标签
    virtual const wchar_t* GetItemValueText() = 0;  // 悬浮窗上显示的值
    virtual const wchar_t* GetItemValueSampleText() = 0;
    virtual int GetItemWidth() = 0;                 // 项目显示宽度
    virtual int OnMouseEvent(MouseEventType event_type, int x, int y, void* hWnd, int flags) = 0;
    virtual void* OnItemInfo(int info_type, void* arg1, void* arg2) = 0; 
};

// 插件主控制器
class ITMPlugin {
public:
    virtual const wchar_t* GetInfo(PluginInfoIndex index) = 0;
    virtual IPluginItem* GetItem(int index) = 0; // 根据索引获取子项
    virtual int GetItemCount() = 0;              // 返回支持的项目数量
    virtual void DataRequired() = 0;             // 核心轮询更新函数，TrafficMonitor 每秒调用
    virtual const wchar_t* GetTooltipInfo() = 0;  // 鼠标悬停提示
    virtual int ShowOptionsDialog(void* parent) = 0; // 弹出选项配置界面
    // 接口版本，当前为 1 即可
    virtual int GetAPIVersion() = 0; 
};
```

### 2.2 C++/CLI 托管端桥接代理
位于 `InterOpLibreHWMon` 命名空间下，以下是逆向提取出的托管类声明骨架：

```csharp
namespace InterOpLibreHWMon {
    // 遍历器，用于执行 LibreHardwareMonitor 内部的 IVisitor，获取特定的硬件/传感器
    public class PowerSensorUpdateVisitor : LibreHardwareMonitor.Hardware.IVisitor {
        private bool _cpu_mon_enable;
        private bool _gpu_mon_enable;

        public PowerSensorUpdateVisitor();
        public void SetCpuMonitorEnable(bool enable);
        public void SetGpuMonitorEnable(bool enable);
        
        public void VisitComputer(IComputer computer);
        public void VisitHardware(IHardware hardware);
        public void VisitSensor(ISensor sensor);
        public void VisitParameter(IParameter parameter);
    }

    // 互操作代理类，负责生存期管理和数据同步
    public class LibreHWMonInterOpProxy : System.IDisposable {
        private static LibreHWMonInterOpProxy __instance;
        private bool _cpu_mon_enable;
        private bool _gpu_mon_enable;
        private LibreHardwareMonitor.Hardware.Computer _p_IO_Computer; // 内部 CPU/GPU 监控对象
        private PowerSensorUpdateVisitor _power_sensor_update_visitor;

        // 构造函数与实例管理
        public LibreHWMonInterOpProxy(bool enable);
        public static LibreHWMonInterOpProxy Instance();
        
        // 关键资源管理与互操作
        public void PrepareInterOpObjects();
        public void ReleaseResources();
        public void SetInterOpComputorSettings();
        public void OpenInterOpComputorObject();
        public void CallUpdateComputorInfo(); // 核心方法，驱动 LibreHardwareMonitor 进行传感器轮询
        
        // C++ Struct 数据与托管配置同步 (SettingData 为 Native C++ 结构体)
        public unsafe void SyncFromSettingData(SettingData* pSetting);
        
        public PowerSensorUpdateVisitor GetUpdateVistor();
        public void Dispose();
    }
}
```

---

## 3. 🛠️ 源代码骨架重建方案 (Reconstruction)

为了重构该插件，你可以直接采用 C++/CLI 混合项目结构。在 Visual Studio 中创建“CLR 动态链接库”项目。

### 3.1 核心配置与全局管理：`SettingData` 和 `CDataManager`

```cpp
// 约定数据大小：80 字节 (见 ClassLayout Size)
struct SettingData {
    bool cpu_mon_enable;
    bool gpu_mon_enable;
    bool battery_mon_enable;
    int digit_length;          // 数字显示长度
    int spacing_control;       // 间距控制
    wchar_t unit_string[16];   // 单位字符 (e.g. "W", "mW")
    // ... 其他对齐与扩展配置字段
};

class CDataManager {
private:
    SettingData m_setting;
    static CDataManager m_instance;
public:
    static CDataManager& Instance() { return m_instance; }
    void LoadConfig();
    void SaveConfig();
    SettingData& GetSetting() { return m_setting; }
    // 执行首期加载配置的检查
    bool FirstInitalCheck(SettingData setting);
};
```

### 3.2 插件主类及生命周期：`PowerMon`

```cpp
#include "SettingData.h"

// 对应 Type [267] PowerMon，大小 520 字节
class PowerMon : public ITMPlugin {
private:
    static PowerMon m_instance;
    // 托管对象的根指针，用于在非托管类中持有托管代理
    gcroot<InterOpLibreHWMon::LibreHWMonInterOpProxy^> m_proxy;
    
public:
    PowerMon() {
        // 初始化代理
        m_proxy = gcnew InterOpLibreHWMon::LibreHWMonInterOpProxy(true);
    }
    
    static PowerMon* Instance() { return &m_instance; }

    virtual const wchar_t* GetInfo(PluginInfoIndex index) override {
        switch (index) {
            case PluginInfoIndex::NAME: return L"功耗监控插件";
            case PluginInfoIndex::AUTHOR: return L"AzulEterno";
            case PluginInfoIndex::VERSION: return L"1.0.0";
            // ...
        }
        return L"";
    }

    virtual IPluginItem* GetItem(int index) override;
    virtual int GetItemCount() override { return 5; } // Battery, CPU, GPU, Platform, Time etc.

    virtual void DataRequired() override {
        // 1. 调用托管代理，进行 LibreHardwareMonitor 传感器数据更新
        if (m_proxy != nullptr) {
            m_proxy->CallUpdateComputorInfo();
        }
        
        // 2. 读取 WinRT API 获取电池功耗 (如果是放电模式)
        // 3. 将值计算后写到各个子 MonItem 中
    }

    virtual const wchar_t* GetTooltipInfo() override {
        return L"CPU/GPU/电池 功耗实时监控";
    }

    virtual int ShowOptionsDialog(void* parent) override {
        // 显示配置对话框，并同步 SettingData
        return 0;
    }
    
    virtual int GetAPIVersion() override { return 1; }
};

// 导出唯一实例
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance() {
    return PowerMon::Instance();
}
```

### 3.3 功耗获取逻辑 (WinRT & LibreHardwareMonitor)

根据日志及逆向大小，插件在电池模式下使用 **WinRT (Windows.Devices.Power)** 来获取准确的电池功率，其余状态使用 LibreHardwareMonitor：

```cpp
// 电池功率获取伪代码 (混合 C++/WinRT)
#include <winrt/Windows.Devices.Power.h>
#include <winrt/Windows.System.Power.h>

double GetBatteryDischargeRateW() {
    using namespace winrt::Windows::Devices::Power;
    auto report = Battery::AggregateBattery().GetReport();
    if (report) {
        auto rate = report.DischargeRateInMilliwatts();
        if (rate.has_value()) {
            return rate.value() / 1000.0; // 毫瓦转瓦特
        }
    }
    return 0.0;
}
```

对于 CPU 和 GPU 的功耗，`InterOpLibreHWMon::PowerSensorUpdateVisitor` 中遍历 LibreHardwareMonitor 的硬件树：
- **CPU**: 匹配 `HardwareType.Cpu`。在传感器中匹配 `SensorType.Power` 且名字包含 `Package` 或 `Core` 的值。
- **GPU**: 匹配 `HardwareType.GpuNvidia` 或 `HardwareType.GpuAmd`（日志提到了集显识别逻辑）。获取其对应的 `SensorType.Power`。

---

## 4. 💡 二开与优化方向建议 (Innovate)

逆向完成后，原项目的结构和逻辑已清晰展现，此时可开展以下二开工作：

1. **🚀 跨平台支持 (Linux/ARM64)**
   * 原作者提到：“因为 WinRT 运行时和 ARM64EC 不兼容，所以 ARM64 用户需要使用 X64 版本”。
   * **二开方向**：在 ARM64 编译环境下，采用 Native API（例如传统的 `GetSystemPowerStatus` 或 WMI `BatteryStatus`）替换 WinRT，以完全兼容原生 ARM64/ARM64EC 编译，大幅降低轻薄本（搭载 Snapdragon X Elite 或 ARM 芯片）上的 CPU 开销。
2. **📈 CPU 细分监控 (Core/Package 拆分)**
   * 原版仅输出总的 CPU Package Power。
   * **二开方向**：增加“CPU 核心功耗”和“CPU Uncore 功耗”的细分项目，对于调试处理器调度非常有用。
3. **🎨 自定义渲染与图标扩展**
   * 目前 TrafficMonitor 仅支持输出文本。
   * **二开方向**：在 `IPluginItem::OnItemInfo` 中返回自定义绘图（如在悬浮窗上画出功耗历史曲线折线图，这在 2.0 接口中是支持的）。
4. **🔌 平台总功耗估算 (Platform Power)**
   * 目前的 Platform Power 处理逻辑比较简单。
   * **二开方向**：引入更完善的平台估算公式（如：`Platform = CPU + GPU + Screen + RAM + Loss`），给没有总功耗传感器的台式机或轻薄本估算整机插座端输入功率。
