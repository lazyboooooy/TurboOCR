#include "main_window.h"
#include "core/logger.h"
#include "core/config_manager.h"
#include "core/theme.h"
#include "core/i18n.h"
#include <QtWidgets/QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QMessageBox>

static const char* kSharedMemKey = "TurboOCR_SingleInstance_v1";
static const char* kLocalServerName = "TurboOCR_LocalServer";

/// 尝试通知已有实例显示主窗口，成功返回 true
static bool notifyExistingInstance() {
    QLocalSocket socket;
    socket.connectToServer(kLocalServerName);
    if (socket.waitForConnected(1000)) {
        socket.write("show", 4);
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("TOCR");
    app.setApplicationVersion("1.1.0");

    // ---- 单实例检测 ----
    QSharedMemory sharedMem(kSharedMemKey);
    if (!sharedMem.create(1)) {
        // 已有实例在运行 → 通知它显示主界面后退出
        if (sharedMem.attach()) {
            sharedMem.detach();
        }
        notifyExistingInstance();
        return 0;
    }

    // 初始化日志（按天轮转，自动清理超过3天的旧日志）
    Logger::instance().init(QApplication::applicationDirPath() + "/log/turboocr.log");
    TLOG_INFO() << "TOCR v" << app.applicationVersion() << " starting...";

    // 清理上次可能残留的临时文件
    {
        QString appDir = QApplication::applicationDirPath();
        QDir(appDir + "/_pdf_tmp").removeRecursively();
        QFile::remove(appDir + "/_snapshot_tmp.png");
        QFile::remove(appDir + "/_clipboard_tmp.png");
    }

    // 加载配置，获取保存的语言设置
    auto& cfg = ConfigManager::instance();
    QString iniPath = QApplication::applicationDirPath() + "/config.ini";
    cfg.setIniPath(iniPath);
    if (QFile::exists(iniPath)) cfg.load();

    // 安装应用语言（Qt 翻译 + TOCR 翻译）
    I18n::instance().setLanguage(cfg.language());

    // 应用图标
    QIcon appIcon(QApplication::applicationDirPath() + "/icon.png");
    if (!appIcon.isNull())
        app.setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();

    return app.exec();
}
