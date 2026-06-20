# 功耗监控插件重构与性能优化 (PowerMonPlugin Reconstruct) - 产品需求文档 (PRD)

## 1. 项目信息
* **项目名称**：`power_mon_plugin`
* **语言 (Language)**：简体中文
* **开发语言/技术栈**：纯原生 C++ (MSVC, Win32 SDK, C++17)
* **负责人**：产品经理 许清楚 (Xu)
* **原始需求复述**：
  对原版基于 C++/CLI 托管混合模式编写的 TrafficMonitor 功耗插件 `AzulEterno/PowerMonPlugin-For-TrafficMonitor` 进行彻底的【纯原生 C++】重构与性能优化。移除对 .NET CLR、`LibreHardwareMonitorLib.dll` 托管类库以及 WinRT 运行时的强依赖，解决在 ARM64 和 ARM64EC 等平台上的兼容性与高延迟、高内存驻留（原版 30MB+）问题，用原生 API 获取电池与 CPU/GPU 硬件功耗，保留原有数字长度、间距控制、单位字符修改与智能功耗模式等核心功能。

## 2. 产品定义

### 2.1 产品目标 (Product Goals)
1. **零托管运行时依赖与全平台原生兼容 (Native Compatibility)**：完全移出 .NET CLR 与 WinRT 依赖，提供 x86、x64、ARM64 及 ARM64EC 的原生编译与加载支持，消除特定平台的二进制兼容和加载崩溃问题。
2. **极简系统资源开销 (High Performance & Low Footprint)**：单次轮询 CPU 占用微乎其微，内存驻留从 30MB+ 降至 2MB 以下，消除由于 JIT 和 CLR 初始化导致的 TrafficMonitor 启动延迟。
3. **精准与多源硬件功耗监测 (Accurate Hardware Power Telemetry)**：保留智能功耗显示模式，提供在电池放电状态下读取电池放电功率、在电源接通状态下读取 CPU 核心/Package 功耗与主流 GPU (NVIDIA/AMD) 功耗的精准数据源。

### 2.2 用户故事 (User Stories)
* **User Story 1**: As an ARM64 laptop user (e.g., Snapdragon X Elite), I want a native ARM64EC/ARM64 power monitoring plugin so that I can monitor my laptop's battery discharge and chip power consumption without running x64 emulation or triggering CLR compatibility crashes.
* **User Story 2**: As a performance-oriented system monitor user, I want the power plugin to occupy minimum memory (less than 2MB) and near-zero CPU cycles so that the background monitoring tool does not impact my gaming or heavy compiling workloads.
* **User Story 3**: As a mobile office worker, I want the plugin to automatically switch between battery discharge rate (when unplugged) and CPU+GPU joint power consumption (when plugged in) so that I can dynamically understand my power status under different scenarios without manual toggling.
* **User Story 4**: As a detail-oriented enthusiast, I want to customize the digit length, item spacing, and unit symbol (e.g. from 'W' to 'mW') in the configuration settings so that the monitoring UI integrates perfectly with my customized TrafficMonitor taskbar layout.

## 3. 技术规范与需求池

### 3.1 核心需求池 (Requirements Pool)

| 需求ID | 优先级 | 需求分类 | 功能/技术描述 | 验收标准 |
| :--- | :--- | :--- | :--- | :--- |
| **REQ-001** | **P0** | 核心架构 | 去除 C++/CLI 混合模式，改为纯原生 C++。不再依赖 .NET Runtime, CLR 以及任何托管 DLL（如 LibreHardwareMonitorLib.dll）。 | 插件在没有任何 .NET 环境的干净 Windows 系统上可原生加载并运行，不拉起 CLR，无 `mscoree.dll` 模块加载。 |
| **REQ-002** | **P0** | 电池功耗获取 | 移除对 WinRT (`Windows.Devices.Power`) 的依赖。采用原生 Windows API 获取电池放电率（如 `GetSystemPowerStatus` 判定供电状态，结合 WMI `MSBatteryClass` 或 IOCTL 访问电池传感器数据）。 | 成功在不引入 WinRT 运行时的情况下获取以瓦特（W）或毫瓦（mW）为单位的电池实时放电功率。 |
| **REQ-003** | **P0** | 硬件功耗获取 (CPU) | 使用原生方法获取 CPU Package / Core 功耗（如通过 Windows PDH 性能计数器、直接读取 MSR 寄存器，或者封装 Intel/AMD 对应底层驱动/WMI 接口）。 | 在主流 Intel 和 AMD 处理器上获取到准确的 CPU 功耗（W），误差在合理范围内，且不造成驱动冲突或系统崩溃。 |
| **REQ-004** | **P0** | 插件接口实现 | 基于 TrafficMonitor SDK 的 `ITMPlugin` 和 `IPluginItem` 虚函数接口提供完整的二进制兼容实现。 | 插件成功在 TrafficMonitor 插件列表中显示，子项可被勾选、能正常在状态栏/悬浮窗渲染。 |
| **REQ-005** | **P0** | 智能显示模式 | 依据系统供电状态（AC/DC），智能切换显示：接电时显示【CPU + GPU】功耗之和或并列项，电池供电时显示【电池放电】功耗。 | 拔插电源适配器时，插件数据源能在 2 秒内平滑切换，无卡顿。 |
| **REQ-006** | **P1** | 硬件功耗获取 (GPU) | 针对 NVIDIA GPU 动态加载 `nvml.dll` 并调用相应 API 获取功耗；针对 AMD GPU 动态加载 `atiadlxx.dll`/`atiadlxy.dll` 获取功耗，或回退至 D3DKMT 系统计数器。 | 能够识别独立显卡并获取实时 GPU 功耗，若显卡处于休眠状态（如笔记本双显卡切换），应避免频繁唤醒独显造成额外功耗。 |
| **REQ-007** | **P1** | 设置界面与持久化 | 使用原生 Win32 API（或纯 C++ 对话框）实现插件选项对话框（`ShowOptionsDialog`），支持读写配置文件（如 `config.ini`）以持久化用户设定。 | 用户在 TrafficMonitor 配置页面修改设置后可即时生效并写入配置文件，下次启动能正确读取。 |
| **REQ-008** | **P1** | 界面定制需求 | 支持限制数字显示长度（数字长度控制）、调节间距（间距控制）以及修改自定义单位字符（如修改单位为 W、mW 或 自定义文本）。 | 插件输出字符串格式严格遵循设置规范，字符长度与间距渲染与用户设置一致。 |
| **REQ-009** | **P2** | 平台兼容与多品牌优化 | 优化对核显（如 AMD Radeon 集显、Intel Iris Xe）的功耗估算或检测逻辑；支持平台总功耗（Platform Power）的估算模型。 | 在无独立显卡的轻薄本或部分台式机上，能够通过估算模型（如 Platform = CPU + 预设基础功耗）输出合理的整机参考功耗。 |

### 3.2 界面设置需求与参数定义

用户可通过 TrafficMonitor 插件的“选项”对话框配置以下参数：

* **CPU监控开关 (`cpu_mon_enable`)**：`bool`，是否开启 CPU 功耗监控。
* **GPU监控开关 (`gpu_mon_enable`)**：`bool`，是否开启 GPU 功耗监控。
* **电池监控开关 (`battery_mon_enable`)**：`bool`，是否开启电池监控。
* **数字显示长度 (`digit_length`)**：`int`，控制数值的小数位数或有效位数，取值范围：`0 - 3`。
* **间距控制 (`spacing_control`)**：`int`，数值与单位字符之间的空格数或间距。
* **单位字符 (`unit_string`)**：`wchar_t[16]`，功耗单位后缀自定义，默认为 `"W"`。
* **智能模式开关 (`smart_mode`)**：`bool`，启用后在接电时显示 CPU+GPU 功耗，用电时显示电池功率。

### 3.3 待确认的问题 (Open Questions)
1. **原生 MSR 读取的安全与权限限制**：直接在 Win32 用户态读取 CPU MSR 寄存器（如 `0x611 MSR_PKG_ENERGY_STATUS`）通常需要内核级驱动（Ring 0）。如果不分发数字签名驱动，是否可以通过 Windows 内部的 Performance Counter (PDH) 或 WMI 提供对普通用户权限更友好的获取方案？
2. **多 GPU 环境下的处理策略**：在核显+独显的双显卡笔记本中，如果独显进入省电休眠（D3 状态），频繁调用 NVML / ADL API 是否会强制唤醒独显导致电池续航崩溃？是否需要提供“屏蔽独显唤醒”的低功耗安全机制？
3. **电池功耗的原生 API 精度差异**：使用原生 `GetSystemPowerStatus` 无法获取具体的放电功率，而 WMI `MSBatteryClass` 或 IOCTL 读取的电池数据在不同厂商的 ACPI 驱动上实现程度不一。是否需要为不同机型建立平滑的 API 回退/适配链？
