#pragma once
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <QMutex>

namespace cv { class Mat; }

/// OCR 识别结果
struct TurboResult {
    std::string text;                          // 完整识别文本
    std::vector<std::string> lines;            // 每行文本
    std::vector<float> scores;                 // 每行置信度
    std::vector<std::array<int, 4>> boxes;     // 每行坐标 [x0,y0,x1,y1]
    double total_ms = 0;                       // 耗时(ms)
};

/// OCR 引擎参数
struct OcrParams {
    // 模型选择
    std::string model_size = "medium";         // tiny / small / medium
    // 模型路径（由 model_size 自动推导，也可手动覆盖）
    std::string det_model_dir;
    std::string rec_model_dir;

    // 文本行方向分类模型（自动推导，通常无需手动设置）
    std::string cls_model_dir;

    // 设备
    std::string device = "gpu:0";                // cpu / gpu:0
    int    cpu_threads = 8;
    bool   enable_mkldnn = false;
    int    mkldnn_cache_capacity = 10;

    // 检测参数（与 PP-OCRv6 官方推荐值一致）
    float  det_thresh = 0.3f;
    float  det_box_thresh = 0.6f;
    float  det_unclip_ratio = 1.5f;

    // 识别参数
    float  rec_score_thresh = 0.0f;
    int    rec_batch_size = 6;
};

/// OCR 引擎（轻量封装 ppocrv6_infer）
class OcrEngine {
public:
    OcrEngine();
    ~OcrEngine();

    bool initialize(const OcrParams& params);
    bool isReady() const { return ready_; }
    bool isPredicting() const { return predicting_; }
    const OcrParams& params() const { return params_; }

    /// OCR 识别（同步，在工作线程调用）
    TurboResult predict(const std::string& imagePath);
    TurboResult predict(const cv::Mat& image);

private:
    OcrParams   params_;
    bool        ready_ = false;
    std::atomic<bool> predicting_{false};
    class Impl;
    std::unique_ptr<Impl> impl_;
};
