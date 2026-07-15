#include "engine.h"
#include "src/api/pipelines/ocr.h"
#include "src/pipelines/ocr/result.h"
#include "../core/logger.h"
#include <chrono>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <cstdint>
#include <QThread>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>

/// 将 Qt 路径转为 PaddleOCR 可识别的本地编码 std::string
static std::string toLocalPath(const QString& path) {
    return QFile::encodeName(path).toStdString();
}

/// 检测路径是否含非 ASCII 字符（Paddle/yaml-cpp 在 Windows 上不支持）
static bool hasNonAscii(const QString& path) {
    for (const QChar& c : path) {
        if (c.unicode() > 127) return true;
    }
    return false;
}

/// 如有中文路径，将模型复制到临时 ASCII 目录，返回安全路径；否则返回原路径
static std::string ensureSafeModelPath(const std::string& utf8Path) {
    QString srcPath = QString::fromStdString(utf8Path);
    if (!hasNonAscii(srcPath))
        return toLocalPath(srcPath);

    // 路径含中文 → 复制到 %TEMP%/TurboOCR/models/ 下
    QString tmpBase = QDir::tempPath() + "/TurboOCR/models";
    QString srcModelsDir = QFileInfo(srcPath).path();         // .../models/
    QString relPath = srcPath.mid(srcModelsDir.length() + 1); // PP-OCRv6_xxx/
    QString dstPath = tmpBase + "/" + relPath;

    if (!QDir(dstPath).exists()) {
        QDir().mkpath(dstPath);
        // 复制整个模型目录到临时路径
        QDir srcDir(srcPath);
        for (const auto& entry : srcDir.entryList(QDir::Files)) {
            QFile::copy(srcPath + "/" + entry, dstPath + "/" + entry);
        }
    }
    return toLocalPath(dstPath);
}

struct OcrEngine::Impl {
    std::unique_ptr<PaddleOCR> ocr;
    PaddleOCRParams            ocrParams;
    QMutex                     mutex;   // 保护 predict() 串行化
};

OcrEngine::OcrEngine() : impl_(std::make_unique<Impl>()) {}
OcrEngine::~OcrEngine() = default;

bool OcrEngine::initialize(const OcrParams& params) {
    params_ = params; ready_ = false;
    try {
        // 模型名称由 model_size 推导（统一命名，不含 _infer 后缀）
        std::string detModelName = "PP-OCRv6_" + params.model_size + "_det";
        std::string recModelName = "PP-OCRv6_" + params.model_size + "_rec";

        auto& p = impl_->ocrParams;
        // 中文路径 → 自动复制模型到 %TEMP%/TurboOCR/ 纯 ASCII 目录
        p.text_detection_model_dir   = ensureSafeModelPath(params.det_model_dir);
        p.text_detection_model_name  = detModelName;
        p.text_recognition_model_dir  = ensureSafeModelPath(params.rec_model_dir);
        p.text_recognition_model_name = recModelName;

        TLOG_INFO() << "Engine init: det_dir=" << params.det_model_dir
                    << " det_name=" << detModelName
                    << " rec_dir=" << params.rec_model_dir
                    << " rec_name=" << recModelName
                    << " device=" << params.device;

        // OCR.yml 配置内嵌到代码中，彻底消除文件依赖
        {
            std::unordered_map<std::string, std::string> cfgMap;
            cfgMap["text_type"] = "general";
            cfgMap["use_doc_orientation_classify"] = "false";
            cfgMap["use_doc_unwarping"] = "false";
            cfgMap["use_textline_orientation"] = "true";
            cfgMap["TextDetection.model_name"] = "";
            cfgMap["TextDetection.model_dir"] = "";
            cfgMap["TextRecognition.model_name"] = "";
            cfgMap["TextRecognition.model_dir"] = "";
            cfgMap["TextLineOrientation.model_name"] = "PP-LCNet_x1_0_textline_ori";
            cfgMap["TextLineOrientation.model_dir"] = "";
            cfgMap["TextLineOrientation.batch_size"] = "6";
            p.paddlex_config = cfgMap;
        }

        p.device = params.device; p.cpu_threads = params.cpu_threads;
        p.enable_mkldnn = params.enable_mkldnn;
        p.mkldnn_cache_capacity = params.mkldnn_cache_capacity;
        // 检测参数
        p.text_det_thresh = params.det_thresh;
        p.text_det_box_thresh = params.det_box_thresh;
        p.text_det_unclip_ratio = params.det_unclip_ratio;
        p.text_det_limit_side_len = 64;     // 短边最小64px
        p.text_det_limit_type = "min";
        // 识别参数
        p.text_rec_score_thresh = params.rec_score_thresh;
        p.text_recognition_batch_size = params.rec_batch_size;
        // 显式关闭预处理（CheckParams 要求：要么给模型路径，要么显式设为 false）
        p.use_doc_orientation_classify = false;
        p.use_doc_unwarping = false;
        p.use_textline_orientation = true;
        // 方向分类模型路径：优先用 params，否则自动从 exe 同级 models/ 查找
        std::string clsDir = params.cls_model_dir;
        if (clsDir.empty()) {
            clsDir = (QApplication::applicationDirPath() + "/models/PP-LCNet_x1_0_textline_ori").toStdString();
        }
        if (QDir(QString::fromStdString(clsDir)).exists()) {
            p.textline_orientation_model_dir = ensureSafeModelPath(clsDir);
            p.textline_orientation_model_name = "PP-LCNet_x1_0_textline_ori";
        }

        impl_->ocr = std::make_unique<PaddleOCR>(p);
        ready_ = true;
    } catch (const std::exception& e) {
        TLOG_ERROR() << "Engine init failed: " << e.what();
        ready_ = false; return false;
    } catch (...) {
        TLOG_ERROR() << "Engine init failed: unknown exception";
        ready_ = false; return false;
    }
    return true;
}

TurboResult OcrEngine::predict(const std::string& imagePath) {
    TurboResult r;
    if (!ready_ || !impl_->ocr) {
        TLOG_WARN() << "predict: engine not ready!";
        return r;
    }

    predicting_ = true;
    struct Guard { std::atomic<bool>& f; ~Guard() { f = false; } } guard{predicting_};
    TLOG_DEBUG() << "predict: acquiring lock...";
    QMutexLocker lock(&impl_->mutex);   // 防止并发调用导致 Paddle 内部崩溃
    std::string safePath = toLocalPath(QString::fromStdString(imagePath));
    TLOG_DEBUG() << "predict: lock acquired, calling PaddleOCR::Predict(" << safePath << ")";

    auto t0 = std::chrono::steady_clock::now();

    TLOG_DEBUG() << "predict: calling Predict()...";
    std::vector<std::unique_ptr<BaseCVResult>> results;
    try {
        results = impl_->ocr->Predict(safePath);
    } catch (const std::exception& e) {
        TLOG_ERROR() << "Predict() exception: " << e.what();
        return r;
    } catch (...) {
        TLOG_ERROR() << "Predict() unknown exception!";
        return r;
    }
    TLOG_DEBUG() << "predict: Predict() returned " << (int)results.size() << " items";

    for (auto& o : results) {
        auto* res = dynamic_cast<OCRResult*>(o.get());
        if (!res) continue;
        const auto& pr = res->PipelineResult();
        r.lines = pr.rec_texts; r.scores = pr.rec_scores;
        for (auto& b : pr.rec_boxes)
            r.boxes.push_back({(int)b[0],(int)b[1],(int)b[2],(int)b[3]});
        for (size_t i = 0; i < pr.rec_texts.size(); ++i) {
            if (i > 0) r.text += "\n"; r.text += pr.rec_texts[i];
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    r.total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    TLOG_INFO() << "predict: done, " << (int)r.lines.size() << " lines, " << r.total_ms << " ms";
    return r;
}

TurboResult OcrEngine::predict(const cv::Mat& image) {
    // 用线程 ID + 原子计数器生成唯一文件名，防止并行 worker 互相覆盖
    static std::atomic<int> seq{0};
    std::string tmp = "_turboocr_tmp_"
        + std::to_string((uintptr_t)QThread::currentThreadId())
        + "_" + std::to_string(seq.fetch_add(1)) + ".png";
    cv::imwrite(tmp, image);
    auto r = predict(tmp);
    std::remove(tmp.c_str());
    return r;
}
