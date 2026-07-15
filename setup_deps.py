#!/usr/bin/env python3
"""
TurboOCR 依赖准备工具

从 GitHub Releases 下载预编译的第三方库 (3rdparty/) 和 OCR 模型 (models/)。
总计约 3.4GB，请预留足够磁盘空间。

用法:
    python setup_deps.py                  # 下载当前版本对应的依赖
    python setup_deps.py --tag v0.3.0     # 指定 Release 版本
    python setup_deps.py --local deps.zip # 使用本地已下载的压缩包
    python setup_deps.py --check          # 仅检查，不下载

Release 页面: https://github.com/lazyboooooy/TurboOCR/releases
"""

import os
import sys
import zipfile
import hashlib
import argparse
from pathlib import Path
from urllib.request import urlopen, urlretrieve
from urllib.error import URLError, HTTPError

# ============================================================
# 配置
# ============================================================
REPO = "lazyboooooy/TurboOCR"
GITHUB_HOST = "https://github.com"
# 也可通过 ghproxy.com 等镜像加速 (国内用户):
# GITHUB_HOST = "https://ghproxy.com/https://github.com"

ROOT = Path(__file__).parent
DEPS_ZIP = ROOT / "deps.zip"

# Release 包里应包含的顶层目录
REQUIRED_DIRS = ["3rdparty", "models"]


# ============================================================
# 下载
# ============================================================
def download_with_progress(url: str, dest: Path):
    """带进度条的下载，处理重定向"""
    print(f"  下载地址: {url}")
    print()

    try:
        resp = urlopen(url)
    except HTTPError as e:
        if e.code == 302:
            # 手动处理重定向 (GitHub Releases 经常 redirect)
            redirect_url = e.headers.get("Location")
            if redirect_url:
                print(f"  跟随重定向 -> {redirect_url}")
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


def get_release_url(tag: str) -> str:
    """拼接 GitHub Release 下载链接"""
    return f"{GITHUB_HOST}/{REPO}/releases/download/{tag}/deps.zip"


# ============================================================
# 解压
# ============================================================
def extract_deps(zip_path: Path) -> bool:
    """解压 deps.zip 到项目根目录"""
    print(f"\n  解压 {zip_path.name} -> {ROOT} ...")
    try:
        with zipfile.ZipFile(zip_path, "r") as zf:
            # 先打印解压清单概览
            total_files = len(zf.infolist())
            total_size = sum(info.file_size for info in zf.infolist()) / (1024 * 1024)

            print(f"  文件数: {total_files}, 总大小: {total_size:.0f} MB")
            print()

            extracted = 0
            for info in zf.infolist():
                zf.extract(info, ROOT)
                extracted += 1
                if extracted % 500 == 0 or extracted == total_files:
                    print(f"\r  解压中... {extracted}/{total_files}", end="", flush=True)

        print()
        return True
    except Exception as e:
        print(f"\n  ✗ 解压失败: {e}")
        return False


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
            # 统计大小
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
        help="使用本地 zip 文件，跳过下载"
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
        zip_path = Path(args.local)
        if not zip_path.exists():
            print(f"  ✗ 文件不存在: {args.local}")
            return 1
        print(f"  使用本地压缩包: {zip_path}")
    else:
        url = get_release_url(args.tag)
        print(f"  目标版本: {args.tag}")
        print()

        # 先探一下 URL 是否可达
        try:
            print("  检查 Release 是否存在...")
            urlopen(url)
            print("  ✓ Release 可用")
        except HTTPError as e:
            print(f"  ✗ Release 不存在 (HTTP {e.code})")
            print()
            print(f"  请确认版本 tag 正确: {args.tag}")
            print(f"  查看可用版本: {GITHUB_HOST}/{REPO}/releases")
            print(f"  或手动下载后指定: python setup_deps.py --local deps.zip")
            return 1
        except URLError as e:
            print(f"  ✗ 网络错误: {e}")
            print()
            print("  请检查网络连接。国内用户可尝试:")
            print(f"    1. 编辑此脚本, 将 GITHUB_HOST 改为 ghproxy 镜像")
            print(f"    2. 或手动下载 Release 中的 deps.zip")
            print(f"       {GITHUB_HOST}/{REPO}/releases")
            print(f"       然后运行: python setup_deps.py --local deps.zip")
            return 1

        print()
        print(f"  开始下载 deps.zip (~3.4GB), 请耐心等待...")
        zip_path = DEPS_ZIP

        try:
            download_with_progress(url, zip_path)
            print()
            print("  ✓ 下载完成")
        except (URLError, HTTPError, OSError) as e:
            print(f"\n  ✗ 下载失败: {e}")
            # 清理不完整的文件
            if zip_path.exists():
                zip_path.unlink()
            return 1

    # 解压
    if not extract_deps(zip_path):
        return 1

    print()
    print("  验证结果:")
    ok = verify_deps()

    # 清理下载的 zip
    if zip_path == DEPS_ZIP and zip_path.exists():
        print()
        ans = input("  是否删除 deps.zip? (已解压完毕) [Y/n]: ").strip().lower()
        if ans != "n":
            zip_path.unlink()
            print("  ✓ 已删除")

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
        print("  ✗ 验证未通过，请检查 zip 包内容是否完整。")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
