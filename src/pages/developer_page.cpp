#include "developer_page.h"
#include "../main_window.h"
#include "../core/config_manager.h"
#include "../core/i18n.h"
#include "../core/http_server.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QScrollArea>

DeveloperPage::DeveloperPage(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow_(mw)
{
    setupUi();
    loadFromConfig();

    server_ = mainWindow_->httpServer();
    if (server_) {
        connect(server_, &HttpServer::logMessage, this, &DeveloperPage::appendLog);
    }
}

void DeveloperPage::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(24, 20, 24, 20);

    auto* titleLabel = new QLabel(_T("开发者模式"));
    titleLabel->setStyleSheet("font: 14pt 'Microsoft YaHei'; font-weight: bold; color: #333;");
    mainLayout->addWidget(titleLabel);

    auto* descLabel = new QLabel(
        _T("HTTP API 供其他程序调用 OCR 引擎。\n端点: /api/ocr/image  /api/ocr/pdf  /api/ocr/qrcode  /api/health"));
    descLabel->setStyleSheet("font: 9pt 'Microsoft YaHei'; color: #888; margin-bottom: 12px;");
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // ---- HTTP API ----
    auto* httpGrp = new QGroupBox(_T("HTTP API 服务"));
    auto* httpLayout = new QFormLayout(httpGrp);
    httpLayout->setSpacing(10);

    httpPortSpin_ = new QSpinBox;
    httpPortSpin_->setRange(1024, 65535); httpPortSpin_->setValue(19527);
    httpPortSpin_->setMinimumWidth(160);
    httpLayout->addRow(_T("监听端口:"), httpPortSpin_);

    httpLanChk_ = new QCheckBox(_T("暴露到局域网 (0.0.0.0)，取消则仅本机 (127.0.0.1)"));
    httpLayout->addRow(httpLanChk_);

    auto* httpBtnRow = new QHBoxLayout;
    startStopBtn_ = new QPushButton(_T("启动服务"));
    startStopBtn_->setMinimumWidth(120); startStopBtn_->setMinimumHeight(34);
    httpBtnRow->addWidget(startStopBtn_);
    statusLabel_ = new QLabel(_T("服务未启动"));
    statusLabel_->setStyleSheet("font: 10pt 'Microsoft YaHei'; color: #888;");
    httpBtnRow->addWidget(statusLabel_);
    httpBtnRow->addStretch();
    httpLayout->addRow(httpBtnRow);
    mainLayout->addWidget(httpGrp);

    connect(startStopBtn_, &QPushButton::clicked, this, [this]() {
        if (!server_) return;
        if (server_->isRunning()) { stopServer(); } else { startServer(); }
    });

    // ---- 日志 ----
    auto* logGrp = new QGroupBox(_T("运行日志"));
    auto* logLayout = new QVBoxLayout(logGrp);
    logEdit_ = new QTextEdit;
    logEdit_->setReadOnly(true);
    logEdit_->setStyleSheet("QTextEdit{font:9pt 'Consolas',monospace;color:#ccc;background:#1e1e2e;"
                           "border:none;padding:8px;}");
    logEdit_->setMinimumHeight(220);
    logLayout->addWidget(logEdit_);
    mainLayout->addWidget(logGrp);

    mainLayout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
}

void DeveloperPage::startServer() {
    if (!server_) return;
    auto& cfg = ConfigManager::instance();
    cfg.setHttpPort((quint16)httpPortSpin_->value());
    cfg.setHttpLanAccess(httpLanChk_->isChecked());
    cfg.setHttpEnabled(true); cfg.save();

    server_->setOcrEngine(mainWindow_->engine());
    QHostAddress addr = httpLanChk_->isChecked() ? QHostAddress::Any : QHostAddress::LocalHost;
    if (server_->start(httpPortSpin_->value(), addr)) {
        appendLog(_T("[HTTP] 已启动: ") + addr.toString() + ":" + QString::number(httpPortSpin_->value()));
    } else {
        appendLog(_T("[HTTP] 启动失败"));
    }
    updateButtonState();
}

void DeveloperPage::stopServer() {
    if (server_) { server_->stop(); appendLog(_T("[HTTP] 已停止")); }
    updateButtonState();
}

void DeveloperPage::updateButtonState() {
    bool running = server_ && server_->isRunning();
    startStopBtn_->setText(running ? _T("停止服务") : _T("启动服务"));
    startStopBtn_->setStyleSheet(running
        ? "QPushButton{background:#d32f2f;color:white;font-weight:bold;padding:6px 16px;border-radius:4px;}"
        : "QPushButton{background:#0078d4;color:white;font-weight:bold;padding:6px 16px;border-radius:4px;}");
    QString addr = httpLanChk_->isChecked() ? "0.0.0.0" : "127.0.0.1";
    statusLabel_->setText(running
        ? QString(_T("● 运行中 — http://%1:%2")).arg(addr).arg(httpPortSpin_->value())
        : _T("服务未启动"));
    statusLabel_->setStyleSheet(running
        ? "font:10pt 'Microsoft YaHei';color:#22c55e;font-weight:bold;"
        : "font:10pt 'Microsoft YaHei';color:#888;");
    httpPortSpin_->setEnabled(!running);
    httpLanChk_->setEnabled(!running);
}

void DeveloperPage::appendLog(const QString& msg) {
    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    logEdit_->append(QString("[%1] %2").arg(ts, msg));
    logEdit_->verticalScrollBar()->setValue(logEdit_->verticalScrollBar()->maximum());
}

void DeveloperPage::loadFromConfig() {
    loadingConfig_ = true;
    auto& cfg = ConfigManager::instance();
    httpPortSpin_->setValue(cfg.httpPort());
    httpLanChk_->setChecked(cfg.httpLanAccess());
    loadingConfig_ = false;
    updateButtonState();
    if (server_ && server_->isRunning()) {
        appendLog(_T("[HTTP] 服务已在运行中"));
    }
}

#include "developer_page.moc"
