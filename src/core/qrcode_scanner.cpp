#include "qrcode_scanner.h"
#include "ReadBarcode.h"
#include <QImage>
#include <QFileInfo>
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

// ======================== OpenCV 快速 QR 预检 ========================
std::string QrScanner::quickDetect(const QString& imagePath) {
    try {
        cv::Mat img = cv::imread(imagePath.toStdString(), cv::IMREAD_COLOR);
        if (img.empty()) return {};

        cv::QRCodeDetector detector;
        // detectAndDecode 返回空串表示没有二维码
        std::string text = detector.detectAndDecode(img);
        return text;
    } catch (...) {
        return {};
    }
}

// ======================== ZXing 全格式扫描 ========================
QVector<QrResult> QrScanner::scanFile(const QString& imagePath) {
    QVector<QrResult> out;
    QImage img(imagePath);
    if (img.isNull()) return out;

    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    return scanGray(gray.constBits(), gray.width(), gray.height());
}

QVector<QrResult> QrScanner::scanGray(const unsigned char* data, int width, int height) {
    QVector<QrResult> out;
    if (!data || width <= 0 || height <= 0) return out;

    try {
        auto image = ZXing::ImageView(data, width, height, ZXing::ImageFormat::Lum);
        auto options = ZXing::ReaderOptions()
            .setTryHarder(true)
            .setTryRotate(true);
        auto barcodes = ZXing::ReadBarcodes(image, options);

        for (const auto& b : barcodes) {
            QrResult r;
            r.text   = b.text();
            r.format = ZXing::ToString(b.format());

            auto pos = b.position();
            int x1 = std::min({pos[0].x, pos[1].x, pos[2].x, pos[3].x});
            int y1 = std::min({pos[0].y, pos[1].y, pos[2].y, pos[3].y});
            int x2 = std::max({pos[0].x, pos[1].x, pos[2].x, pos[3].x});
            int y2 = std::max({pos[0].y, pos[1].y, pos[2].y, pos[3].y});
            r.box = {x1, y1, x2 - x1, y2 - y1};

            out.append(r);
        }
    } catch (const std::exception& e) {
        QrResult r;
        r.error = e.what();
        out.append(r);
    } catch (...) {
        QrResult r;
        r.error = "unknown ZXing error";
        out.append(r);
    }
    return out;
}
