#pragma once
#include <QString>
#include <QList>
#include <QPixmap>
#include <QImage>
#include <QDateTime>

struct OcrHistoryItem {
    int     id = 0;
    QString fileName;
    QString filePath;
    QString uploadTime;
    QPixmap thumbnail;       // 缩略图（160×120）
    QImage  pageImage;       // 完整页面渲染图（PDF 渲染后的 PNG，图片为自身）
    QString ocrResult;       // 纯文本
    QString resultJson;      // 结构化 JSON（含 boxes/lines/scores）
    bool    isFavorite = false;
    bool    isDocument = false;
    int     pageIndex = 0;
    int     totalPages = 1;
    QString pagesJson;           // 多页文档全部页面的 OCR 数据 JSON
};

/// SQLite 管理的 OCR 历史记录
class OcrHistory {
public:
    OcrHistory();
    ~OcrHistory();

    bool init(const QString& dbPath);

    /// 新增一条记录，返回 id
    /// @param filePath    原始文件路径
    /// @param ocrText     OCR 纯文本结果
    /// @param resultJson  结构化 JSON
    /// @param pageImage   完整页面图像（PDF 为渲染后 PNG，图片为自身）
    int  add(const QString& filePath, const QString& ocrText,
             const QString& resultJson, const QImage& pageImage,
             int pageIndex = 0, int totalPages = 1,
             const QString& pagesJson = QString());  // 多页文档的全部页面数据

    void setFavorite(int id, bool fav);
    /// 更新最近一条记录的 pagesJson（多页文档后台 OCR 完成后调用）
    void updateLastPagesJson(const QString& pagesJson);
    QList<OcrHistoryItem> list(int isFavorite = -1);
    QList<OcrHistoryItem> search(const QString& keyword);
    void remove(int id);

private:
    OcrHistoryItem rowToItem(class QSqlQuery& query);
};
