# Convert PaddlePaddle PP-OCRv6 models to ONNX for TurboOCR v1.1.0.
#
# Prerequisites (recommended in a dedicated venv):
#   python -m pip install paddlepaddle==3.1.0 -i https://www.paddlepaddle.org.cn/packages/stable/cpu/
#   python -m pip install paddle2onnx==2.1.0 onnx==1.17.0
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools/convert_models.ps1 `
#     -PaddleModels "F:\QPrj\TurboOCR\models" `
#     -OutputModels ".\models"

param(
    [string]$PaddleModels = "",
    [string]$OutputModels = "",
    [int]$OpsetVersion = 11
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($PaddleModels)) {
    $PaddleModels = Join-Path $ProjectRoot "..\TurboOCR\models"
}
if ([string]::IsNullOrWhiteSpace($OutputModels)) {
    $OutputModels = Join-Path $ProjectRoot "models"
}

if (-not (Test-Path $PaddleModels)) {
    throw "Paddle model directory not found: $PaddleModels"
}
$PaddleModels = (Resolve-Path $PaddleModels).Path
New-Item -ItemType Directory -Force -Path $OutputModels | Out-Null
$OutputModels = (Resolve-Path $OutputModels).Path

$p2o = Get-Command paddle2onnx -ErrorAction SilentlyContinue
if (-not $p2o) {
    throw "paddle2onnx not found. Install it with: python -m pip install paddle2onnx==2.1.0"
}

Write-Host "Paddle models : $PaddleModels"
Write-Host "ONNX output   : $OutputModels"
Write-Host "Opset version : $OpsetVersion"
Write-Host ""

$converted = 0
$skipped = 0

foreach ($dir in Get-ChildItem -Path $PaddleModels -Directory | Sort-Object Name) {
    $json = Join-Path $dir.FullName "inference.json"
    $params = Join-Path $dir.FullName "inference.pdiparams"
    if (-not (Test-Path $json) -or -not (Test-Path $params)) {
        Write-Host "[skip] $($dir.Name) (no inference.json / inference.pdiparams)"
        $skipped++
        continue
    }

    $outDir = Join-Path $OutputModels $dir.Name
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null

    $yml = Join-Path $dir.FullName "inference.yml"
    if (Test-Path $yml) {
        Copy-Item $yml $outDir -Force
    }

    $outOnnx = Join-Path $outDir "inference.onnx"
    Write-Host "[convert] $($dir.Name)"

    & paddle2onnx `
        --model_dir $dir.FullName `
        --model_filename inference.json `
        --params_filename inference.pdiparams `
        --save_file $outOnnx `
        --opset_version $OpsetVersion `
        --enable_onnx_checker True `
        --optimize_tool None

    if ($LASTEXITCODE -ne 0) {
        throw "Conversion failed: $($dir.Name)"
    }
    if (-not (Test-Path $outOnnx)) {
        throw "Output not created: $outOnnx"
    }
    $converted++
}

Write-Host ""
Write-Host "Done. converted=$converted skipped=$skipped"
