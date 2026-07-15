#include "gpu_checker.h"
#include "logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QProcess>
#include <QRegularExpression>
#include <QFileInfo>
#include <QSettings>
#include <QDir>
#include <QLibrary>

// ======================== 公开接口 ========================

/// 检查 cuDNN DLL 是否可加载
static bool checkCudnn() {
#ifdef Q_OS_WIN
    HMODULE h = LoadLibraryW(L"cudnn64_8.dll");
    if (h) { FreeLibrary(h); return true; }
    // 也尝试 cudnn64_9.dll 等
    h = LoadLibraryW(L"cudnn64_9.dll");
    if (h) { FreeLibrary(h); return true; }
    h = LoadLibraryW(L"cudnn64_7.dll");
    if (h) { FreeLibrary(h); return true; }
#endif
    return false;
}

GpuInfo GpuChecker::detect() {
    GpuInfo info = detectBySmi();
    if (!info.hasNvidia) {
        info = detectByRegistry();
    }
    if (!info.hasNvidia) {
        info = detectByDll();
    }

    if (info.hasNvidia) {
        info.hasCudnn = checkCudnn();
        info.paddleCudaOk = info.hasCudnn
            && ((info.cudaMajor > kPaddleCudaMajor)
             || (info.cudaMajor == kPaddleCudaMajor && info.cudaMinor >= kPaddleCudaMinor));
    }
    return info;
}

QString GpuInfo::summary() const {
    if (!hasNvidia)
        return QStringLiteral("未检测到 NVIDIA 显卡");
    QString s = gpuName;
    if (cudaMajor > 0)
        s += QString(" | CUDA %1.%2").arg(cudaMajor).arg(cudaMinor);
    if (!hasCudnn)
        s += QStringLiteral("\n⚠ 未找到 cuDNN 库 (cudnn64_8.dll)，GPU 推理不可用");
    s += paddleCudaOk
        ? QStringLiteral("\n✅ 兼容 Paddle GPU 库")
        : QString("\n⚠ 需要 CUDA ≥%1.%2 + cuDNN 8")
              .arg(GpuChecker::kPaddleCudaMajor).arg(GpuChecker::kPaddleCudaMinor);
    return s;
}

// ======================== nvidia-smi 检测 ========================

GpuInfo GpuChecker::detectBySmi() {
    GpuInfo info;
    QProcess proc;

    // 查找 nvidia-smi.exe
    QString smiPath = "nvidia-smi";
    QStringList searchPaths = {
        "C:/Windows/System32",
        "C:/Windows/SysWOW64",
        "C:/Program Files/NVIDIA Corporation/NVSMI",
    };
    for (auto& p : searchPaths) {
        QString candidate = p + "/nvidia-smi.exe";
        if (QFileInfo::exists(candidate)) {
            smiPath = candidate;
            break;
        }
    }

    proc.start(smiPath, {"--query-gpu=name,driver_version", "--format=csv,noheader"});
    if (!proc.waitForFinished(5000)) {
        TLOG_DEBUG() << "nvidia-smi timed out or not found";
        return info;
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (output.isEmpty()) return info;

    TLOG_DEBUG() << "nvidia-smi output:" << output;

    // 解析: "NVIDIA GeForce RTX 3060, 551.23"
    QStringList parts = output.split(',');
    if (parts.size() >= 2) {
        info.hasNvidia = true;
        info.gpuName = parts[0].trimmed();

        QString drvVer = parts[1].trimmed();
        QStringList verParts = drvVer.split('.');
        if (verParts.size() >= 2) {
            info.driverMajor = verParts[0].toInt();
            info.driverMinor = verParts[1].toInt();
        }

        // 驱动版本 → CUDA 版本映射
        // R550+ → CUDA 12.4, R545+ → 12.3, R535+ → 12.2, R525+ → 12.0
        // R515+ → 11.7, R470+ → 11.4, R450+ → 11.0
        int drv = info.driverMajor;
        if (drv >= 550)      { info.cudaMajor = 12; info.cudaMinor = 4; }
        else if (drv >= 545) { info.cudaMajor = 12; info.cudaMinor = 3; }
        else if (drv >= 535) { info.cudaMajor = 12; info.cudaMinor = 2; }
        else if (drv >= 525) { info.cudaMajor = 12; info.cudaMinor = 0; }
        else if (drv >= 515) { info.cudaMajor = 11; info.cudaMinor = 7; }
        else if (drv >= 470) { info.cudaMajor = 11; info.cudaMinor = 4; }
        else if (drv >= 450) { info.cudaMajor = 11; info.cudaMinor = 0; }
        else                 { info.cudaMajor = 10; info.cudaMinor = 0; }
    }

    return info;
}

// ======================== 注册表检测 ========================

GpuInfo GpuChecker::detectByRegistry() {
    GpuInfo info;
#ifdef Q_OS_WIN
    // HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}
    // 搜索子键中包含 "NVIDIA" 的项
    QSettings reg(
        "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}",
        QSettings::NativeFormat);

    QStringList groups = reg.childGroups();
    for (auto& g : groups) {
        reg.beginGroup(g);
        QString desc = reg.value("DriverDesc").toString();
        QString drvVer = reg.value("DriverVersion").toString();
        reg.endGroup();

        if (desc.contains("NVIDIA", Qt::CaseInsensitive)) {
            info.hasNvidia = true;
            info.gpuName = desc;

            // 格式：通常为 "31.0.15.5123" 或类似
            QStringList parts = drvVer.split('.');
            if (parts.size() >= 3) {
                info.driverMajor = parts[1].toInt();  // 第二段通常是主版本
                info.driverMinor = parts[2].toInt();
            }

            // 简易映射
            if (info.driverMajor >= 31)      { info.cudaMajor = 12; info.cudaMinor = 2; }
            else if (info.driverMajor >= 30) { info.cudaMajor = 11; info.cudaMinor = 7; }
            else                             { info.cudaMajor = 11; info.cudaMinor = 0; }

            break;
        }
    }
#endif
    return info;
}

// ======================== DLL 检测 ========================

GpuInfo GpuChecker::detectByDll() {
    GpuInfo info;
#ifdef Q_OS_WIN
    // 检查 nvcuda.dll（CUDA 运行时 DLL）是否存在
    HMODULE hCuda = LoadLibraryW(L"nvcuda.dll");
    if (hCuda) {
        info.hasNvidia = true;
        info.gpuName = "NVIDIA GPU (检测到 nvcuda.dll)";

        // 获取 DLL 版本
        wchar_t dllPath[MAX_PATH];
        GetModuleFileNameW(hCuda, dllPath, MAX_PATH);
        FreeLibrary(hCuda);

        DWORD handle = 0;
        DWORD size = GetFileVersionInfoSizeW(dllPath, &handle);
        if (size > 0) {
            std::vector<BYTE> buf(size);
            if (GetFileVersionInfoW(dllPath, 0, size, buf.data())) {
                VS_FIXEDFILEINFO* ffi = nullptr;
                UINT len = 0;
                if (VerQueryValueW(buf.data(), L"\\", (LPVOID*)&ffi, &len) && ffi) {
                    info.cudaMajor = HIWORD(ffi->dwFileVersionMS);
                    info.cudaMinor = LOWORD(ffi->dwFileVersionMS);
                }
            }
        }

        // 版本映射：nvcuda.dll 版本号通常对应 CUDA 版本
        // 例如 6.14.11.8000 → CUDA 11.8
        if (info.cudaMajor == 0) info.cudaMajor = 11;
    }
#endif
    return info;
}
