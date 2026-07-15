#!/usr/bin/env python3
"""
TurboOCR 依赖准备工具

从 GitHub Releases 下载预编译的第三方库 (3rdparty/) 和 OCR 模型 (models/)。
总计约 2.7GB (压缩后)，解压后约 3.4GB，请预留足够磁盘空间。

用法:
    python setup_deps.py                  # 下载当前版本对应的依赖
    python setup_deps.py --tag v0.3.0     # 指定 Release 版本
    python setup_deps.py --local ./deps/  # 使用本地已下载的压缩包目录
    python setup_deps.py --check          # 仅检查，不下载

Release 页面: https://github.com/lazyboooooy/TurboOCR/releases
"""

import os
import sys
import tarfile
import zipfile
import argparse
from pathlib import Path
from urllib.request import urlopen, urlretrieve
from urllib.error import URLError, HTTPError

# ============================================================
# 配置
# ============================================================
REPO = "lazyboooooy/TurboOCR"
GITHUB_HOST = "https://github.com"
# 国内用户可通过 ghproxy 镜像加速 (取消下一行注释):
# GITHUB_HOST = "https://ghproxy.com/https://github.com"

ROOT = Path(__file__).parent
REQUIRED_DIRS = ["3rdparty", "models"]

# 依赖分三包 (GitHub Release 单文件限制 2GB)
ARCHIVES = [
    "deps-paddle-core.zip",   # paddle_inference 核心 (不含 CUDA DLL)
    "deps-paddle-cuda.zip",   # CUDA/cuDNN DLL
    "deps-other.zip",         # 其余第三方库 + OCR 模型
]


# ============================================================
# 下载
# ============================================================
def download_with_progress(url: str, dest: Path):
    """带进度条的下载"""
    print(f"  下载: {dest.name}")
    try:
        resp = urlopen(url)
    except HTTPError as e:
        if e.code == 302:
            redirect_url = e.headers.get("Location")
            if redirect_url:
                resp = urlopen(redirect_url)
            else:
                raise
        else:
            raise

    total = int(resp.headers.get("Content-Length", 0))
    downloaded = 0
    chunk_size = 1024 * 1024  # 1MB

    with open(dest, "wb") as f:
        while True:
            chunk = resp.read(chunk_size)
            if not chunk:
                break
            f.write(chunk)
            downloaded += len(chunk)
            if total > 0:
                pct = downloaded / total * 100
                mb_down = downloaded / (1024 * 1024)
                mb_total = total / (1024 * 1024)
                bar_len = 40
                filled = int(bar_len * downloaded / total)
                bar = "█" * filled + "░" * (bar_len - filled)
                print(f"\r  [{bar}] {pct:.1f}%  {mb_down:.0f}/{mb_total:.0f} MB", end="", flush=True)

    print()
    return True


# ============================================================
# 解压
# ============================================================
def extract_archive(archive_path: Path) -> bool:
    """解压 zip 到项目根目录"""
    print(f"  解压 {archive_path.name} ...")
    try:
        with zipfile.ZipFile(archive_path, "r") as zf:
            total = len(zf.infolist())
            for i, info in enumerate(zf.infolist(), 1):
                zf.extract(info, ROOT)
                if i % 500 == 0 or i == total:
                    print(f"\r  解压中... {i}/{total}", end="", flush=True)
        print()
        return True
    except Exception as e:
        print(f"\n  ✗ 解压失败: {e}")
        return False


def extract_all(local_dir: Path) -> bool:
    """逐一解压所有分包"""
    for name in ARCHIVES:
        archive_path = local_dir / name
        if not archive_path.exists():
            print(f"  ✗ 缺少文件: {name}")
            return False
        if not extract_archive(archive_path):
            return False
    return True


# ============================================================
# 验证
# ============================================================
def verify_deps() -> bool:
    """验证 3rdparty/ 和 models/ 目录是否存在且非空"""
    all_ok = True
    for d in REQUIRED_DIRS:
        path = ROOT / d
        if not path.exists():
            print(f"  ✗ {d}/ 不存在")
            all_ok = False
        elif not any(path.iterdir()):
            print(f"  ✗ {d}/ 为空")
            all_ok = False
        else:
            total = sum(f.stat().st_size for f in path.rglob("*") if f.is_file())
            size_mb = total / (1024 * 1024)
            print(f"  ✓ {d}/  ({size_mb:.0f} MB)")
    return all_ok


# ============================================================
# 主流程
# ============================================================
def main():
    parser = argparse.ArgumentParser(
        description="TurboOCR 依赖准备 — 从 GitHub Releases 下载 3rdparty + models"
    )
    parser.add_argument(
        "--tag", default="v0.3.0",
        help="Release 版本标签 (默认 v0.3.0)"
    )
    parser.add_argument(
        "--local",
        help="使用本地目录 (包含三个 zip 文件)，跳过下载"
    )
    parser.add_argument(
        "--check", action="store_true",
        help="仅检查依赖状态"
    )
    args = parser.parse_args()

    print()
    print("  ╔══════════════════════════════════════════╗")
    print("  ║     TurboOCR 依赖准备工具               ║")
    print("  ╚══════════════════════════════════════════╝")
    print()

    # --check 模式
    if args.check:
        print("  检查依赖状态...")
        ok = verify_deps()
        if ok:
            print("\n  ✓ 依赖完整，可以编译。")
        else:
            print(f"\n  ✗ 依赖缺失，请运行: python setup_deps.py --tag <版本>")
        return 0 if ok else 1

    # 如果已存在且完整，跳过
    if verify_deps():
        print()
        print("  ✓ 依赖已完整，无需重新下载。")
        print("  (如需强制重下，先删除 3rdparty/ 和 models/ 再运行)")
        return 0

    # 下载
    if args.local:
        local_dir = Path(args.local)
        if not local_dir.is_dir():
            print(f"  ✗ 目录不存在: {args.local}")
            return 1
        print(f"  使用本地文件: {local_dir}")
        if not extract_all(local_dir):
            return 1
    else:
        print(f"  目标版本: {args.tag}")
        print(f"  共 {len(ARCHIVES)} 个分包 (~2.7GB), 开始下载...")
        print()

        tmp_dir = ROOT / ".deps_tmp"
        tmp_dir.mkdir(exist_ok=True)

        for name in ARCHIVES:
            url = f"{GITHUB_HOST}/{REPO}/releases/download/{args.tag}/{name}"
            dest = tmp_dir / name

            # 先检查 URL
            try:
                urlopen(url)
            except HTTPError as e:
                print(f"  ✗ 文件不存在 (HTTP {e.code}): {name}")
                print(f"    请确认版本 tag 和 Release 文件: {GITHUB_HOST}/{REPO}/releases")
                return 1
            except URLError as e:
                print(f"  ✗ 网络错误: {e}")
                print(f"    请检查网络连接，或手动下载后运行:")
                print(f"      python setup_deps.py --local <目录>")
                return 1

            try:
                download_with_progress(url, dest)
            except (URLError, HTTPError, OSError) as e:
                print(f"  ✗ 下载失败: {e}")
                return 1

        print()
        print("  ✓ 下载完成，开始解压...")
        if not extract_all(tmp_dir):
            return 1

        # 清理临时目录
        import shutil
        shutil.rmtree(tmp_dir)

    print()
    print("  验证结果:")
    ok = verify_deps()

    if ok:
        print()
        print("  ╔══════════════════════════════════════════╗")
        print("  ║   依赖准备完成！可以开始编译。         ║")
        print("  ║                                         ║")
        print("  ║   cmake -B build                        ║")
        print("  ║   cmake --build build --config Release  ║")
        print("  ╚══════════════════════════════════════════╝")
    else:
        print()
        print("  ✗ 验证未通过，请检查下载是否完整。")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
