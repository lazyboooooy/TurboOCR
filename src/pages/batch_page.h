#pragma once
#include <QtWidgets/QWidget>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QLabel>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtCore/QTimer>
#include <memory>
#include <QMutex>
#include <atomic>

class MainWindow;
class PdfRender;

/// 单文件 OCR 结果（保留坐标信息用于导出）
struct BatchResult {
    QString filePath;
    QString text;
    std::vector<std::string> lines;
    std::vector<std::array<int, 4>> boxes;
    std::vector<float> scores;
};

/// 批量 OCR 页
class BatchPage : public QWidget {
    Q_OBJECT
public:
    explicit BatchPage(MainWindow* mainWindow, QWidget* parent = nullptr);
    ~BatchPage() override;

private slots:
    void addFiles();
    void addFolder();
    void clearList();
    void onStartStop();
    void pauseBatch();
    void exportResults();

private:
    void setupUi();
    void processNext();
    void launchWorker();
    void onWorkerDone();
    void onAllDone();
    void setRunning(bool on);

    MainWindow* mainWindow_;

    QListWidget*  fileList_ = nullptr;
    QTextEdit*    logEdit_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPushButton*  startStopBtn_ = nullptr;
    QPushButton*  pauseBtn_ = nullptr;
    QPushButton*  exportBtn_ = nullptr;
    QCheckBox*    parallelChk_ = nullptr;
    QSpinBox*     parallelSpin_ = nullptr;

    QMutex queueMutex_;
    QStringList pendingFiles_;
    std::unique_ptr<PdfRender> pdfRender_;  // PDF 渲染器（存活到 OCR 完成以清理临时文件）
    std::vector<BatchResult> results_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<int>  processedCount_{0};
    std::atomic<int>  activeWorkers_{0};
    std::atomic<bool> running_{false};
};
