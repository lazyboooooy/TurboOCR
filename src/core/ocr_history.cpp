#include "ocr_history.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QBuffer>
#include <QDebug>

OcrHistory::OcrHistory() {}
OcrHistory::~OcrHistory() {
    if (QSqlDatabase::contains("ocr_history")) {
        QSqlDatabase::database("ocr_history").close();
    }
}

bool OcrHistory::init(const QString& dbPath) {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "ocr_history");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "OcrHistory open failed:" << db.lastError().text();
        return false;
    }
    QSqlQuery q(db);
    bool ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS ocr_history (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name       TEXT    NOT NULL,
            file_path       TEXT,
            upload_time     TEXT,
            thumbnail_blob  BLOB,
            ocr_result      TEXT,
            is_favorite     INTEGER DEFAULT 0
        )
    )");
    // 迁移：补加旧表缺少的列
    q.exec("ALTER TABLE ocr_history ADD COLUMN result_json TEXT");
    q.exec("ALTER TABLE ocr_history ADD COLUMN page_image_blob BLOB");
    q.exec("ALTER TABLE ocr_history ADD COLUMN is_document INTEGER DEFAULT 0");
    q.exec("ALTER TABLE ocr_history ADD COLUMN page_index INTEGER DEFAULT 0");
    q.exec("ALTER TABLE ocr_history ADD COLUMN total_pages INTEGER DEFAULT 1");
    q.exec("ALTER TABLE ocr_history ADD COLUMN pages_json TEXT");
    return ok;
}

int OcrHistory::add(const QString& filePath, const QString& ocrText,
                    const QString& resultJson, const QImage& pageImage,
                    int pageIndex, int totalPages, const QString& pagesJson) {
    QSqlDatabase db = QSqlDatabase::database("ocr_history");
    if (!db.isOpen()) return -1;

    // 缩略图：从 pageImage 缩放
    QByteArray thumbBuf, pageBuf;
    QPixmap thumb;
    if (!pageImage.isNull()) {
        thumb = QPixmap::fromImage(
            pageImage.scaled(160, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QBuffer tb(&thumbBuf); tb.open(QIODevice::WriteOnly);
        thumb.save(&tb, "PNG");
        QBuffer pb(&pageBuf); pb.open(QIODevice::WriteOnly);
        pageImage.save(&pb, "PNG");
    }

    QString fileName = QFileInfo(filePath).fileName();
    QString timeStr = QDateTime::currentDateTime().toString("MM-dd HH:mm");

    // 检测是否为文档格式
    QString ext = QFileInfo(filePath).suffix().toLower();
    bool isDoc = (ext == "pdf" || ext == "docx" || ext == "doc"
                  || ext == "xps" || ext == "epub" || ext == "mobi"
                  || ext == "fb2" || ext == "cbz" || ext == "svg");

    QSqlQuery q(db);
    q.prepare("INSERT INTO ocr_history(file_name,file_path,upload_time,"
              "thumbnail_blob,ocr_result,result_json,page_image_blob,is_document,"
              "page_index,total_pages,pages_json) "
              "VALUES (?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(fileName);
    q.addBindValue(filePath);
    q.addBindValue(timeStr);
    q.addBindValue(thumbBuf);
    q.addBindValue(ocrText);
    q.addBindValue(resultJson);
    q.addBindValue(pageBuf);
    q.addBindValue(isDoc ? 1 : 0);
    q.addBindValue(pageIndex);
    q.addBindValue(totalPages);
    q.addBindValue(pagesJson);
    if (!q.exec()) {
        qWarning() << "OcrHistory insert failed:" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void OcrHistory::updateLastPagesJson(const QString& pagesJson) {
    QSqlDatabase db = QSqlDatabase::database("ocr_history");
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("UPDATE ocr_history SET pages_json=? WHERE id=(SELECT MAX(id) FROM ocr_history)");
    q.addBindValue(pagesJson);
    q.exec();
}

void OcrHistory::setFavorite(int id, bool fav) {
    QSqlDatabase db = QSqlDatabase::database("ocr_history");
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("UPDATE ocr_history SET is_favorite=? WHERE id=?");
    q.addBindValue(fav ? 1 : 0);
    q.addBindValue(id);
    q.exec();
}

QList<OcrHistoryItem> OcrHistory::list(int isFavorite) {
    QList<OcrHistoryItem> result;
    QSqlDatabase db = QSqlDatabase::database("ocr_history");
    if (!db.isOpen()) return result;
    QSqlQuery q(db);
    if (isFavorite >= 0) {
        q.prepare("SELECT * FROM ocr_history WHERE is_favorite=? ORDER BY id DESC");
        q.addBindValue(isFavorite);
    } else {
        q.exec("SELECT * FROM ocr_history ORDER BY id DESC");
    }
    while (q.next()) result.append(rowToItem(q));
    return result;
}

QList<OcrHistoryItem> OcrHistory::search(const QString& keyword) {
    QList<OcrHistoryItem> result;
    QSqlDatabase db = QSqlDatabase::database("ocr_history");
    if (!db.isOpen()) return result;
    QSqlQuery q(db);
    q.prepare("SELECT * FROM ocr_history WHERE file_name LIKE ? ORDER BY id DESC");
    q.addBindValue("%" + keyword + "%");
    if (!q.exec()) return result;
    while (q.next()) result.append(rowToItem(q));
    return result;
}

void OcrHistory::remove(int id) {
    QSqlDatabase db = QSqlDatabase::database("ocr_history");
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("DELETE FROM ocr_history WHERE id=?");
    q.addBindValue(id);
    q.exec();
}

OcrHistoryItem OcrHistory::rowToItem(QSqlQuery& query) {
    OcrHistoryItem item;
    item.id         = query.value("id").toInt();
    item.fileName   = query.value("file_name").toString();
    item.filePath   = query.value("file_path").toString();
    item.uploadTime = query.value("upload_time").toString();
    item.ocrResult  = query.value("ocr_result").toString();
    item.resultJson = query.value("result_json").toString();
    item.isFavorite = query.value("is_favorite").toBool();
    item.isDocument = query.value("is_document").toBool();
    item.pageIndex  = query.value("page_index").toInt();
    item.totalPages = query.value("total_pages").toInt();
    item.pagesJson  = query.value("pages_json").toString();

    // 缩略图
    QByteArray thumbBlob = query.value("thumbnail_blob").toByteArray();
    item.thumbnail.loadFromData(thumbBlob, "PNG");

    // 完整页面图
    QByteArray pageBlob = query.value("page_image_blob").toByteArray();
    item.pageImage.loadFromData(pageBlob, "PNG");

    return item;
}
