#pragma once
#include <QString>
#include <QStringList>
#include <QPdfDocument>
#include <QImage>
#include <QMutex>

/// PDF 文本块（直接提取，不经 OCR）
struct PdfTextBlock {
    QString text;
    double  x, y, w, h;   // PDF 坐标（points）
};

/// PDF 页面数据
struct PdfExtractResult {
    QString       extractedText;       // 从 PDF 直接提取的文字
    QList<PdfTextBlock> textBlocks;    // 文字块（含坐标）
    bool          hasEmbeddedText = false;
};

/// PDF 文档渲染器 — 基于 Qt6 QPdfDocument，无需外部工具
///
/// 支持模式:
///   - 混合模式（仿 Umi-OCR）：优先提取 PDF 内嵌文字（100%准确），
///     仅对无文字层页面回退到 OCR
///   - 渲染模式：整页渲染为 PNG（兼容扫描件/图片 PDF）
///
class PdfRender {
public:
    explicit PdfRender();
    ~PdfRender() { cleanup(); }

    /// 将文件渲染为临时 PNG 列表（一页一个文件）
    QStringList render(const QString& filePath);

    /// 渲染单页到指定目录
    QString renderSinglePage(const QString& filePath, int pageIndex,
                             const QString& outDir);

    /// 提取单页内嵌文字（不含 OCR）
    PdfExtractResult extractPageText(const QString& filePath, int pageIndex);

    /// 获取 PDF 页数
    static int getPageCount(const QString& filePath);

    /// 清理临时文件
    void cleanup();

    int pageCount() const { return pageCount_; }

    static bool isSupported(const QString& ext);
    static bool isDocument(const QString& ext);

private:
    QStringList renderPdf(const QString& filePath);
    QStringList renderDocx(const QString& filePath);
    QImage renderPageToImage(QPdfDocument& doc, int pageIndex,
                             double& outZoom, QSizeF& outPtSize);

    QString sofficePath_;
    QStringList tempFiles_;
    QMutex     tempFilesMutex_;
    int pageCount_ = 0;
};
