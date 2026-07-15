#include "main_window.h"
#include "core/config_manager.h"
#include "core/logger.h"
#include "core/screen_capture.h"
#include "core/theme.h"
#include "core/i18n.h"
#include "core/http_server.h"
#include "core/gpu_checker.h"
#include "ocr/engine.h"
#include "pages/calligraphy_ocr_page.h"
#include "pages/batch_page.h"
#include "pages/settings_page.h"
#include "pages/developer_page.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMimeData>
#include <QPushButton>
#include <QThread>
#include <QBuffer>
#include <QCryptographicHash>
#include <QTabBar>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_StyledBackground, true);

    // 先创建 HTTP 服务对象，确保 DeveloperPage 构造时可用
    httpServer_ = new HttpServer(this);

    setupUi();
    setupTray();
    setupLocalServer();
    loadSettings();

    // 检测 GPU 兼容性，不兼容则强制 CPU
    GpuInfo gpuInfo = GpuChecker::detect();
    TLOG_INFO() << "GPU detect:" << gpuInfo.summary();
    if (!gpuInfo.paddleCudaOk) {
        auto& cfg = ConfigManager::instance();
        auto params = cfg.ocrParams();
        if (params.device.find("gpu") != std::string::npos) {
            params.device = "cpu";
            cfg.setOcrParams(params);
            cfg.save();
            TLOG_INFO() << "GPU not compatible, forced device to CPU";
        }
    }

    initEngine();
    registerHotkey();
    setupClipboardMonitor();

    // 引擎就绪后绑定到 HTTP 服务，并根据配置自动启动
    httpServer_->setOcrEngine(engine_.get());
    if (ConfigManager::instance().httpEnabled()) {
        QHostAddress addr = ConfigManager::instance().httpLanAccess()
            ? QHostAddress::Any : QHostAddress::LocalHost;
        httpServer_->start(ConfigManager::instance().httpPort(), addr);
    }
}

MainWindow::~MainWindow() = default;

QPushButton* MainWindow::makeWinBtn() {
    auto* btn = new QPushButton;
    btn->setFixedSize(32, 32);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFlat(true);
    btn->setFocusPolicy(Qt::NoFocus);
    return btn;
}

QWidget* MainWindow::buildTitleBar() {
    auto* bar = new QWidget(this);
    bar->setObjectName("titleBar");
    bar->setFixedHeight(54);

    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(24, 10, 14, 6);
    lay->setSpacing(10);

    auto* textLayout = new QVBoxLayout;
    textLayout->setSpacing(2);
    textLayout->setContentsMargins(0, 0, 0, 0);

    titleLabel_ = new QLabel("TOCR");
    titleLabel_->setObjectName("titleLabel");
    textLayout->addWidget(titleLabel_);

    lay->addLayout(textLayout);
    lay->addStretch();

    aboutBtn_ = makeWinBtn();
    aboutBtn_->setText("?");
    aboutBtn_->setToolTip(_T("关于"));
    connect(aboutBtn_, &QPushButton::clicked, this, &MainWindow::showAbout);
    lay->addWidget(aboutBtn_);

    minBtn_ = makeWinBtn();
    minBtn_->setText("—");
    minBtn_->setToolTip(_T("最小化"));
    connect(minBtn_, &QPushButton::clicked, this, &MainWindow::showMinimized);
    lay->addWidget(minBtn_);

    closeBtn_ = makeWinBtn();
    closeBtn_->setText("✕");
    closeBtn_->setToolTip(_T("关闭"));
    connect(closeBtn_, &QPushButton::clicked, this, &MainWindow::hide);
    lay->addWidget(closeBtn_);

    return bar;
}

bool MainWindow::inTitleBarArea(const QPoint& pos) const {
    if (!titleBar_) return false;
    QPoint barPos = titleBar_->mapFrom(this, pos);
    return barPos.y() >= 0 && barPos.y() < titleBar_->height();
}

void MainWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && inTitleBarArea(event->pos())) {
        // 在标题栏区域：检查是否点击了窗口按钮（关闭/最小化/关于）
        QWidget* child = titleBar_ ? titleBar_->childAt(
            titleBar_->mapFrom(this, event->pos())) : nullptr;
        if (!qobject_cast<QPushButton*>(child)) {
            dPos_ = event->globalPos() - frameGeometry().topLeft();
        } else {
            dPos_ = QPoint();
        }
    } else {
        dPos_ = QPoint();
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    if ((event->buttons() & Qt::LeftButton) && !dPos_.isNull()) {
        move(event->globalPos() - dPos_);
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent* event) {
    if (inTitleBarArea(event->pos())) {
        if (isMaximized()) showNormal(); else showMaximized();
        return;
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

void MainWindow::showAbout() {
    QMessageBox about(this);
    about.setWindowTitle(_T("关于 TOCR"));
    about.setIconPixmap(QPixmap(QApplication::applicationDirPath() + "/icon.png")
                        .scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    about.setText("<h3>TOCR v0.4.0</h3>"
                  "<p>作者: 闻道且行之</p>"
                  "<p>联系方式: knowtoact@qq.com</p>");
    auto* copyBtn = about.addButton(_T("复制邮箱"), QMessageBox::ActionRole);
    connect(copyBtn, &QPushButton::clicked, [&]() {
        QApplication::clipboard()->setText("knowtoact@qq.com");
    });
    about.exec();
}

void MainWindow::setupTray() {
    trayIcon_ = new QSystemTrayIcon(this);
    QPixmap pm(QApplication::applicationDirPath() + "/icon.png");
    if (!pm.isNull()) {
        trayIcon_->setIcon(QIcon(pm));
    } else {
        trayIcon_->setIcon(qApp->style()->standardIcon(QStyle::SP_ComputerIcon));
    }
    trayIcon_->setToolTip("TOCR");

    trayMenu_ = new QMenu(this);
    auto* showAction = trayMenu_->addAction(_T("显示主窗口"));
    connect(showAction, &QAction::triggered, this, &MainWindow::showFromTray);
    trayMenu_->addSeparator();
    auto* quitAction = trayMenu_->addAction(_T("退出"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    trayIcon_->setContextMenu(trayMenu_);
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showFromTray();
        }
    });
    trayIcon_->show();
}

void MainWindow::showFromTray() {
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // 关闭窗口 → 隐藏到托盘，不退出
    hide();
    if (trayIcon_) trayIcon_->show();
    event->ignore();
}

void MainWindow::setupUi() {
    setWindowTitle("TOCR");
    setMinimumSize(1200, 600);
    resize(1200, 600);

    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
        this, &MainWindow::applyTheme);

    auto* central = new QWidget(this);
    central->setObjectName("centralWidget");
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(26, 14, 26, 26);  // 边距 ≥ border-radius 26px 防止圆角裁切
    mainLayout->setSpacing(6);

    titleBar_ = buildTitleBar();
    mainLayout->addWidget(titleBar_);

    tabs_ = new QTabWidget(central);
    QTabBar* tb = tabs_->tabBar();
    tb->setExpanding(true);
    tb->setElideMode(Qt::ElideNone);
    QFont tabFont("Microsoft YaHei", 9);
    tabFont.setStyleStrategy(QFont::PreferAntialias);
    tb->setFont(tabFont);
    mainLayout->addWidget(tabs_);

    setCentralWidget(central);

    ocrPage_       = new CalligraphyOcrPage(this);
    batchPage_     = new BatchPage(this);
    settingsPage_  = new SettingsPage(this);
    auto* devPage  = new DeveloperPage(this);

    tabs_->addTab(ocrPage_,      _T("OCR 识别"));
    tabs_->addTab(batchPage_,    _T("批量 OCR"));
    tabs_->addTab(settingsPage_, _T("设置"));
    tabs_->addTab(devPage,       _T("开发者"));

    // 状态标签（替代 QStatusBar，固定高度防止文本变化时布局跳动）
    statusLabel_ = new QLabel(_T("OCR 引擎就绪"));
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->setFixedHeight(28);
    mainLayout->addWidget(statusLabel_);

    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    connect(statusTimer_, &QTimer::timeout, this, [this]() {
        statusLabel_->clear();
    });
}

void MainWindow::loadSettings() {
    auto& cfg = ConfigManager::instance();
    QString iniPath = QApplication::applicationDirPath() + "/config.ini";
    cfg.setIniPath(iniPath);
    if (!QFile::exists(iniPath)) { cfg.setOcrParams(OcrParams{}); cfg.save(); }
    cfg.load();
    // 应用上次保存的主题
    ThemeManager::instance().setTheme(cfg.themeName());
}

void MainWindow::initEngine() {
    engine_ = std::make_unique<OcrEngine>();
    auto& cfg = ConfigManager::instance();
    auto params = cfg.ocrParams();
    QString base = QApplication::applicationDirPath() + "/models/";

    // 根据 model_size 自动推导模型目录（尝试两种命名约定）
    auto resolveModelDir = [&](const std::string& modelSize, const char* suffix) -> std::string {
        // 优先: PP-OCRv6_<size>_det / PP-OCRv6_<size>_rec (medium 使用)
        QString pathA = base + "PP-OCRv6_" + modelSize.c_str() + suffix;
        if (QDir(pathA).exists()) return pathA.toStdString();
        // 备选: PP-OCRv6_<size>_det_infer / PP-OCRv6_<size>_rec_infer (tiny/small 使用)
        QString pathB = base + "PP-OCRv6_" + modelSize.c_str() + suffix + "_infer";
        if (QDir(pathB).exists()) return pathB.toStdString();
        // 均不存在则返回主方案
        return pathA.toStdString();
    };

    if (params.det_model_dir.empty())
        params.det_model_dir = resolveModelDir(params.model_size, "_det");
    if (params.rec_model_dir.empty())
        params.rec_model_dir = resolveModelDir(params.model_size, "_rec");

    if (engine_->initialize(params))
        showStatus(_T("OCR 引擎就绪"), 0);   // 持久显示
    else
        showStatus(_T("引擎初始化失败"), 0);
}

void MainWindow::startHttpServer() {
    auto& cfg = ConfigManager::instance();
    if (!cfg.httpEnabled()) return;

    if (!httpServer_) {
        httpServer_ = new HttpServer(this);
        httpServer_->setOcrEngine(engine_.get());
    }

    QHostAddress addr = cfg.httpLanAccess() ? QHostAddress::Any : QHostAddress::LocalHost;
    if (httpServer_->start(cfg.httpPort(), addr)) {
        QString addrStr = cfg.httpLanAccess() ? "0.0.0.0" : "127.0.0.1";
        showStatus(QString(_T("HTTP API: http://%1:%2")).arg(addrStr).arg(cfg.httpPort()), 0);
    } else {
        showStatus(QString(_T("HTTP API 启动失败 (端口 %1)")).arg(cfg.httpPort()), 0);
    }
}

void MainWindow::showStatus(const QString& msg, int timeout) {
    if (statusLabel_) {
        statusLabel_->setText(msg);
        if (timeout > 0) {
            statusTimer_->start(timeout);
        } else {
            statusTimer_->stop();   // timeout=0 表示持久显示
        }
    }
}

// ======================== 全局热键 ========================
static int qtModToWinMod(int qtMods) {
    int w = 0;
    if (qtMods & Qt::ControlModifier)  w |= MOD_CONTROL;
    if (qtMods & Qt::ShiftModifier)    w |= MOD_SHIFT;
    if (qtMods & Qt::AltModifier)      w |= MOD_ALT;
    if (qtMods & Qt::MetaModifier)     w |= MOD_WIN;
    return w;
}

void MainWindow::registerHotkey() {
#ifdef Q_OS_WIN
    auto hk = ConfigManager::instance().hotkey();
    if (!hk.enabled || hk.key == 0) {
        TLOG_INFO() << "Hotkey disabled";
        return;
    }
    hotkeyId_ = 100;
    int winMod = qtModToWinMod(hk.modifiers);
    int vk = hk.key;  // Qt::Key 值与 Windows VK 码基本一致
    if (!RegisterHotKey((HWND)winId(), hotkeyId_, winMod, vk)) {
        TLOG_WARN() << "RegisterHotKey failed: " << hk.toString();
    } else {
        TLOG_INFO() << "Hotkey registered: " << hk.toString();
    }
#endif
}

void MainWindow::unregisterHotkey() {
#ifdef Q_OS_WIN
    if (hotkeyId_) {
        UnregisterHotKey((HWND)winId(), hotkeyId_);
        hotkeyId_ = 0;
    }
#endif
}

void MainWindow::reregisterHotkey() {
    unregisterHotkey();
    registerHotkey();
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY && msg->wParam == (WPARAM)hotkeyId_) {
            hotkeyScreenshotOcr();
            *result = 0;
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::hotkeyScreenshotOcr() {
    if (!ocrPage_) return;

    bool background = ConfigManager::instance().backgroundMode();

    if (background) {
        // 后台模式：最小化 → 截图 → 静默 OCR → 复制 → 托盘提示
        bool wasVisible = isVisible();
        if (wasVisible) showMinimized();
        QApplication::processEvents();
        QThread::msleep(200);

        ScreenCapture cap;
        QPixmap shot = cap.capture();

        if (shot.isNull()) return;

        QString tmpPath = QApplication::applicationDirPath() + "/_snapshot_tmp.png";
        shot.save(tmpPath, "PNG");

        auto* engine = this->engine();
        if (engine && engine->isReady()) {
            auto result = engine->predict(tmpPath.toStdString());
            if (!result.text.empty()) {
                QApplication::clipboard()->setText(QString::fromStdString(result.text));
            }
        }
        if (trayIcon_) {
            trayIcon_->showMessage("TOCR", _T("截图已识别，已复制到剪贴板"),
                                   QSystemTrayIcon::Information, 2000);
        }
    } else {
        // 正常模式：隐藏 → 截图 → 恢复 → 显示结果
        hide();
        QApplication::processEvents();
        QThread::msleep(200);

        ScreenCapture cap;
        QPixmap shot = cap.capture();

        show();
        if (shot.isNull()) return;

        QString tmpPath = QApplication::applicationDirPath() + "/_snapshot_tmp.png";
        shot.save(tmpPath, "PNG");
        if (tabs_) tabs_->setCurrentWidget(ocrPage_);
        ocrPage_->onImageDropped(tmpPath);
        showStatus(_T("热键截图已识别"));
    }
}

// ======================== 剪贴板监听 ========================
void MainWindow::setupClipboardMonitor() {
    auto* cb = QApplication::clipboard();
    connect(cb, &QClipboard::dataChanged, this, [this]() {
        const QMimeData* mime = QApplication::clipboard()->mimeData();
        if (!mime || !mime->hasImage()) return;

        QImage img = qvariant_cast<QImage>(mime->imageData());
        if (img.isNull()) return;

        // 去重：相同图片不重复弹窗
        QByteArray imgBytes;
        QBuffer buf(&imgBytes);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        QByteArray h = QCryptographicHash::hash(imgBytes, QCryptographicHash::Md5);
        if (h == lastClipHash_) return;
        lastClipHash_ = h;
        lastClipboardImg_ = img;

        // 弹窗询问
        QMessageBox mb(QMessageBox::Question, _T("剪贴板图片"),
                       _T("检测到剪贴板有新图片，是否进行 OCR 识别？"),
                       QMessageBox::Yes | QMessageBox::No, this);
        mb.setDefaultButton(QMessageBox::Yes);
        if (mb.exec() != QMessageBox::Yes) return;

        // 保存并 OCR
        QString tmpPath = QApplication::applicationDirPath() + "/_clipboard_tmp.png";
        img.save(tmpPath, "PNG");

        bool background = ConfigManager::instance().backgroundMode();
        if (background) {
            auto* engine = this->engine();
            if (engine && engine->isReady()) {
                auto result = engine->predict(tmpPath.toStdString());
                if (!result.text.empty()) {
                    QApplication::clipboard()->setText(QString::fromStdString(result.text));
                }
            }
            if (trayIcon_) {
                trayIcon_->showMessage("TOCR", _T("剪贴板图片已识别，已复制到剪贴板"),
                                       QSystemTrayIcon::Information, 2000);
            }
        } else {
            if (ocrPage_) {
                if (tabs_) tabs_->setCurrentWidget(ocrPage_);
                ocrPage_->onImageDropped(tmpPath);
                showStatus(_T("剪贴板图片已识别"));
            }
        }
    });
}

void MainWindow::applyTheme() {
    const auto& t = ThemeManager::instance().currentTheme();
    setStyleSheet(
        "QMainWindow { background: transparent; }" +
        buildMainWindowStylesheet(t)
    );
    // 更新标题栏按钮样式（自定义图标替换 QPushButton 文字颜色）
    if (closeBtn_) closeBtn_->setObjectName("closeBtn");
}

void MainWindow::setupLocalServer() {
    localServer_ = new QLocalServer(this);
    // 先移除可能残留的同名服务（上次异常退出时遗留）
    QLocalServer::removeServer("TurboOCR_LocalServer");
    if (!localServer_->listen("TurboOCR_LocalServer")) {
        TLOG_WARN() << "LocalServer listen failed:" << localServer_->errorString();
        return;
    }
    connect(localServer_, &QLocalServer::newConnection, this, [this]() {
        auto* client = localServer_->nextPendingConnection();
        if (!client) return;
        connect(client, &QLocalSocket::readyRead, this, [this, client]() {
            QByteArray data = client->readAll();
            if (data == "show") {
                showFromTray();
            }
        });
        connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
    });
}

#include "main_window.moc"
