#pragma once
#include <QString>

/// DirectML GPU 检测结果
struct GpuInfo {
    bool    directMlOk = false;   // DirectML.dll 可加载且存在可用的 DX12 显卡
    QString gpuName;              // 显卡型号，如 "NVIDIA GeForce RTX 3060"

    QString summary() const;      // 人类可读的摘要
};

/// GPU 检测器 — 检测 DirectML / DX12 可用性
class GpuChecker {
public:
    static GpuInfo detect();
};
