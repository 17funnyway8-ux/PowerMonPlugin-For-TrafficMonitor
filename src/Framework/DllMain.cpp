#include "../Core/PowerMon.h"

// 唯一导出的获取插件实例的函数
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return PowerMon::Instance();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // 禁用 DLL_THREAD_ATTACH 和 DLL_THREAD_DETACH 通知，提升性能
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
