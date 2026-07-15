#pragma once
#include <QtWidgets/QWidget>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTextEdit>
#include <QDateTime>

class MainWindow;
class HttpServer;

class DeveloperPage : public QWidget {
    Q_OBJECT
public:
    explicit DeveloperPage(MainWindow* mainWindow, QWidget* parent = nullptr);

private:
    void setupUi();
    void loadFromConfig();
    void startServer();
    void stopServer();
    void updateButtonState();
    void appendLog(const QString& msg);

    MainWindow* mainWindow_;
    HttpServer* server_ = nullptr;

    // HTTP
    QSpinBox*    httpPortSpin_ = nullptr;
    QCheckBox*   httpLanChk_ = nullptr;
    QPushButton* startStopBtn_ = nullptr;
    QLabel*      statusLabel_ = nullptr;

    QTextEdit*   logEdit_ = nullptr;
    bool         loadingConfig_ = false;
};
