#include "gpu_checker.h"
#include "logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#endif

GpuInfo GpuChecker::detect() {
    GpuInfo info;
#ifdef Q_OS_WIN
    HMODULE dml = LoadLibraryW(L"DirectML.dll");
    const bool dmlLoaded = dml != nullptr;
    if (dml) FreeLibrary(dml);

    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        for (UINT i = 0;; ++i) {
            IDXGIAdapter1* adapter = nullptr;
            if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
                break;

            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            const bool isHardware =
                (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;

            ID3D12Device* device = nullptr;
            const HRESULT hr = D3D12CreateDevice(
                adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
            if (device) device->Release();
            adapter->Release();

            if (isHardware && SUCCEEDED(hr)) {
                info.gpuName = QString::fromWCharArray(desc.Description);
                info.directMlOk = dmlLoaded;
                break;
            }
        }
        factory->Release();
    }

    TLOG_DEBUG() << "DirectML detect: dmlLoaded=" << dmlLoaded
                 << " gpu=" << info.gpuName;
#endif
    return info;
}

QString GpuInfo::summary() const {
    if (gpuName.isEmpty())
        return QStringLiteral("未检测到可用的 DX12 显卡");
    if (!directMlOk)
        return gpuName + QStringLiteral("\n⚠ DirectML.dll 不可用，GPU 推理不可用");
    return gpuName + QStringLiteral("\n✅ DirectML 可用");
}
