# TurboOCR v1.1.0

<div align="center">

**Offline OCR for Windows — PP-OCRv6 + ONNX Runtime + DirectML**

No CUDA / cuDNN / Paddle runtime required; one package works on NVIDIA, AMD and Intel GPUs

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2B-lightgrey)]()

[中文](README.md)

</div>

---

## What's new in v1.1.0

The inference backend moved from **Paddle Inference + CUDA** to **ONNX Runtime + DirectML**:

- Removed ~2.8 GB of Paddle/CUDA/cuDNN/MKL runtime
- One package runs on all DirectX 12 GPUs: NVIDIA (Pascal+), AMD, Intel iGPU
- No per-GPU-architecture rebuild of the inference library
- Automatic CPU fallback when DirectML is unavailable
- Current portable package: **~317 MB** (including 7 ONNX models, ~175 MB)

See [ONNXRuntime迁移三方案对比.md](ONNXRuntime迁移三方案对比.md) for the design comparison (Chinese).

## Performance (v1.1.0 vs v0.3.0)

Windows 11, i7-11800H (8C/16T), 32 GB RAM, RTX 3060 12 GB; PP-OCRv6 **medium**; `demo/封面.png` (7 lines). GPU: 10-run median; CPU: 20-run median.

| Scenario | Backend | engine | HTTP | CPU cores | Private | GPU mem |
|---|---|---:|---:|---:|---:|---:|
| GPU | Paddle CUDA (old) | **117.6 ms** | **353.9 ms** | 0.28 | 1435 MB | +466 MB |
| GPU | ORT DirectML (new) | 183.8 ms | 423.8 ms | 0.29 | **1144 MB** | +701 MB |
| CPU 8T | Paddle MKL-DNN (old) | **2087 ms** | **2359 ms** | 11.86 | 1195 MB | — |
| CPU 8T | ORT CPU (new) | 2351 ms | 2591 ms | **7.34** | **663 MB** | — |
| CPU 16T | Paddle MKL-DNN (old) | 2239 ms | 2548 ms | 11.61 | 1206 MB | — |
| CPU 16T | ORT CPU (new) | **2147 ms** | **2411 ms** | 13.24 | **658 MB** | — |

**Takeaways:**

- **NVIDIA GPU**: Paddle CUDA is still ~**20%** faster end-to-end; DirectML wins on hardware coverage, package size (**3.2 GB → 317 MB**) and process memory.
- **CPU, 8 threads**: old Paddle is ~**12.6%** faster but uses ~**38%** more CPU; the new ORT path is lighter.
- **CPU, 16 threads**: ORT is roughly equal (~4% faster, within noise) with ~**45%** lower private memory.

Full methodology, raw JSON and variance notes: [BENCHMARK.md](BENCHMARK.md).

## Features

- Three model tiers: tiny / small / medium
- DirectML GPU acceleration with CPU fallback
- Fully offline
- Screenshot OCR, batch OCR (images/PDF), background OCR, clipboard OCR
- PDF rendering and multi-page OCR
- 8 text layout parsers
- Built-in HTTP API
- SQLite history
- QR / barcode scanning
- Themes and i18n

## Requirements

| Item | Requirement |
|---|---|
| OS | Windows 10 1903+ / Windows 11, x64 |
| GPU (optional) | DirectX 12 capable GPU; CPU fallback otherwise |
| Runtime | VC++ runtime, Qt, OpenCV, ONNX Runtime, DirectML are bundled |

## Run

Download `TurboOCR-1.1.0-win64.zip` from Releases and run `TurboOCR.exe`.

The build output is `build/Release/`; `dist/` is a local packaging folder and is not committed. The release package is portable and can be copied to another Windows PC. No CUDA, cuDNN, Paddle or Qt installation is required.

## Build from source

| Tool | Version |
|---|---|
| Visual Studio | 2019 (v142) |
| CMake | 3.16+ |
| Qt | 6.8.3 msvc2022_64 |
| Windows SDK | 10.x |

```bash
# 0. Download build dependencies (3rdparty/ + models/, ~192 MB zip / 336 MB extracted)
#    Get deps-v1.1.0.zip from Releases and extract it to the project root,
#    or run: python setup_deps.py
python setup_deps.py

# 1. Configure
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 -T v142 \
  -DCMAKE_PREFIX_PATH="F:/Develop/Qt6/6.8.3/msvc2022_64"

# 2. Build
cmake --build build --config Release --parallel
```

Output: `build/Release/`.

## Models

Runtime models are ONNX:

```text
models/<model_name>/
├── inference.onnx
└── inference.yml
```

Regenerate with `tools/convert_models.ps1`. See [tools/README.md](tools/README.md) for the required Paddle / Paddle2ONNX versions.

## Inference backend

```text
Qt app → OcrEngine → PaddleOCR pipeline (pre/post-processing)
        → OrtInfer (ocr_engine/src/common/static_infer.cc)
             ├── device=gpu:0 → ONNX Runtime DirectML EP
             └── device=cpu   → ONNX Runtime CPU EP
```

`OrtInfer` automatically falls back to CPU if DirectML session creation fails.

## HTTP API

Enable in Settings or `config.ini`:

```ini
[http]
enabled=true
port=19527
lan_access=false
```

| Method | Path | Description |
|---|---|---|
| GET | `/api/health` | Health check |
| POST | `/api/ocr/image` | Raw image body → OCR JSON |
| POST | `/api/ocr/pdf` | Raw PDF body |
| POST | `/api/ocr/qrcode` | Raw image body → QR/barcode result |

## Known limitations

- First DirectML run pays shader/PSO compilation latency; later runs use driver cache.
- Dynamic shapes (detection H/W, recognition width) limit some GPU optimizations.
- Batch OCR still serializes inference with a `QMutex` (historical issue A1); parallelism mainly overlaps pre/post-processing.
- No installer or code signing yet.

## Third-party licenses

- ONNX Runtime: MIT
- DirectML: Microsoft license (`3rdparty/directml/LICENSE.txt`)
- OpenCV: Apache 2.0
- Qt 6: LGPLv3 / commercial
- ZXing-C++: Apache 2.0
- PaddleOCR pre/post-processing: Apache 2.0
