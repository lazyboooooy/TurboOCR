# ONNX Runtime 替代 Paddle Inference：三方案对比

> 文档日期：2026-11-02
> 基线来源：`F:\QPrj\TurboOCR\ROADMAP.md`（2026-11-01 全量审查）+ 2026-11-02 对 `build/Release/`、`3rdparty/paddle_inference/lib/`、`models/` 的实测
> 目的：为 TurboOCR v1.1.0 的推理后端选型提供决策依据
> 约定：标 **⚠️ 待实测** 的是推算或依赖外部发行包的数据，不要当成已验证结论

---

## 0. 结论摘要

| 方案 | 一句话定位 | 分发包估算 | 显卡兼容性 |
|---|---|---:|---|
| **A：ORT + CPU EP** | 最轻、最稳、纯 CPU | **~300–320 MB** | 无 CUDA / 无架构问题 |
| **B：ORT + DirectML EP** | 一份包覆盖 N/A/Intel 显卡 | **~330–350 MB** | **根治** per-arch 兼容问题 |
| **C：ORT + CUDA EP** | 保留 NVIDIA GPU 性能 | **~1.5–1.9 GB** | 缓解，不根治 |

**推荐顺序**：先做模型转换 POC → 实测 CPU 与 DirectML 性能 → 默认选 A 或 B；C 仅在「只服务 NVIDIA、GPU 性能是刚需」时考虑。

---

## 1. 现状基线（实测）

### 1.1 体积构成

`build/Release/` 实测总计 **3.2 GB**，其中推理栈约 **2.84–2.87 GB（89%）**：

| 组件 | 实测体积 | 说明 |
|---|---:|---|
| `phi.dll` | 1.3 GB | 多架构重编产物，指南记录为 1.36 GB |
| CUDA / cuDNN / cuBLAS | 1.33 GB | `cudnn_cnn` 549 MB + `cublasLt` 519 MB + `cudnn_adv` 120 MB + `cudnn_ops` 86 MB + `cublas` 85 MB |
| `paddle_inference.dll` | 100 MB | |
| `mklml` + `mkldnn` + `libiomp5md` | 137 MB | |
| OpenCV | 72 MB | `opencv_world` 54 MB + `opencv_ffmpeg` 18 MB（ffmpeg 实际用不到） |
| 模型（7 个目录） | 177 MB | medium det 60 / medium rec 74 / small det 9.6 / small rec 21 / tiny det 1.9 / tiny rec 4.4 / ori 6.6 |
| Qt6 | ~30 MB | |
| `TurboOCR.exe` | 2.5 MB | |
| **合计** | **3.2 GB** | |

> 关键事实：体积问题**完全来自推理栈**，不是应用代码。`phi.dll` 之所以达到 1.3 GB，正是上次为修复 Pascal 显卡兼容而做「6 架构重编」的直接代价。

### 1.2 显卡兼容问题回顾

来自 `F:\QPrj\TurboOCR\Paddle_Inference_GPU多架构编译指南.md`：

- 预编译 Paddle Inference 使用 `CUDA_ARCH_NAME=Auto`，在 RTX 3060（sm86）上编译，`phi.dll` **只含 sm86 的 CUDA kernel**。
- GTX 1050/1060（Pascal，sm61）找不到对应 kernel → GPU 推理失败、CPU 正常。
- 修法：从 Paddle 源码重编 `phi.dll`，指定 `CUDA_ARCH_NAME=All` 生成 sm60/61/70/75/80/86 六种架构。
- 代价：
  - `phi.dll` 从 297 MB 膨胀到 **1.36 GB**；
  - 编译耗时 **2–4 小时**（817 个 CUDA 文件 × 6 架构）；
  - 还要修 CUDA 11.8 的 `half` 二义性、sm52 不支持 fp16 等源码问题。

### 1.3 迁移耦合面

```
pipeline.cc / processors.h          （保留：前后处理纯 OpenCV，框架无关）
          │
          │  Apply(const vector<cv::Mat>&) -> vector<cv::Mat>
          ▼
static_infer.cc                     （替换：PaddleInfer → OrtInfer）
          │
          ├── A：CPU Execution Provider
          ├── B：DirectML Execution Provider
          └── C：CUDA Execution Provider
```

- 直接接触 Paddle API 的只有 `ocr_engine/src/common/static_infer.cc`（201 行）：
  - `static_infer.h:23` `#include "paddle_inference_api.h"`
  - `static_infer.cc:129` `paddle_infer::CreatePredictor`
  - `static_infer.cc:38` `GetInputHandle`
  - `static_infer.cc:150` `Run()`
- DB 检测后处理、框扩展、CTCLabelDecode、方向分类都在 `common/processors.h`，**纯 OpenCV，迁移时不用动**。
- 因此换后端 = 新增 `OrtInfer` 实现相同 `Apply(...)` 签名 + 改选项映射 + 改 CMake 链接 + 改 `gpu_checker`。

---

## 2. 三个候选方案

### 方案 A：ORT + CPU Execution Provider

- 保留：应用层、OCR pipeline、OpenCV 前后处理、Qt UI。
- 移除：`phi.dll`、`paddle_inference.dll`、整套 CUDA/cuDNN、`mklml`/`mkldnn`/`libiomp5md`。
- 新增：ORT CPU 运行时（`onnxruntime.dll`，约 15–20 MB ⚠️ 待实测）。
- 推理设备：仅 CPU。
- 显卡兼容：无 GPU 路径，架构问题不存在。

### 方案 B：ORT + DirectML Execution Provider

- 移除：同方案 A。
- 新增：ORT 运行时 + `DirectML.dll`（约 25 MB ⚠️ 待实测），合计约 45 MB。
- 推理设备：DX12 显卡（NVIDIA / AMD / Intel 核显）；无可用显卡时回退 CPU EP。
- 显卡兼容：DirectML 不按 GPU 架构打包机器码，兼容问题在发行层面消除。
- 系统要求：Windows 10 1903+ / 支持 DX12 的显卡与驱动（Qt6 本身已要求 Win10，通常不构成额外门槛）。

### 方案 C：ORT + CUDA Execution Provider

- 移除：`phi.dll`、`paddle_inference.dll`、`mklml`/`mkldnn`/`libiomp5md`。
- 保留/升级：CUDA Runtime + cuDNN（ORT 版本对 CUDA/cuDNN 版本有严格对应关系）。
- 新增：ORT GPU 运行时（`onnxruntime_providers_cuda.dll` 等，约 100–200 MB ⚠️ 待实测）。
- 推理设备：仅 NVIDIA GPU，无 GPU 时回退 CPU EP。
- 显卡兼容：仍是 per-arch 预编译 kernel 模式，需确认官方包是否覆盖 sm61；未覆盖时需自行重编 ORT。

---

## 3. 横向对比总表

| 维度 | A：CPU EP | B：DirectML EP | C：CUDA EP |
|---|---|---|---|
| **推理栈体积** | ~15–20 MB | ~45 MB | ~1.3–1.7 GB（含 CUDA/cuDNN） |
| **分发包总体积** | **~300–320 MB** | **~330–350 MB** | **~1.5–1.9 GB** |
| **相对现状 3.2 GB** | 约 **−90%** | 约 **−89%** | 约 **−40%～−53%** |
| **GPU 加速** | 无 | 有（DX12） | 有（CUDA） |
| **支持硬件** | 任意 x64 CPU | N 卡 / A 卡 / Intel 核显 | 仅 NVIDIA |
| **根治 sm61 等架构兼容问题** | 不涉及 GPU，天然无问题 | **是**（不按架构分发 kernel） | **否**（同类问题仍在，只是更轻） |
| **需要 CUDA / cuDNN** | 否 | 否 | 是 |
| **版本耦合** | 无 | 依赖 Windows/DX12/DirectML 版本 | ORT 版本 ↔ CUDA ↔ cuDNN 强绑定 |
| **性能预期** | CPU 推理；MLAS vs Paddle 的 MKL-DNN，需实测 | 未知，需实测；首跑有 shader 编译延迟 | 接近 Paddle CUDA，较成熟 |
| **部署复杂度** | 最低（一个 ORT DLL） | 低（ORT + DirectML.dll） | 高（CUDA/cuDNN 全套） |
| **开发工作量** | 较小 | 中等 | 中等偏大（版本对齐） |
| **主要风险** | CPU 速度不够 | DirectML 算子覆盖 / 性能 / 首跑延迟 | 体积收益小、版本矩阵复杂 |
| **适用场景** | 便携绿包、无独显、追求稳定 | 一份包通吃所有显卡、体积敏感 | NVIDIA-only、GPU 性能优先 |

> ⚠️ 表中 ORT 与 DirectML 的体积为官方发行包的典型值，**本项目尚未实测**；以 POC 阶段实际拷贝的 DLL 为准。

---

## 4. 各方案优缺点详解

### 4.1 方案 A：ORT + CPU EP

**优点**

1. **体积最小、最可控**：直接砍掉 2.87 GB 推理栈，总体积到 300 MB 级。
2. **部署最简单**：只需要 `onnxruntime.dll`，没有 CUDA、cuDNN、显卡驱动版本要求。
3. **稳定性最好**：不涉及 GPU 初始化、显存、驱动兼容，灰度/便携场景问题最少。
4. **彻底摆脱多架构编译维护**：`Paddle_Inference_GPU多架构编译指南.md` 中的 6 架构重编、源码补丁、2–4 小时编译全部不再需要。
5. **代码改动最小**：`OrtInfer` 只需实现 CPU EP；`gpu_checker` 可以移除或强制 CPU；无需处理 EP 回退与显存。

**缺点**

1. **失去 GPU 加速**：批量 OCR、高分辨率图片、medium 模型场景速度会明显下降（具体倍数需实测）。
2. **CPU 性能不保证优于 Paddle**：Paddle CPU 路径使用 MKL-DNN/oneDNN，ORT 默认使用 MLAS；两者在不同 CPU（是否支持 AVX-512/VNNI）上表现不同，必须用真实图片集对比。
3. **产品能力缩水**：设置页现有的「设备：CPU/GPU」选项在 CPU-only 包中失去意义，需要调整 UI 或拆双发行包。
4. **无法利用 DirectML 的跨厂商优势**。

**适用**：便携绿色版、低配机器、以稳定性优先的默认发行包；或作为「CPU 版」与 B 方案并存。

**不适用**：用户明确需要 GPU 加速且批量吞吐是核心卖点的场景。

---

### 4.2 方案 B：ORT + DirectML EP

**优点**

1. **一份发行包覆盖所有 DX12 显卡**：NVIDIA（Pascal 到最新）、AMD、Intel 核显统一走 DirectML，无需为不同厂商/架构出包。
2. **根治本次的架构兼容问题**：DirectML 发行包中不含按 GPU 架构划分的 kernel，最终代码生成由 DirectML/显卡驱动在运行时完成；不会再出现「phi.dll 只有 sm86，Pascal 跑不了」这类问题。
3. **体积仍然很小**：约 330–350 MB，仅比纯 CPU 版多约 25 MB 的 `DirectML.dll`。
4. **不需要 CUDA/cuDNN**：部署不依赖 CUDA Toolkit、cuDNN、NVCC，也不需要维护 6 架构编译。
5. **可回退 CPU**：ORT 在 DirectML 不可用或算子不支持时可回退 CPU EP，单一包具备兜底能力。
6. **顺带扩大硬件覆盖**：A 卡和 Intel 核显用户获得 GPU 加速，这是当前 Paddle 方案做不到的。

**缺点 / 待验证**

1. **性能不确定**：DirectML 在 PP-OCR 这类卷积 + 动态 shape 模型上的表现需要实测；可能低于原生 CUDA，也可能在消费级卡上接近。
2. **首跑延迟**：DirectML 首次运行时需要编译/准备 GPU 代码，第一次推理可能明显变慢；需要评估是否做启动预热。
3. **算子覆盖与回退**：DB 检测、CRNN 识别、方向分类属于常见算子，理论上支持良好；但动态 shape、某些后处理算子可能回退 CPU，导致「部分图快、部分图慢」。⚠️ 需 POC 验证。
4. **系统门槛**：需 Win10 1903+ / DX12；老驱动可能需要升级。Qt6 本身已要求 Win10，所以通常不是新增门槛。
5. **fp16 / 精度**：DirectML 对 fp16 的支持与模型 dtype 相关，性能与精度需要一起测。
6. **多显卡/集显切换**：笔记本双显卡环境下的适配器选择逻辑需要额外处理（`gpu_checker` 要重写为枚举 DX12 适配器）。

**适用**：希望「一个包拷到任何 Windows 电脑都能用」的发行目标；需要同时覆盖 NVIDIA 老卡、AMD、核显；体积敏感。

**不适用**：对 GPU 推理延迟极其敏感、且只服务高端 NVIDIA 卡的场景（此时 C 或保留 Paddle CUDA 更合适）。

---

### 4.3 方案 C：ORT + CUDA EP

**优点**

1. **保留 NVIDIA GPU 性能**：迁移后推理路径仍是 CUDA，性能预期最接近现有 Paddle CUDA 方案。
2. **生态成熟**：ORT CUDA EP 文档、算子支持、调试工具链完善。
3. **代码迁移直接**：`OrtInfer` 的 CUDA EP 配置与 A/B 共用大部分代码，只是 SessionOptions 不同。
4. **仍然能砍掉最大单体**：`phi.dll`（1.3 GB）、`paddle_inference.dll`（100 MB）、MKL 三件套（137 MB）都能移除。

**缺点**

1. **体积收益有限**：仍需携带 CUDA/cuDNN（约 1.2–1.5 GB），总包只能降到 1.5–1.9 GB，相比 3.2 GB 只减一半左右。
2. **没有根治架构兼容**：ORT 的 CUDA EP 同样包含按架构编译的 kernel；官方包是否覆盖 sm61 必须确认，未覆盖时仍需自行重编（虽然比 Paddle 轻得多）。
3. **版本矩阵严格**：ORT 版本与 CUDA/cuDNN 版本强绑定（例如 ORT 1.17 对应 CUDA 11.8/cuDNN 8，较新版本转向 CUDA 12/cuDNN 9；以官方矩阵为准），选错版本直接起不来。
4. **部署复杂度高**：仍需携带 CUDA Runtime、cuDNN 多个 DLL，安装包体积和杀软误报面都更大。
5. **只服务 NVIDIA**：A 卡/核显用户仍只能走 CPU。
6. **维护成本延续**：显卡架构、驱动、CUDA 版本的兼容问题依然要持续跟进，只是把 Paddle 的坑换成了 ORT 的坑。

**适用**：产品明确只面向 NVIDIA 用户，且 GPU 推理性能/延迟是核心竞争力；或作为「GPU 增强包」与 A 方案并存。

**不适用**：以「减小分发包体积」或「一份包兼容所有显卡」为主要目标的项目。

---

## 5. 三个方案的共同前提与共同改动

无论选 A/B/C，以下内容都要做，且与 EP 选择无关。

### 5.1 决定性前提：Paddle → ONNX 模型转换 POC

这是整个迁移的**唯一实质性风险**，不过则方案终止：

1. 用 Paddle2ONNX 转换：
   - `PP-OCRv6_small_det` / `PP-OCRv6_small_rec`
   - `PP-LCNet_x1_0_textline_ori`
   - 若目标发行版使用 medium/tiny，也要各转一份
2. 确认：
   - DB 检测模型的**动态 H/W shape** 是否保留；
   - 识别模型的**动态宽度**是否保留；
   - 批维度是否可动态（影响批量推理）；
   - ONNX opset 与 ORT 版本匹配；
   - 是否有 Paddle 自定义算子无法映射。
3. 用同一批图片对比 Paddle 与 ORT 的输出：文本框位置、识别文本、置信度。
4. ⚠️ 若 PP-OCRv6 没有官方 ONNX 导出，Paddle2ONNX 对动态输入 DB 模型可能需要手工处理；这一步失败则 A/B/C 全部作废。

### 5.2 代码改动

| 改动 | 内容 |
|---|---|
| `static_infer.cc/.h` | 新增 `OrtInfer`，实现 `Apply(const vector<cv::Mat>&) -> vector<cv::Mat>`；处理输入名、输出名、动态 shape、NCHW 转换 |
| `base_predictor.cc` | 选项映射改为 ORT SessionOptions：`intra_op_num_threads`、`execution_mode`、EP 选择 |
| `ocr/engine.cpp:81-95` | `paddlex_config` 内嵌映射不再适用，重写模型加载与配置解析 |
| `core/gpu_checker.cpp` | A：移除/强制 CPU；B：改为枚举 DX12 适配器；C：改为检查 ORT CUDA EP 可用性 |
| `CMakeLists.txt` | 移除 paddle/protobuf/glog/gflags/absl/mkl/onednn 链接；加入 `onnxruntime.lib`；按方案拷贝对应 DLL |
| 部署脚本 | A：`onnxruntime.dll`；B：+ `DirectML.dll`；C：+ CUDA/cuDNN 全套 |

### 5.3 不会因换框架自动解决的问题

- **A1「假并行」仍在应用层**：`ocr/engine.cpp:145` 的 `QMutexLocker` 仍会串行化请求。ORT 的 `InferenceSession::Run` 本身支持并发调用，但必须改掉应用层互斥/改批量接口才能真正并行。
- **A4「PNG 中转」仍在应用层**：`ocr/engine.cpp:182-192` 的 `cv::imwrite` + 相对路径问题与推理框架无关，应同步修掉。
- **精度回归必须做**：ORT（MLAS/DirectML/CUDA）与 Paddle（MKL-DNN）数值实现不同，检测框通常有 1–2 px 偏移，识别文本可能有极小概率差异，需要用固定样本集回归。

### 5.4 可选叠加收益

- **INT8 量化**：模型 177 MB → 约 50 MB（⚠️ 待实测），可与 A/B/C 任意方案叠加。
- **OpenCV 裁剪**：`opencv_world` 54 MB + 用不到的 ffmpeg 18 MB → 自编 static 版约 15 MB。
- 叠加后：
  - A：估算 **~120–150 MB**；
  - B：估算 **~150–180 MB**；
  - 均为推算，需实测。

---

## 6. 工作量拆解

基于 `ROADMAP.md` §4.4，按方案差异调整。以下为**人天估算**，不含集成测试与打包发布：

| 步骤 | 内容 | 预估 | A | B | C |
|---|---|---:|---|---|---|
| 1 | 模型转换 POC（决定性） | 0.5–3 天 | 相同 | 相同 | 相同 |
| 2 | 实现 `OrtInfer`（tensor 名、动态 shape、EP 配置） | 3–5 天 | 最简单 | +DirectML 适配 | +CUDA 版本对齐 |
| 3 | 选项映射（device / cpu_threads / 旧 mkldnn 选项废弃） | 0.5 天 | 相同 | 相同 | 相同 |
| 4 | 重写 `gpu_checker.cpp` | 1 天 | 可删除 | 枚举 DX12 适配器 | 检查 ORT CUDA EP |
| 5 | CMake / 依赖链接 / DLL 拷贝 | 0.5 天 | 相同 | +DirectML.dll | +CUDA/cuDNN |
| 6 | 精度回归（Paddle vs ORT） | 2–3 天 | 相同 | 相同 | 相同 |
| 7 | 打包与目标机验证 | 0.5–1 天 | 最简单 | 中等 | 最复杂 |
| **合计** | | | **~7–10 天** | **~8–11 天** | **~8–12 天** |

> 估算口径：单人、熟悉现有代码、POC 一次通过。若模型转换反复，整体顺延。

---

## 7. 选型建议

### 7.1 决策树

```
是否必须支持非 NVIDIA 显卡（A 卡 / Intel 核显）？
├─ 是 → 方案 B（DirectML）
└─ 否 → 是否必须保留 NVIDIA GPU 加速且性能优先？
        ├─ 是 → 方案 C（CUDA EP）；但需接受 ~1.5–1.9 GB 体积
        └─ 否 → 方案 A（CPU EP）
```

### 7.2 按产品目标选

| 产品目标 | 推荐 | 理由 |
|---|---|---|
| 「绿色包拷到任何电脑都能跑」 | **A** | 300 MB 级、零 GPU 依赖、问题最少 |
| 「一份包通吃 N/A/Intel，体积敏感」 | **B** | 330–350 MB，根治架构兼容，硬件覆盖最广 |
| 「只服务 N 卡，GPU 性能第一」 | **C**（或暂不迁移） | 体积只减一半；若仅为体积，迁 C 不划算 |
| 「先拿即时收益，不换框架」 | 不迁移，做零风险瘦身 | 拆 CPU/GPU 双包 + INT8 + 裁 OpenCV，约 500 MB；但显卡兼容仍靠自编 6-arch phi |

### 7.3 推荐落地方式

1. **先做 POC**（0.5–3 天）：转换 det/rec/ori 模型，跑通 ORT CPU；这是所有方案的闸门。
2. **同一套样本测两组数据**：
   - CPU EP 在目标最低配机器上的耗时；
   - DirectML EP 在 GTX 1060（sm61）和一台核显机器上的耗时、首跑延迟、回退情况。
3. **若 CPU 速度可接受** → 直接选 A，最快落地、风险最低。
4. **若 CPU 不达标但 DirectML 可用** → 选 B，一份包覆盖所有显卡。
5. **只有 DirectML 性能不达标且产品明确 NVIDIA-only** → 才考虑 C；否则建议保留现有 Paddle CUDA 包，仅做体积瘦身。
6. 无论选哪个，同步修掉 A1（假并行）和 A4（PNG 中转），否则换框架后的性能收益会被应用层抵消。

---

## 8. 待实测 / 待确认事项

1. ⚠️ **Paddle2ONNX 对 PP-OCRv6 的转换成功率**：det/rec/ori 是否都能无损导出；动态 shape 是否保留。
2. ⚠️ **ORT 各发行包的真实体积**：CPU 版、DirectML 版、CUDA 版的 DLL 实际大小与依赖文件清单。
3. ⚠️ **CPU EP vs Paddle CPU（MKL-DNN）的实测速度**：在目标最低配机器、medium 模型、批量 100 张场景下对比。
4. ⚠️ **DirectML 在 GTX 1060 / 核显上的性能与首跑延迟**；是否存在算子回退导致的部分图片变慢。
5. ⚠️ **ORT CUDA EP 官方包是否覆盖 sm61**；以及所选 ORT 版本对应的 CUDA/cuDNN 版本矩阵。
6. ⚠️ **INT8 量化后的精度损失**：识别准确率、检测框偏移是否可接受。
7. **发行目标**：是否接受 Win10 1903+（DirectML 要求）；是否必须支持 Win7（Qt6 已不支持）。
8. **是否保留 GPU 设备选项**：若选 A，设置页的 CPU/GPU 切换如何处理；是否发布 CPU/GPU 双包。
9. **批量 OCR 的并行策略**：换 ORT 后是采用「单 session 批量 Run」还是「多 session 并行」，需与 A1 一起设计。

---

## 9. POC 验收清单

- [ ] `PP-OCRv6_small_det` 转 ONNX 成功，动态 H/W 保留
- [ ] `PP-OCRv6_small_rec` 转 ONNX 成功，动态宽度保留
- [ ] `PP-LCNet_x1_0_textline_ori` 转 ONNX 成功
- [ ] ORT CPU 跑通 20 张样本，无 unsupported op
- [ ] 与 Paddle 基线对比：文本框位置偏差 ≤ 2 px，识别文本一致率 ≥ 99%（⚠️ 阈值可调整）
- [ ] DirectML 在 GTX 1060（sm61）上跑通，记录首跑延迟与稳定耗时
- [ ] DirectML 在 Intel/AMD 任一设备上跑通（如有条件）
- [ ] 记录三种 EP 的实际 DLL 体积与总包体积
- [ ] 记录 CPU / DirectML / CUDA 三组吞吐数据（单张 + 批量 100 张）
- [ ] 确认模型 batch 维度是否可动态；若不可，明确批量策略
- [ ] 输出 POC 报告，给出 A/B/C 的最终选型建议

---

## 10. 关键文件速查

| 想改什么 | 打开 |
|---|---|
| 推理绑定（替换点） | `ocr_engine/src/common/static_infer.cc` / `.h` |
| 前后处理（保留） | `ocr_engine/src/common/processors.h` |
| OCR 串联流程 | `ocr_engine/src/pipelines/ocr/pipeline.cc` |
| 引擎参数 / 设备选项 | `ocr_engine/src/pipelines/ocr/pipeline.h:61-97` |
| 应用层引擎封装（QMutex、PNG 中转） | `src/ocr/engine.cpp:135-192` |
| GPU 探测 | `src/core/gpu_checker.cpp` |
| 模型路径推导 | `src/main_window.cpp:293-302`、`src/pages/settings_page.cpp:247` |
| 构建配置 | `CMakeLists.txt` |
| 体积与架构基线 | `F:\QPrj\TurboOCR\ROADMAP.md` §0、§4 |
| 上次显卡兼容问题 | `F:\QPrj\TurboOCR\Paddle_Inference_GPU多架构编译指南.md` |

---

## 11. 结论

1. **如果目标只是把 3.2 GB 压到 300 MB 级**：A 和 B 都能做到，且都能顺手消灭 `phi.dll` 多架构编译这个负担；C 做不到（只能到 1.5–1.9 GB）。
2. **如果目标是同时解决「老 N 卡跑不了」和「包太大」**：只有 **B（ORT + DirectML）** 能在分发层面根治 per-arch 兼容问题，并覆盖 A 卡/核显。
3. **如果只服务 NVIDIA 且 GPU 性能不可妥协**：C 可行，但迁移的主要收益是摆脱 Paddle，而不是体积；需接受 CUDA/cuDNN 版本矩阵的持续维护。
4. **推荐路径**：先花 0.5–3 天做 Paddle2ONNX POC；POC 通过后，用同一批图片实测 CPU 与 DirectML，按 §7 的决策树二选一。POC 不通过则整个 ORT 方案终止，退回「Paddle 零风险瘦身」。

---

## 附录：v1.1.0 实测结果（2026-11-02）

- **实施路线**：方案 B（ORT + DirectML EP + CPU fallback）。
- **分发包**：`dist/TurboOCR-1.1.0` 实测 **317 MB**（移除未使用的 `opencv_ffmpeg` 后）；其中模型 175 MB，ORT + DirectML 约 34 MB。
- **运行库**：输出目录无 `phi.dll`、`paddle_inference.dll`、CUDA/cuDNN、MKL；仅 `onnxruntime.dll`、`onnxruntime_providers_shared.dll`、`DirectML.dll`。
- **测试机器**：Windows 11 + RTX 3060（12 GB）+ i7-11800H，medium 模型，`demo/封面.png`，2 次预热 + 10 次计时（详见 `BENCHMARK.md`）：
  - 旧版 Paddle CUDA：engine **117.6 ms** / HTTP **353.9 ms**
  - 新版 ORT DirectML：engine **183.8 ms** / HTTP **423.8 ms**；首次冷启动约 **3.9 s**（PSO 编译）
  - CPU（20 次中位数）：旧版 Paddle 8 线程 **2087 ms / 11.86 核**；新版 ORT 8 线程 **2351 ms / 7.34 核**；新版 ORT 16 线程 **2147 ms / 13.24 核**，与旧版基本持平
  - 新版工作集 **526 MB**（旧 841 MB）；GPU 显存增量 **701 MB**（旧 466 MB）；CPU 模式私有内存 663/658 MB（旧 1195/1206 MB）
- **结论**：B 方案实测可行；在 N 卡上比 Paddle CUDA 慢约 20% 端到端，但包体 317 MB、进程内存更低，并覆盖 AMD/Intel。CPU 模式默认 8 线程旧版快约 12.6% 但多用约 38% CPU；新版 16 线程基本追平且内存低约 45%。若只追求 N 卡速度，应保留 Paddle CUDA 或 ORT CUDA/TensorRT。方案 C 的体积收益（~1.5–1.9 GB）在 B 的 317 MB 面前不具吸引力。
