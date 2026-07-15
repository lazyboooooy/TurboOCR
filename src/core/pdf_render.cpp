#include "pdf_render.h"
#include "logger.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QPdfSelection>
#include <QPolygonF>
#include <algorithm>

static const int MIN_SIDE = 1080;  // 短边最低分辨率（仿 Umi-OCR）

static const QStringList PDF_FORMATS  = {"pdf"};
static const QStringList DOCX_FORMATS = {"docx", "doc"};
static const QStringList IMG_FORMATS  = {"png", "jpg", "jpeg", "bmp", "tif", "tiff", "webp"};

bool PdfRender::isSupported(const QString& ext) {
    QString e = ext.toLower();
    return PDF_FORMATS.contains(e) || DOCX_FORMATS.contains(e) || IMG_FORMATS.contains(e);
}

bool PdfRender::isDocument(const QString& ext) {
    QString e = ext.toLower();
    return PDF_FORMATS.contains(e) || DOCX_FORMATS.contains(e);
}

PdfRender::PdfRender() {
    QStringList candidates = {
        "soffice.exe",
        "C:/Program Files/LibreOffice/program/soffice.exe",
        "C:/Program Files (x86)/LibreOffice/program/soffice.exe",
        QStandardPaths::findExecutable("soffice"),
    };
    for (auto& c : candidates) {
        if (!c.isEmpty() && QFileInfo::exists(c)) {
            sofficePath_ = c;
            break;
        }
    }
}

QStringList PdfRender::render(const QString& filePath) {
    cleanup();
    QFileInfo fi(filePath);
    QString suffix = fi.suffix().toLower();

    if (IMG_FORMATS.contains(suffix)) {
        pageCount_ = 1;
        return {filePath};
    }
    if (PDF_FORMATS.contains(suffix)) {
        return renderPdf(filePath);
    }
    if (DOCX_FORMATS.contains(suffix)) {
        return renderDocx(filePath);
    }
    TLOG_WARN() << "unsupported format:" << suffix;
    return {};
}

// ---------------------------------------------------------------------------
// renderPageToImage — 渲染单页到 QImage（带白底修正）
// ---------------------------------------------------------------------------
QImage PdfRender::renderPageToImage(QPdfDocument& doc, int pageIndex,
                                     double& outZoom, QSizeF& outPtSize) {
    outPtSize = doc.pagePointSize(pageIndex);
    double minPt = qMin(outPtSize.width(), outPtSize.height());
    outZoom = (minPt > 0 && minPt < MIN_SIDE) ? MIN_SIDE / minPt : 1.0;

    QSize renderSize(qMax(1, (int)(outPtSize.width()  * outZoom)),
                     qMax(1, (int)(outPtSize.height() * outZoom)));

    QImage img = doc.render(pageIndex, renderSize);
    if (img.isNull()) return {};

    // 白色背景修正预乘 alpha
    QImage outImg(renderSize, QImage::Format_RGB888);
    outImg.fill(Qt::white);
    {
        QPainter painter(&outImg);
        painter.drawImage(0, 0, img);
    }
    return outImg;
}

// ---------------------------------------------------------------------------
// extractPageText — 仿 Umi-OCR 混合模式：优先提取 PDF 内嵌文字
// ---------------------------------------------------------------------------
PdfExtractResult PdfRender::extractPageText(const QString& filePath, int pageIndex) {
    PdfExtractResult data;
    QPdfDocument doc;
    if (doc.load(filePath) != QPdfDocument::Error::None) {
        TLOG_WARN() << "extractPageText: load failed";
        return data;
    }
    if (pageIndex < 0 || pageIndex >= doc.pageCount()) return data;

    QPdfSelection sel = doc.getAllText(pageIndex);
    if (!sel.isValid() || sel.text().isEmpty()) return data;

    QString text = sel.text();
    // 过滤：如果提取的文本全是空白或太短，视为无内嵌文字
    QString stripped = text.trimmed();
    if (stripped.length() < 10) return data;

    data.hasEmbeddedText = true;
    data.extractedText = text;

    // bounds() 返回每个文本段的多边形边界（PDF points 坐标）
    const QList<QPolygonF>& bounds = sel.bounds();
    // boundingRectangle 是整体包围盒
    QRectF brect = sel.boundingRectangle();

    // 将各文本段的 bounds 按页面的 point size 转换并分块
    // 简单策略：整体文本作为一个大块，不做逐行拆分
    // （QPdfSelection 的 bounds 粒度不够细，仿 Umi-OCR 需更复杂处理）
    PdfTextBlock block;
    block.text = text;
    block.x = brect.x();
    block.y = brect.y();
    block.w = brect.width();
    block.h = brect.height();
    data.textBlocks.append(block);

    TLOG_INFO() << "extractPageText: page" << pageIndex
                << "extracted" << stripped.length() << "chars";
    return data;
}

// ---------------------------------------------------------------------------
// renderPdf — QPdfDocument 渲染
// ---------------------------------------------------------------------------
QStringList PdfRender::renderPdf(const QString& filePath) {
    QStringList result;
    QFileInfo fi(filePath);

    QPdfDocument doc;
    if (doc.load(filePath) != QPdfDocument::Error::None) {
        TLOG_ERROR() << "QPdfDocument load failed:" << fi.fileName();
        return result;
    }

    int total = doc.pageCount();
    if (total <= 0) {
        TLOG_WARN() << "PDF has 0 pages:" << fi.fileName();
        return result;
    }
    TLOG_INFO() << "PDF" << fi.fileName() << "pages:" << total;

    QString tmpDir = QApplication::applicationDirPath() + "/_pdf_tmp";
    QDir().mkpath(tmpDir);
    QString baseName = "pdf_" + QString::number(qHash(filePath), 16);

    for (int i = 0; i < total; ++i) {
        double zoom; QSizeF ptSize;
        QImage outImg = renderPageToImage(doc, i, zoom, ptSize);
        if (outImg.isNull()) {
            TLOG_WARN() << "render page" << i << "returned null image";
            continue;
        }

        QString outPath = tmpDir + "/" + baseName + "_" + QString::number(i + 1) + ".png";
        if (!outImg.save(outPath, "PNG")) {
            TLOG_WARN() << "save failed page" << i;
            continue;
        }

        TLOG_INFO() << "page" << i << ":" << ptSize.width() << "x" << ptSize.height()
                     << "pt zoom=" << zoom << "→" << outImg.width() << "x" << outImg.height() << "px";

        {
            QMutexLocker l(&tempFilesMutex_);
            tempFiles_ << outPath;
        }
        result << outPath;
    }

    pageCount_ = result.size();
    return result;
}

// ---------------------------------------------------------------------------
// renderDocx — LibreOffice → PDF → QPdfDocument
// ---------------------------------------------------------------------------
QStringList PdfRender::renderDocx(const QString& filePath) {
    QFileInfo fi(filePath);

    if (sofficePath_.isEmpty() || !QFileInfo::exists(sofficePath_)) {
        TLOG_ERROR() << "DOCX support requires LibreOffice. soffice.exe not found.";
        return {};
    }

    QString tmpDir = QApplication::applicationDirPath() + "/_pdf_tmp";
    QDir().mkpath(tmpDir);

    QProcess soffice;
    soffice.start(sofficePath_, {
        "--headless", "--convert-to", "pdf", "--outdir", tmpDir, filePath
    });
    soffice.waitForFinished(60000);

    if (soffice.exitCode() != 0) {
        TLOG_ERROR() << "LibreOffice conversion failed:"
                      << QString::fromUtf8(soffice.readAllStandardError());
        return {};
    }

    QString pdfPath = tmpDir + "/" + fi.completeBaseName() + ".pdf";
    if (!QFileInfo::exists(pdfPath)) {
        TLOG_ERROR() << "LibreOffice did not produce:" << pdfPath;
        return {};
    }

    QStringList result = renderPdf(pdfPath);
    QFile::remove(pdfPath);
    tempFiles_.removeAll(pdfPath);
    return result;
}

void PdfRender::cleanup() {
    QMutexLocker l(&tempFilesMutex_);
    for (auto& f : tempFiles_) QFile::remove(f);
    tempFiles_.clear();
    pageCount_ = 0;
}

int PdfRender::getPageCount(const QString& filePath) {
    QPdfDocument doc;
    if (doc.load(filePath) != QPdfDocument::Error::None)
        return 0;
    return doc.pageCount();
}

QString PdfRender::renderSinglePage(const QString& filePath, int pageIndex,
                                     const QString& outDir) {
    QFileInfo fi(filePath);
    QPdfDocument doc;
    if (doc.load(filePath) != QPdfDocument::Error::None) {
        TLOG_ERROR() << "renderSinglePage: load failed:" << fi.fileName();
        return {};
    }

    int total = doc.pageCount();
    if (pageIndex < 0 || pageIndex >= total) {
        TLOG_WARN() << "renderSinglePage: page" << pageIndex << "out of range [0," << total << ")";
        return {};
    }

    double zoom; QSizeF ptSize;
    QImage outImg = renderPageToImage(doc, pageIndex, zoom, ptSize);
    if (outImg.isNull()) {
        TLOG_WARN() << "renderSinglePage: render returned null for page" << pageIndex;
        return {};
    }

    QDir().mkpath(outDir);
    QString outPath = outDir + "/pdf_p" + QString::number(pageIndex) + "_"
                      + QString::number(qHash(filePath), 16) + ".png";
    if (!outImg.save(outPath, "PNG")) {
        TLOG_WARN() << "renderSinglePage: save failed";
        return {};
    }

    {
        QMutexLocker l(&tempFilesMutex_);
        tempFiles_ << outPath;
    }
    return outPath;
}
