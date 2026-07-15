#pragma once
#include <QString>
#include <QVector>
#include <array>
#include <string>

/// 单条二维码/条形码结果
struct QrResult {
    std::string text;            // 解码文字
    std::string format;          // 格式名: QRCode / DataMatrix / Code128 ...
    std::string error;           // 错误信息（空=成功）
    std::array<int, 4> box {};   // 边界框 {x, y, width, height}，全零表示无位置
};

/// 二维码/条形码扫描器 — OpenCV 快速预检 + ZXing 全格式扫描
class QrScanner {
public:
    /// 快速检测图片是否包含二维码 (OpenCV QRCodeDetector, <10ms)
    /// 返回解码文字，空串表示未检测到
    static std::string quickDetect(const QString& imagePath);

    /// 全格式扫描 (ZXing: QR+DataMatrix+PDF417+Code128 等 20+ 种)
    static QVector<QrResult> scanFile(const QString& imagePath);

    /// 扫描灰度图数据
    static QVector<QrResult> scanGray(const unsigned char* data, int width, int height);
};
