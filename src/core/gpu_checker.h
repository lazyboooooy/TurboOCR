#pragma once
#include <QString>

/// GPU 检测结果
struct GpuInfo {
    bool     hasNvidia   = false;   // 是否有 NVIDIA 显卡
    QString  gpuName;               // 显卡型号，如 "NVIDIA GeForce RTX 3060"
    int      driverMajor = 0;       // 驱动主版本
    int      driverMinor = 0;       // 驱动次版本
    int      cudaMajor   = 0;       // 驱动支持的 CUDA 主版本
    int      cudaMinor   = 0;       // 驱动支持的 CUDA 次版本
    bool     hasCudnn     = false;  // cuDNN 库是否可用
    bool     paddleCudaOk = false;  // 驱动 + cuDNN 是否兼容 Paddle Inference GPU 库

    QString  summary() const;       // 人类可读的摘要
};

/// GPU 检测器 — 检测 NVIDIA 显卡 + CUDA 兼容性
class GpuChecker {
public:
    /// 执行检测（通过 nvidia-smi + 注册表），返回结果
    static GpuInfo detect();

    /// Paddle Inference 3.x GPU 库所需的 CUDA 版本（CUDA 11.8 + cuDNN 8.6）
    static constexpr int kPaddleCudaMajor = 11;
    static constexpr int kPaddleCudaMinor = 8;

private:
    static GpuInfo detectBySmi();       // 通过 nvidia-smi
    static GpuInfo detectByRegistry();  // 通过注册表
    static GpuInfo detectByDll();       // 通过 nvcuda.dll 版本
};
