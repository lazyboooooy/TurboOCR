# 模型转换说明

TurboOCR v1.1.0 运行时只加载 ONNX 模型。`models/` 中每个模型目录需要：

```text
inference.onnx
inference.yml
```

`inference.yml` 来自 PP-OCRv6 官方 Paddle 模型，包含预处理/后处理参数；`inference.onnx` 由 Paddle 模型转换得到。

## 已验证的版本组合

| 组件 | 版本 | 说明 |
|---|---|---|
| Python | 3.10 | Anaconda 环境已验证 |
| paddlepaddle | **3.1.0（CPU）** | Paddle2ONNX 2.1.0 需要 >= 3.0.0.dev20250426 |
| paddle2onnx | **2.1.0** | Windows wheel |
| onnx | **1.17.0** | Paddle2ONNX 要求 <=1.17.0 |

安装：

```powershell
python -m pip install paddlepaddle==3.1.0 -i https://www.paddlepaddle.org.cn/packages/stable/cpu/
python -m pip install paddle2onnx==2.1.0 onnx==1.17.0
```

> 建议使用独立 venv，避免 Paddle 升级 numpy/protobuf 影响其他项目。

## 为什么固定 3.1.0

- Paddle2ONNX 2.1.0 的 Windows 版本明确拒绝 `paddlepaddle==3.0.0`（版本检查逻辑）。
- Paddle 3.4.0 的 PIR 符号与 Paddle2ONNX 2.1.0 不匹配，加载扩展时会报
  `DLL load failed ... procedure could not be found`。
- **Paddle 3.1.0 实测可用。**

## 转换

```powershell
powershell -ExecutionPolicy Bypass -File tools/convert_models.ps1 `
  -PaddleModels "F:\QPrj\TurboOCR\models" `
  -OutputModels ".\models"
```

脚本会遍历 `PaddleModels` 下所有含 `inference.json` + `inference.pdiparams` 的目录，复制 `inference.yml`，并生成 `inference.onnx`。

使用 `--optimize_tool None` 跳过 Polygraphy 常量折叠，避免 Windows 上 `ml_dtypes` 扩展缺失导致转换中断；运行时的图优化交给 ONNX Runtime 的 `ORT_ENABLE_ALL`。

## 转换后检查

```python
import onnx, onnxruntime as ort

m = onnx.load("models/PP-OCRv6_small_det_infer/inference.onnx")
onnx.checker.check_model(m)
for i in m.graph.input:
    print("input", i.name, i.type.tensor_type.shape)
for o in m.graph.output:
    print("output", o.name, o.type.tensor_type.shape)

sess = ort.InferenceSession(
    "models/PP-OCRv6_small_det_infer/inference.onnx",
    providers=["CPUExecutionProvider"])
print(sess.get_inputs()[0].shape)
```

预期：

| 模型 | 输入 | 输出 |
|---|---|---|
| det | `[N, 3, H, W]` 动态 | `[N, 1, H, W]` |
| rec | `[N, 3, 48, W]` 动态 | `[N, T, C]` |
| ori | `[N, 3, 80, 160]` | `[N, 2]` |

## 运行时如何选择模型

`src/ocr/engine.cpp` 根据 `model_size`（tiny/small/medium）推导：

```text
models/PP-OCRv6_<size>_det 或 PP-OCRv6_<size>_det_infer/
models/PP-OCRv6_<size>_rec 或 PP-OCRv6_<size>_rec_infer/
models/PP-LCNet_x1_0_textline_ori/
```

`OrtInfer` 加载每个目录下的 `inference.onnx`，预处理参数从同目录 `inference.yml` 读取。
