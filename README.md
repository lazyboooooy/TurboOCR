# TurboOCR v1.1.0

<div align="center">

**Windows 离线 OCR 工具 — PP-OCRv6 + ONNX Runtime + DirectML**

无需 CUDA / cuDNN / Paddle 运行库，一份包覆盖 NVIDIA、AMD、Intel 显卡

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2B-lightgrey)]()

[English](README_EN.md)

</div>

---

## 本版本变化

v1.1.0 将推理后端从 **Paddle Inference + CUDA** 迁移到 **ONNX Runtime + DirectML**：

- 移除 `phi.dll`、`paddle_inference.dll`、CUDA/cuDNN、MKL 等约 2.8 GB 运行库
- 一份发行包可运行在 NVIDIA（Pascal 及更新）、AMD、Intel 核显等所有 DX12 显卡上
- 不再需要为不同 GPU 架构重新编译推理库
- 无可用 GPU 时自动回退 CPU（ONNX Runtime CPU EP）
- 当前测试包约 **317 MB**（含 7 个 ONNX 模型，模型合计约 175 MB）

选型与方案对比见 [ONNXRuntime迁移三方案对比.md](ONNXRuntime迁移三方案对比.md)，性能实测见 [BENCHMARK.md](BENCHMARK.md)。

## 特性

- **三档模型** — tiny / small / medium，覆盖速度与精度需求
- **DirectML GPU 加速** — 自动检测 DX12 显卡；不兼容时自动降级 CPU
- **离线运行** — 全部本地推理，无需联网
- **多模式 OCR**
  - 截图 OCR（全局热键）
  - 批量 OCR（图片 / PDF）
  - 后台 OCR（结果自动复制到剪贴板）
  - 剪贴板 OCR
- **PDF 支持** — 内置渲染，多页 OCR
- **智能排版** — 8 种解析器（单段、多段、代码块、竖排等）
- **HTTP API** — 内置 REST 服务，支持跨语言调用
- **历史记录** — SQLite 持久化，缩略图 + 原图 + 多页数据
- **二维码/条形码** — ZXing + OpenCV 预检
- **多语言 / 深色主题**

## 运行要求

| 项 | 要求 |
|---|---|
| 系统 | Windows 10 1903+ / Windows 11，x64 |
| 显卡（可选） | 支持 DirectX 12 的 GPU；无 GPU 时自动使用 CPU |
| 运行库 | 已随包携带 VC++ 运行时与 Qt/OpenCV/ONNX Runtime/DirectML DLL |

## 直接运行

从 Releases 下载 `TurboOCR-1.1.0-win64.zip`，解压后运行 `TurboOCR.exe`。

源码构建产物在 `build/Release/`；`dist/` 是本地打包目录，不提交到 Git。整个发布包可以直接复制到其他 Windows 电脑运行，不需要安装 CUDA、cuDNN、Paddle 或 Qt。

首次启动会在 exe 同级生成 `config.ini`、`log/`、`ocr_history.db`。设置页可切换 CPU / GPU、模型档位、线程数等。

## 从源码构建

### 环境要求

| 工具 | 版本 | 说明 |
|---|---|---|
| Visual Studio | 2019（v142） | 与本项目 3rdparty 预编译库一致 |
| CMake | 3.16+ | 构建系统 |
| Qt | 6.8.3 msvc2022_64 | GUI 框架 |
| Windows SDK | 10.x | 由 VS 安装 |

### 构建步骤

```bash
# 0. 下载构建依赖（3rdparty/ + models/，约 192 MB 压缩 / 336 MB 解压）
#    从 Releases 下载 deps-v1.1.0.zip 并解压到项目根目录
#    或运行：python setup_deps.py
python setup_deps.py

# 1. 配置
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -T v142 \
  -DCMAKE_PREFIX_PATH="F:/Develop/Qt6/6.8.3/msvc2022_64"

# 2. 编译
cmake --build build --config Release --parallel
```

产物在 `build/Release/`。CMake 会自动复制 Qt、OpenCV、ONNX Runtime、DirectML 和 VC++ 运行时 DLL。

> VC145 运行时已随 `3rdparty/vc_runtime/x64` 提供，CMake 会优先复制；如果删除该目录，则回退到本机最新 VS Redist（`VC_REDIST_CANDIDATES`）。

## 目录结构

```text
TurboOCRV1.1.0/
├── src/                          Qt 应用
│   ├── main.cpp                  入口、单实例锁、日志
│   ├── main_window.cpp           主窗口、托盘、热键、剪贴板、HTTP
│   ├── ocr/engine.cpp            OCR 引擎封装
│   ├── core/gpu_checker.cpp      DirectML / DX12 检测
│   ├── core/http_server.cpp      REST API
│   ├── core/ocr_history.cpp      SQLite 历史
│   ├── pages/                    主 OCR / 批量 / 设置 / 开发者页面
│   ├── postprocess/parsers.cpp   文本排版
│   └── output/formatters.cpp     导出格式
├── ocr_engine/                   PaddleOCR C++ 前后处理与 pipeline
│   └── src/common/static_infer.* ONNX Runtime 推理绑定（OrtInfer）
├── models/                       PP-OCRv6 ONNX 模型 + inference.yml
├── 3rdparty/                     OpenCV / ZXing / absl / yaml-cpp / ORT / DirectML 等
├── tools/convert_models.ps1      Paddle → ONNX 模型转换脚本
├── dist/TurboOCR-1.1.0/          可直接分发的运行目录
└── CMakeLists.txt
```

## 模型与转换

运行时不依赖 Paddle，模型目录只保留：

```text
models/<model_name>/
├── inference.onnx
└── inference.yml      # 预处理/后处理参数
```

如需重新生成 ONNX 模型：

```powershell
# 依赖（建议独立 venv）：
#   pip install paddlepaddle==3.1.0 -i https://www.paddlepaddle.org.cn/packages/stable/cpu/
#   pip install paddle2onnx==2.1.0 onnx==1.17.0
powershell -ExecutionPolicy Bypass -File tools/convert_models.ps1 `
  -PaddleModels "F:\QPrj\TurboOCR\models" `
  -OutputModels ".\models"
```

详细版本约束见 [tools/README.md](tools/README.md)。

## 推理后端

```text
Qt 应用
  └── OcrEngine
        └── PaddleOCR pipeline（前后处理、DB 检测、CTC 解码）
              └── OrtInfer（ocr_engine/src/common/static_infer.cc）
                    ├── device=gpu:0 → ONNX Runtime DirectML EP
                    └── device=cpu   → ONNX Runtime CPU EP
```

- DirectML 不可用或 Session 创建失败时，`OrtInfer` 自动回退 CPU EP。
- `gpu_checker` 通过 DXGI 枚举 DX12 适配器并检查 `DirectML.dll` 是否可加载。
- 不再链接 Paddle / CUDA / cuDNN / MKL。

## HTTP API

在设置页或 `config.ini` 中启用：

```ini
[http]
enabled=true
port=19527
lan_access=false
```

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/health` | 健康检查 |
| POST | `/api/ocr/image` | 请求体为图片二进制，返回 OCR JSON |
| POST | `/api/ocr/pdf` | 请求体为 PDF 二进制 |
| POST | `/api/ocr/qrcode` | 请求体为图片二进制，返回二维码/条码结果 |

示例：

```bash
curl -X POST --data-binary @demo/封面.png http://127.0.0.1:19527/api/ocr/image
```

## 已知限制

- DirectML 首次运行会有 PSO/着色器编译延迟，之后会使用驱动缓存。
- 动态 shape（检测图 H/W、识别行宽）会限制部分 GPU 优化；性能随显卡/驱动不同。
- 现有批量 OCR 仍由 `QMutex` 串行化推理请求（历史问题 A1），"并行"主要提升的是预处理/IO 重叠，不是推理吞吐。
- 暂无安装包与代码签名，首次下载可能触发 SmartScreen 提示。

## 第三方许可

- ONNX Runtime：MIT
- DirectML：Microsoft 许可（见 `3rdparty/directml/LICENSE.txt`）
- OpenCV：Apache 2.0
- Qt 6：LGPLv3 / 商业许可
- ZXing-C++：Apache 2.0
- PaddleOCR 前后处理代码：Apache 2.0

完整许可见 `LICENSE` 与各 `3rdparty/*/` 目录。
