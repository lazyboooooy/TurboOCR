#include "http_server.h"
#include "qrcode_scanner.h"
#include "../ocr/engine.h"
#include "../postprocess/parsers.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QPdfDocument>
#include <QImage>
#include <QPainter>
#include <QUrlQuery>

HttpServer::HttpServer(QObject* parent) : QObject(parent) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start(quint16 port, const QHostAddress& addr) {
    if (tcpServer_ && tcpServer_->isListening()) stop();

    server_ = std::make_unique<QHttpServer>();
    tcpServer_ = std::make_unique<QTcpServer>();

    server_->route("/api/ocr/image", [this](const QHttpServerRequest& req) {
        emit logMessage(QString("[image]  %1 bytes").arg(req.body().size()));
        return handleImage(req);
    });
    server_->route("/api/ocr/pdf", [this](const QHttpServerRequest& req) {
        emit logMessage(QString("[pdf]    %1 bytes").arg(req.body().size()));
        return handlePdf(req);
    });
    server_->route("/api/ocr/qrcode", [this](const QHttpServerRequest& req) {
        emit logMessage(QString("[qrcode] %1 bytes").arg(req.body().size()));
        return handleQrCode(req);
    });
    server_->route("/api/health", [](const QHttpServerRequest&) {
        return QHttpServerResponse(QJsonObject{{"status","ok"}});
    });

    if (!tcpServer_->listen(addr, port)) return false;
    server_->bind(tcpServer_.get());
    return true;
}

void HttpServer::stop() {
    if (tcpServer_) {
        tcpServer_->close();
        tcpServer_.reset();
    }
    server_.reset();
}

bool HttpServer::isRunning() const {
    return tcpServer_ && tcpServer_->isListening();
}

// 从请求 URL 查询参数中提取 parser 类型，默认与 GUI 一致: SinglePara
static ParserType parserFromRequest(const QHttpServerRequest& req) {
    QUrlQuery query(req.url());
    QString v = query.queryItemValue("parser");
    if (v.isEmpty())      return ParserType::SinglePara;
    if (v == "none")        return ParserType::None;
    if (v == "single_para") return ParserType::SinglePara;
    if (v == "single_line") return ParserType::SingleLine;
    if (v == "single_none") return ParserType::SingleNone;
    if (v == "single_code") return ParserType::SingleCode;
    if (v == "multi_para")  return ParserType::MultiPara;
    if (v == "multi_line")  return ParserType::MultiLine;
    if (v == "multi_none")  return ParserType::MultiNone;
    return ParserType::SinglePara;
}

// ---- 工具 ----
static QString saveBodyToTemp(const QHttpServerRequest& req, const QString& suffix) {
    QByteArray body = req.body();
    if (body.isEmpty()) return {};
    QString tmpl = QDir::tempPath() + "/tocr_XXXXXX." + suffix;
    QTemporaryFile f(tmpl);
    f.setAutoRemove(false);
    if (!f.open()) return {};
    f.write(body);
    f.close();
    return f.fileName();
}

// ---- POST /api/ocr/image ----
QHttpServerResponse HttpServer::handleImage(const QHttpServerRequest& req) {
    if (!engine_ || !engine_->isReady())
        return QHttpServerResponse(QJsonObject{{"error","engine not ready"}},
                                   QHttpServerResponse::StatusCode::ServiceUnavailable);

    QString tmp = saveBodyToTemp(req, "png");
    if (tmp.isEmpty())
        return QHttpServerResponse(QJsonObject{{"error","empty body"}},
                                   QHttpServerResponse::StatusCode::BadRequest);

    auto result = engine_->predict(tmp.toStdString());
    QFile::remove(tmp);

    ParserType parser = parserFromRequest(req);
    std::string parsedText = applyParser(result, parser);

    QJsonArray items;
    for (size_t i = 0; i < result.lines.size(); ++i) {
        QJsonObject o;
        o["text"] = QString::fromStdString(result.lines[i]);
        o["score"] = i < result.scores.size() ? result.scores[i] : 0.0;
        if (i < result.boxes.size()) {
            QJsonArray b;
            for (int v : result.boxes[i]) b.append(v);
            o["box"] = b;
        }
        items.append(o);
    }
    QJsonObject resp;
    resp["text"] = QString::fromStdString(parsedText);
    resp["raw_text"] = QString::fromStdString(result.text);
    resp["parser"] = QString::fromStdString(
        parser == ParserType::SinglePara ? "single_para" :
        parser == ParserType::MultiPara ? "multi_para" : "none");
    resp["items"] = items;
    resp["count"] = (int)result.lines.size();
    resp["total_ms"] = result.total_ms;
    return QHttpServerResponse(resp);
}

// ---- POST /api/ocr/pdf ----
QHttpServerResponse HttpServer::handlePdf(const QHttpServerRequest& req) {
    if (!engine_ || !engine_->isReady())
        return QHttpServerResponse(QJsonObject{{"error","engine not ready"}},
                                   QHttpServerResponse::StatusCode::ServiceUnavailable);

    QString tmp = saveBodyToTemp(req, "pdf");
    if (tmp.isEmpty())
        return QHttpServerResponse(QJsonObject{{"error","empty body"}},
                                   QHttpServerResponse::StatusCode::BadRequest);

    QPdfDocument pdfDoc;
    pdfDoc.load(tmp);
    QFile::remove(tmp);

    int total = pdfDoc.pageCount();
    if (total <= 0)
        return QHttpServerResponse(QJsonObject{{"error","failed to load PDF"}},
                                   QHttpServerResponse::StatusCode::BadRequest);

    QJsonArray pages;
    static constexpr int MIN_SIDE = 1080;
    for (int i = 0; i < total; ++i) {
        QSizeF ps = pdfDoc.pagePointSize(i);
        double minPt = qMin(ps.width(), ps.height());
        double zoom = (minPt > 0 && minPt < MIN_SIDE) ? (MIN_SIDE / minPt) : 1.0;
        QSize renderSize(qMax(1, (int)(ps.width() * zoom)),
                         qMax(1, (int)(ps.height() * zoom)));
        QImage img = pdfDoc.render(i, renderSize);
        if (img.isNull()) continue;

        // 白底修复：QPainter 绘制到白色背景，解决预乘 alpha 黑图问题
        QImage outImg(renderSize, QImage::Format_RGB888);
        outImg.fill(Qt::white);
        {
            QPainter p(&outImg);
            p.drawImage(0, 0, img);
        }

        QString pngPath = QDir::tempPath() + QString("/tocr_pdf_page_%1.png").arg(i);
        outImg.save(pngPath, "PNG");
        auto result = engine_->predict(pngPath.toStdString());
        QFile::remove(pngPath);

        ParserType parser = parserFromRequest(req);
        std::string parsedText = applyParser(result, parser);

        QJsonArray items;
        for (size_t j = 0; j < result.lines.size(); ++j) {
            QJsonObject o;
            o["text"] = QString::fromStdString(result.lines[j]);
            o["score"] = j < result.scores.size() ? result.scores[j] : 0.0;
            if (j < result.boxes.size()) {
                QJsonArray b;
                for (int v : result.boxes[j]) b.append(v);
                o["box"] = b;
            }
            items.append(o);
        }
        QJsonObject page;
        page["page"] = i;
        page["text"] = QString::fromStdString(parsedText);
        page["raw_text"] = QString::fromStdString(result.text);
        page["parser"] = QString::fromStdString(
            parser == ParserType::SinglePara ? "single_para" : "none");
        page["items"] = items;
        page["total_ms"] = result.total_ms;
        pages.append(page);
    }

    QJsonObject resp;
    resp["total_pages"] = total;
    resp["pages"] = pages;
    return QHttpServerResponse(resp);
}

// ---- POST /api/ocr/qrcode ----
QHttpServerResponse HttpServer::handleQrCode(const QHttpServerRequest& req) {
    QString tmp = saveBodyToTemp(req, "png");
    if (tmp.isEmpty())
        return QHttpServerResponse(QJsonObject{{"error","empty body"}},
                                   QHttpServerResponse::StatusCode::BadRequest);

    auto results = QrScanner::scanFile(tmp);
    QFile::remove(tmp);

    QJsonArray arr;
    for (const auto& qr : results) {
        if (!qr.error.empty()) continue;
        QJsonObject o;
        o["type"] = QString::fromStdString(qr.format);
        o["text"] = QString::fromStdString(qr.text);
        QJsonArray b;
        b.append(qr.box[0]); b.append(qr.box[1]);
        b.append(qr.box[2]); b.append(qr.box[3]);
        o["box"] = b;
        arr.append(o);
    }

    QJsonObject resp;
    resp["qrcodes"] = arr;
    resp["count"] = arr.size();
    return QHttpServerResponse(resp);
}
