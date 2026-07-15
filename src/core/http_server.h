#pragma once
#include <QObject>
#include <QHttpServer>
#include <QHttpServerResponse>
#include <QHttpServerRequest>
#include <QTcpServer>
#include <memory>

class OcrEngine;

class HttpServer : public QObject {
    Q_OBJECT
public:
    explicit HttpServer(QObject* parent = nullptr);
    ~HttpServer();

    void setOcrEngine(OcrEngine* engine) { engine_ = engine; }
    bool start(quint16 port, const QHostAddress& addr);
    void stop();
    bool isRunning() const;

signals:
    void logMessage(const QString& msg);

private:
    QHttpServerResponse handleImage(const QHttpServerRequest& req);
    QHttpServerResponse handlePdf(const QHttpServerRequest& req);
    QHttpServerResponse handleQrCode(const QHttpServerRequest& req);

    std::unique_ptr<QHttpServer> server_;
    std::unique_ptr<QTcpServer> tcpServer_;
    OcrEngine* engine_ = nullptr;
};
