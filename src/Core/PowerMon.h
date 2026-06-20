#pragma once

#include "../Framework/ITMPlugin.h"
#include "PowerMonItem.h"
#include <vector>

class PowerMon : public ITMPlugin
{
private:
    static PowerMon m_instance;
    std::vector<PowerMonItem*> m_items;

    PowerMon();
    virtual ~PowerMon();

    // 禁用拷贝与赋值
    PowerMon(const PowerMon&) = delete;
    PowerMon& operator=(const PowerMon&) = delete;

public:
    static PowerMon* Instance();

    // ITMPlugin 接口覆写
    int GetAPIVersion() const override;
    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    void OnInitialize(ITrafficMonitor* pApp) override;
};
