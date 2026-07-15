#pragma once
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QSystemTrayIcon>
#include "ocr/engine.h"
#include <memory>
#include <QImage>

class CalligraphyOcrPage;
class BatchPage;
class SettingsPage;
class HttpServer;
class QLocalServer;
class QPushButton;
class QLabel;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    OcrEngine* engine() { return engine_.get(); }
    HttpServer* httpServer() { return httpServer_; }

public slots:
    void showStatus(const QString& msg, int timeout = 3000);
    void showAbout();
    void showFromTray();
    /// 热键截图 OCR
    void hotkeyScreenshotOcr();
    /// 重新注册热键（设置变更时调用）
    void reregisterHotkey();

protected:
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void setupUi();
    void initEngine();
    void loadSettings();
    void setupTray();
    bool inTitleBarArea(const QPoint& pos) const;
    QWidget* buildTitleBar();
    QPushButton* makeWinBtn();
    void applyTheme();

    QTabWidget* tabs_ = nullptr;
    CalligraphyOcrPage* ocrPage_ = nullptr;
    BatchPage*   batchPage_ = nullptr;
    SettingsPage* settingsPage_ = nullptr;

    std::unique_ptr<OcrEngine> engine_;

    QWidget*     titleBar_ = nullptr;
    QLabel*      titleLabel_ = nullptr;
    QPushButton* aboutBtn_ = nullptr;
    QPushButton* minBtn_ = nullptr;
    QPushButton* closeBtn_ = nullptr;

    QLabel* statusLabel_ = nullptr;
    QTimer* statusTimer_ = nullptr;

    QSystemTrayIcon* trayIcon_ = nullptr;
    QMenu*   trayMenu_ = nullptr;

    HttpServer* httpServer_ = nullptr;
    void startHttpServer();

    // 单实例：LocalServer 接收第二个进程的 "show" 消息
    QLocalServer* localServer_ = nullptr;
    void setupLocalServer();

    // 热键 + 剪贴板
    void registerHotkey();
    void unregisterHotkey();
    void setupClipboardMonitor();
    int  hotkeyId_ = 0;
    QImage lastClipboardImg_;
    QByteArray lastClipHash_;   // 去重：剪贴板图片 MD5 前 8 字节

    QPoint dPos_;
};
