#!/usr/bin/env python3
"""
TurboOCR HTTP API 客户端测试脚本
用法:
    python client.py <图片路径>              # OCR 识别单张图片
    python client.py --pdf <PDF路径>         # OCR 识别 PDF
    python client.py --qr <图片路径>         # 二维码/条形码识别
    python client.py --health                # 健康检查
    python client.py --batch <目录路径>      # 批量识别目录下所有图片
"""

import argparse
import base64
import json
import os
import sys
import time
import urllib.request
import urllib.error

# 默认地址和端口
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 19527
TIMEOUT = 120  # 秒


class TurboOCRClient:
    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT):
        self.base_url = f"http://{host}:{port}"

    def _request(self, endpoint, data=None, method="POST"):
        url = f"{self.base_url}{endpoint}"
        req = urllib.request.Request(url, data=data, method=method)
        req.add_header("Content-Type", "application/octet-stream")
        try:
            with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.URLError as e:
            print(f"[ERROR] 请求失败: {e}")
            return None
        except json.JSONDecodeError:
            print("[ERROR] 响应不是有效的 JSON")
            return None

    def health(self):
        """健康检查"""
        return self._request("/api/health", method="GET")

    def ocr_image(self, image_path, parser="single_para"):
        """识别单张图片，parser 默认与 GUI 一致"""
        if not os.path.exists(image_path):
            print(f"[ERROR] 文件不存在: {image_path}")
            return None

        with open(image_path, "rb") as f:
            data = f.read()

        file_size = len(data)
        print(f"[INFO] 发送图片: {image_path} ({file_size:,} bytes)")

        t0 = time.time()
        result = self._request(f"/api/ocr/image?parser={parser}", data=data)
        elapsed = time.time() - t0

        if result is None:
            return None

        self._print_image_result(result, elapsed)
        return result

    def ocr_pdf(self, pdf_path, parser="single_para"):
        """识别 PDF，parser 默认与 GUI 一致"""
        if not os.path.exists(pdf_path):
            print(f"[ERROR] 文件不存在: {pdf_path}")
            return None

        with open(pdf_path, "rb") as f:
            data = f.read()

        file_size = len(data)
        print(f"[INFO] 发送 PDF: {pdf_path} ({file_size:,} bytes)")

        t0 = time.time()
        result = self._request(f"/api/ocr/pdf?parser={parser}", data=data)
        elapsed = time.time() - t0

        if result is None:
            return None

        self._print_pdf_result(result, elapsed)
        return result

    def ocr_qrcode(self, image_path):
        """识别二维码/条形码"""
        if not os.path.exists(image_path):
            print(f"[ERROR] 文件不存在: {image_path}")
            return None

        with open(image_path, "rb") as f:
            data = f.read()

        print(f"[INFO] 发送图片: {image_path}")

        t0 = time.time()
        result = self._request("/api/ocr/qrcode", data=data)
        elapsed = time.time() - t0

        if result is None:
            return None

        self._print_qrcode_result(result, elapsed)
        return result

    def batch_images(self, directory, exts=None):
        """批量识别目录下所有图片"""
        if exts is None:
            exts = {".png", ".jpg", ".jpeg", ".bmp", ".tiff", ".tif"}

        files = sorted(
            f for f in os.listdir(directory)
            if os.path.splitext(f)[1].lower() in exts
        )

        if not files:
            print(f"[WARN] 目录下没有找到图片文件: {directory}")
            return

        print(f"[INFO] 找到 {len(files)} 个图片文件")
        print("-" * 60)

        total_ok = 0
        total_fail = 0
        total_text = 0
        t_start = time.time()

        for i, filename in enumerate(files, 1):
            path = os.path.join(directory, filename)
            print(f"\n[{i}/{len(files)}] {filename}")
            result = self.ocr_image(path)
            if result and "error" not in result:
                total_ok += 1
                total_text += result.get("count", 0)
            else:
                total_fail += 1

        total_time = time.time() - t_start
        print("\n" + "=" * 60)
        print(f"批量完成: {len(files)} 文件, "
              f"成功 {total_ok}, 失败 {total_fail}, "
              f"识别 {total_text} 行文本, "
              f"耗时 {total_time:.1f}s")

    # ---- 输出格式化 ----

    def _print_image_result(self, result, elapsed):
        if "error" in result:
            print(f"[ERROR] {result['error']}")
            return

        print(f"[OK] {result['count']} 行文本, "
              f"服务端 {result.get('total_ms', 0):.0f}ms, "
              f"总耗时 {elapsed:.1f}s")
        print("-" * 40)
        for item in result.get("items", []):
            score = item.get("score", 0)
            bar = "█" * int(score * 10) + "░" * (10 - int(score * 10))
            box_str = ""
            if "box" in item:
                b = item["box"]
                box_str = f"  [{b[0]},{b[1]} -> {b[2]},{b[3]}]"
            print(f"  [{bar}] {score:.3f} | {item['text']}{box_str}")
        print("-" * 40)
        full_text = result.get("text", "")
        if full_text:
            print(f"\n完整文本:\n{full_text}")

    def _print_pdf_result(self, result, elapsed):
        if "error" in result:
            print(f"[ERROR] {result['error']}")
            return

        total_pages = result.get("total_pages", 0)
        pages = result.get("pages", [])
        print(f"[OK] {total_pages} 页, 总耗时 {elapsed:.1f}s")
        print("=" * 60)

        for page in pages:
            pn = page.get("page", "?")
            text = page.get("text", "")
            count = page.get("count", 0)
            ms = page.get("total_ms", 0)
            print(f"\n--- 第 {pn + 1} 页 ({count} 行, {ms:.0f}ms) ---")
            for item in page.get("items", []):
                score = item.get("score", 0)
                print(f"  [{score:.3f}] {item['text']}")

    def _print_qrcode_result(self, result, elapsed):
        if "error" in result:
            print(f"[ERROR] {result['error']}")
            return

        count = result.get("count", 0)
        print(f"[OK] 识别到 {count} 个码, 耗时 {elapsed:.1f}s")
        print("-" * 40)
        for qr in result.get("qrcodes", []):
            print(f"  [{qr['type']}] {qr['text']}")


def main():
    parser = argparse.ArgumentParser(
        description="TurboOCR HTTP API 客户端",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
    python client.py screenshot.png
    python client.py --pdf document.pdf
    python client.py --qr barcode.png
    python client.py --health
    python client.py --batch ./images/
    python client.py --host 192.168.1.100 --port 8080 image.png
        """
    )

    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"服务地址 (默认: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"服务端口 (默认: {DEFAULT_PORT})")
    parser.add_argument("--pdf", dest="pdf_path",
                        help="PDF 文件路径")
    parser.add_argument("--qr", dest="qr_path",
                        help="二维码/条形码图片路径")
    parser.add_argument("--health", action="store_true",
                        help="健康检查")
    parser.add_argument("--batch", dest="batch_dir",
                        help="批量识别目录")
    parser.add_argument("image", nargs="?",
                        help="图片文件路径")

    args = parser.parse_args()

    client = TurboOCRClient(args.host, args.port)

    # 健康检查
    if args.health:
        print("[INFO] 健康检查...")
        result = client.health()
        if result:
            print(f"[OK] 服务状态: {result.get('status', 'unknown')}")
        else:
            print("[FAIL] 服务不可用")
            sys.exit(1)
        return

    # 二维码
    if args.qr_path:
        client.ocr_qrcode(args.qr_path)
        return

    # PDF
    if args.pdf_path:
        client.ocr_pdf(args.pdf_path)
        return

    # 批量
    if args.batch_dir:
        client.batch_images(args.batch_dir)
        return

    # 单张图片
    if args.image:
        client.ocr_image(args.image)
        return

    parser.print_help()


if __name__ == "__main__":
    main()
