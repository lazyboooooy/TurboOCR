#include "batch_page.h"
#include "../main_window.h"
#include "../ocr/engine.h"
#include "../output/formatters.h"
#include "../postprocess/parsers.h"
#include "../core/pdf_render.h"
#include "../core/logger.h"

#include <QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QLineEdit>
#include <QDebug>
#include <QThread>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <opencv2/opencv.hpp>

// (logging now uses TLOG_* macros from logger.h)

#include "../core/theme.h"
#include "../core/i18n.h"
#include "../core/qrcode_scanner.h"

static QString startBtnStyle() {
    const auto& t = ThemeManager::instance().currentTheme();
    return QString("QPushButton { background: %1; color: %2; font-weight: bold; padding: 6px 24px; border-radius: 4px; }")
        .arg(t.actionPrimaryBg.name(), t.actionPrimaryText.name());
}
static QString stopBtnStyle() {
    const auto& t = ThemeManager::instance().currentTheme();
    return QString("QPushButton { background: %1; color: %2; font-weight: bold; padding: 6px 24px; border-radius: 4px; }")
        .arg(t.actionDangerBg.name(), t.actionDangerText.name());
}

BatchPage::BatchPage(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow_(mw)
{
    setupUi();
}

BatchPage::~BatchPage() {
    stopRequested_ = true;
    paused_ = false;
}

void BatchPage::setRunning(bool on) {
    running_ = on;
    parallelChk_->setEnabled(!on);
    parallelSpin_->setEnabled(!on && parallelChk_->isChecked());
    if (on) {
        startStopBtn_->setText(_T("停止"));
        startStopBtn_->setStyleSheet(stopBtnStyle());
        pauseBtn_->setEnabled(true);
        pauseBtn_->setText(_T("暂停"));
    } else {
        startStopBtn_->setText(_T("开始批量 OCR"));
        startStopBtn_->setStyleSheet(startBtnStyle());
        startStopBtn_->setEnabled(fileList_->count() > 0);
        pauseBtn_->setEnabled(false);
    }
}

void BatchPage::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(6);

    // ---- 文件列表 ----
    auto* listGrp = new QGroupBox(_T("待识别文件"));
    auto* listLayout = new QVBoxLayout(listGrp);
    fileList_ = new QListWidget;
    fileList_->setAlternatingRowColors(true);
    fileList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listLayout->addWidget(fileList_);

    auto* fileBtnLayout = new QHBoxLayout;
    auto* addFileBtn = new QPushButton(_T("添加文件"));
    auto* addFolderBtn = new QPushButton(_T("添加文件夹"));
    auto* clearBtn = new QPushButton(_T("清空列表"));
    fileBtnLayout->addWidget(addFileBtn);
    fileBtnLayout->addWidget(addFolderBtn);
    fileBtnLayout->addWidget(clearBtn);
    fileBtnLayout->addStretch();
    listLayout->addLayout(fileBtnLayout);
    mainLayout->addWidget(listGrp);

    // ---- 进度条 ----
    progress_ = new QProgressBar;
    progress_->setVisible(false);
    mainLayout->addWidget(progress_);

    // ---- 控制栏 ----
    auto* ctrlLayout = new QHBoxLayout;

    parallelChk_ = new QCheckBox(_T("并行处理"));
    parallelSpin_ = new QSpinBox;
    parallelSpin_->setRange(2, 8);
    parallelSpin_->setValue(4);
    parallelSpin_->setEnabled(false);
    parallelSpin_->setToolTip(_T("并行 worker 数"));
    connect(parallelChk_, &QCheckBox::toggled, parallelSpin_, &QSpinBox::setEnabled);
    ctrlLayout->addWidget(parallelChk_);
    ctrlLayout->addWidget(parallelSpin_);
    ctrlLayout->addStretch();

    startStopBtn_ = new QPushButton(_T("开始批量 OCR"));
    startStopBtn_->setStyleSheet(startBtnStyle());
    startStopBtn_->setEnabled(false);

    pauseBtn_ = new QPushButton(_T("暂停"));
    pauseBtn_->setEnabled(false);
    pauseBtn_->setStyleSheet("QPushButton { padding: 6px 16px; }");

    exportBtn_ = new QPushButton(_T("导出结果"));
    exportBtn_->setEnabled(false);

    ctrlLayout->addWidget(startStopBtn_);
    ctrlLayout->addWidget(pauseBtn_);
    ctrlLayout->addWidget(exportBtn_);
    mainLayout->addLayout(ctrlLayout);

    // ---- 日志 ----
    auto* logGrp = new QGroupBox(_T("处理日志"));
    auto* logLayout = new QVBoxLayout(logGrp);
    logEdit_ = new QTextEdit;
    logEdit_->setReadOnly(true);
    logEdit_->setMaximumHeight(150);
    logLayout->addWidget(logEdit_);
    mainLayout->addWidget(logGrp);

    // ---- 信号 ----
    connect(addFileBtn, &QPushButton::clicked, this, &BatchPage::addFiles);
    connect(addFolderBtn, &QPushButton::clicked, this, &BatchPage::addFolder);
    connect(clearBtn, &QPushButton::clicked, this, &BatchPage::clearList);
    connect(startStopBtn_, &QPushButton::clicked, this, &BatchPage::onStartStop);
    connect(pauseBtn_, &QPushButton::clicked, this, &BatchPage::pauseBatch);
    connect(exportBtn_, &QPushButton::clicked, this, &BatchPage::exportResults);
}

void BatchPage::addFiles() {
    QFileDialog dlg(this, _T("选择文件"), QDir::homePath(),
        _T("所有支持格式 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.pdf *.xps *.epub *.mobi *.fb2 *.cbz *.docx *.doc);;图片 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;文档 (*.pdf *.xps *.epub *.mobi *.fb2 *.cbz *.docx *.doc)"));
    dlg.setFileMode(QFileDialog::ExistingFiles);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    if (!dlg.exec()) return;
    auto files = dlg.selectedFiles();
    if (files.isEmpty()) return;
    fileList_->setUpdatesEnabled(false);
    for (auto& f : files) fileList_->addItem(f);
    fileList_->setUpdatesEnabled(true);
    if (!running_) startStopBtn_->setEnabled(true);
}

void BatchPage::addFolder() {
    QFileDialog dlg(this, _T("选择文件夹"));
    dlg.setFileMode(QFileDialog::Directory);
    dlg.setOption(QFileDialog::ShowDirsOnly, true);
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
    if (!dlg.exec()) return;
    auto dirs = dlg.selectedFiles();
    if (dirs.isEmpty()) return;
    QString dir = dirs.first();

    QThread::create([this, dir]() {
        QDir d(dir);
        QStringList filters{"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tif", "*.tiff", "*.webp",
                            "*.pdf", "*.xps", "*.epub", "*.mobi", "*.fb2", "*.cbz", "*.docx", "*.doc"};
        QStringList names = d.entryList(filters, QDir::Files);

        QMetaObject::invokeMethod(this, [this, dir, names]() {
            if (names.isEmpty()) return;
            fileList_->setUpdatesEnabled(false);
            for (auto& name : names)
                fileList_->addItem(QDir(dir).absoluteFilePath(name));
            fileList_->setUpdatesEnabled(true);
            if (!running_) startStopBtn_->setEnabled(true);
            mainWindow_->showStatus(QString(_T("已添加 %1 个文件")).arg(names.size()));
        }, Qt::QueuedConnection);
    })->start();
}

void BatchPage::clearList() {
    if (running_) {
        stopRequested_ = true;
        paused_ = false;
    }
    fileList_->clear();
    results_.clear();
    logEdit_->clear();
    progress_->setVisible(false);
    setRunning(false);
    exportBtn_->setEnabled(false);
}

void BatchPage::onStartStop() {
    if (running_) {
        stopRequested_ = true;
        paused_ = false;
        pauseBtn_->setEnabled(false);
        startStopBtn_->setEnabled(false);
        logEdit_->append(_T("⏹ 正在停止..."));
        mainWindow_->showStatus(_T("批量 OCR 正在停止..."));
        if (activeWorkers_ == 0) {
            onAllDone();
            mainWindow_->showStatus(_T("批量 OCR 已停止"));
        }
    } else {
        if (fileList_->count() == 0) return;

        {
            QMutexLocker l(&queueMutex_);
            pendingFiles_.clear();
            if (!pdfRender_) pdfRender_ = std::make_unique<PdfRender>();
            for (int i = 0; i < fileList_->count(); ++i) {
                QString path = fileList_->item(i)->text();
                QString ext = QFileInfo(path).suffix().toLower();
                if (PdfRender::isDocument(ext)) {
                    auto pages = pdfRender_->render(path);
                    if (!pages.isEmpty()) {
                        pendingFiles_.append(pages);  // 展开为每页一个文件
                    } else {
                        TLOG_WARN() << "Failed to render document, skipping:" << path;
                    }
                } else if (PdfRender::isSupported(ext)) {
                    pendingFiles_ << path;
                } else {
                    TLOG_WARN() << "Unsupported format, skipping:" << path;
                }
            }
        }
        results_.clear();
        processedCount_ = 0;
        stopRequested_ = false;
        paused_ = false;

        progress_->setVisible(true);
        progress_->setMaximum(pendingFiles_.size());
        progress_->setValue(0);
        progress_->setFormat(_T("识别进度 %v/%m"));
        exportBtn_->setEnabled(false);

        setRunning(true);
        int workers = parallelChk_->isChecked() ? parallelSpin_->value() : 1;
        logEdit_->append(QString(_T("开始处理 %1 个文件%2..."))
            .arg(pendingFiles_.size())
            .arg(workers > 1 ? QString(_T("（并行 × %1）")).arg(workers) : ""));
        TLOG_INFO() << "starting, " << (int)pendingFiles_.size() << " files, " << workers << " workers";

        for (int i = 0; i < workers; ++i) launchWorker();
    }
}

void BatchPage::pauseBatch() {
    paused_ = !paused_;
    if (paused_) {
        pauseBtn_->setText(_T("继续"));
        logEdit_->append(_T("⏸ 已暂停..."));
        mainWindow_->showStatus(_T("批量 OCR 已暂停"));
    } else {
        pauseBtn_->setText(_T("暂停"));
        logEdit_->append(_T("▶ 继续处理..."));
        mainWindow_->showStatus(_T("批量 OCR 已继续"));
        processNext();
    }
}

void BatchPage::processNext() {
    if (paused_) {
        QTimer::singleShot(500, this, &BatchPage::processNext);
        return;
    }
    if (stopRequested_) return;
}

void BatchPage::launchWorker() {
    activeWorkers_++;

    QThread::create([this]() {
        while (true) {
            while (paused_ && !stopRequested_) {
                QThread::msleep(200);
                QMetaObject::invokeMethod(this, &BatchPage::processNext, Qt::QueuedConnection);
            }
            if (stopRequested_) break;

            QString file;
            {
                QMutexLocker l(&queueMutex_);
                if (pendingFiles_.isEmpty()) break;
                file = pendingFiles_.takeFirst();
            }

            TLOG_DEBUG() << "[w" << ((int)(intptr_t)QThread::currentThreadId() % 10)
                << "] " << qPrintable(QFileInfo(file).fileName());

            BatchResult br;
            br.filePath = file;
            int lineCount = 0;
            double elapsed = 0;

            QFile f(file);
            if (f.open(QIODevice::ReadOnly)) {
                QByteArray raw = f.readAll();
                f.close();

                // 先用 OpenCV 快速检测是否是纯二维码图片
                std::string qrQuick = QrScanner::quickDetect(file);
                if (!qrQuick.empty()) {
                    br.text = QString("[QRCode] %1").arg(QString::fromStdString(qrQuick));
                    lineCount = 1;
                    elapsed = 0;
                } else {
                    std::vector<uchar> buf(raw.begin(), raw.end());
                    cv::Mat img = cv::imdecode(buf, cv::IMREAD_COLOR);
                    if (!img.empty()) {
                        auto* engine = mainWindow_->engine();
                        if (engine && engine->isReady()) {
                            try {
                                auto result = engine->predict(img);
                                std::string parsed = applyParser(result, ParserType::SinglePara);
                                br.text = QString::fromStdString(parsed);
                                br.lines = result.lines;
                                br.boxes = result.boxes;
                                br.scores = result.scores;
                                lineCount = (int)result.lines.size();
                                elapsed = result.total_ms;

                                // ZXing 全格式扫描（非 QR 码条形码）
                                auto qrResults = QrScanner::scanFile(file);
                                for (const auto& qr : qrResults) {
                                    if (!qr.error.empty()) continue;
                                    if (!br.text.isEmpty()) br.text += "\n";
                                    br.text += QString("[%1] %2")
                                        .arg(QString::fromStdString(qr.format),
                                             QString::fromStdString(qr.text));
                                }
                            } catch (const std::exception& e) {
                                TLOG_ERROR() << "[worker] exception: " << e.what();
                            } catch (...) {
                            TLOG_ERROR() << "[worker] unknown exception!";
                        }
                    }
                }
            }
            }

            int done = ++processedCount_;

            bool ok = !br.text.isEmpty() || lineCount > 0;
            QMetaObject::invokeMethod(this, [this, br, lineCount, elapsed, ok, done]() {
                if (ok) results_.push_back(br);
                progress_->setValue(done);
                if (ok) {
                    logEdit_->append(QString("[%1/%2] %3  (%4 %5, %6 ms)")
                        .arg(done).arg(progress_->maximum())
                        .arg(QFileInfo(br.filePath).fileName())
                        .arg(lineCount).arg(_T("行")).arg(elapsed, 0, 'f', 0));
                } else {
                    logEdit_->append(QString("[%1/%2] %3  ⚠ %4")
                        .arg(done).arg(progress_->maximum())
                        .arg(QFileInfo(br.filePath).fileName())
                        .arg(_T("失败，已跳过")));
                }
            }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(this, &BatchPage::onWorkerDone, Qt::QueuedConnection);
    })->start();
}

void BatchPage::onWorkerDone() {
    activeWorkers_--;
    if (activeWorkers_ == 0) onAllDone();
}

void BatchPage::onAllDone() {
    TLOG_INFO() << "all done: " << (int)results_.size() << " results";
    // 清理 PDF 渲染的临时文件（OCR 已全部完成，不再需要）
    if (pdfRender_) pdfRender_->cleanup();
    logEdit_->append(QString(_T("完成！共 %1 个文件")).arg(results_.size()));
    setRunning(false);
    exportBtn_->setEnabled(true);
    progress_->setVisible(false);
    mainWindow_->showStatus(stopRequested_ ? _T("批量 OCR 已停止") : _T("批量 OCR 完成"));
}

void BatchPage::exportResults() {
    if (results_.empty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(_T("导出结果"));
    dlg.setFixedSize(400, 280);

    auto* lay = new QVBoxLayout(&dlg);

    auto* optLabel = new QLabel(_T("导出选项："));
    optLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");
    lay->addWidget(optLabel);

    auto* titledChk = new QCheckBox(_T("带标题 (《文件名》)"));
    titledChk->setChecked(true);
    lay->addWidget(titledChk);

    auto* coordChk = new QCheckBox(_T("带坐标"));
    lay->addWidget(coordChk);

    auto* txtRadio = new QRadioButton("TXT");
    auto* jsonRadio = new QRadioButton("JSON");
    txtRadio->setChecked(true);
    auto* fmtLayout = new QHBoxLayout;
    fmtLayout->addWidget(txtRadio);
    fmtLayout->addWidget(jsonRadio);
    fmtLayout->addStretch();
    lay->addLayout(fmtLayout);

    lay->addSpacing(10);

    auto* dirLabel = new QLabel(_T("导出位置："));
    dirLabel->setStyleSheet("font-weight: bold; font-size: 10pt;");
    lay->addWidget(dirLabel);

    auto* dirLayout = new QHBoxLayout;
    auto* dirEdit = new QLineEdit(QDir::homePath());
    auto* browseBtn = new QPushButton(_T("浏览..."));
    dirLayout->addWidget(dirEdit);
    dirLayout->addWidget(browseBtn);
    lay->addLayout(dirLayout);

    lay->addStretch();

    auto* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    auto* okBtn = new QPushButton(_T("导出"));
    auto* cancelBtn = new QPushButton(_T("取消"));
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    lay->addLayout(btnLayout);

    connect(browseBtn, &QPushButton::clicked, [&]() {
        QFileDialog d(this, _T("选择导出目录"));
        d.setFileMode(QFileDialog::Directory);
        d.setOption(QFileDialog::ShowDirsOnly, true);
        d.setOption(QFileDialog::DontUseNativeDialog, true);
        if (d.exec()) {
            auto dirs = d.selectedFiles();
            if (!dirs.isEmpty()) dirEdit->setText(dirs.first());
        }
    });

    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (!dlg.exec()) return;

    bool withTitle = titledChk->isChecked();
    bool withCoord = coordChk->isChecked();
    bool asJson = jsonRadio->isChecked();
    QString dir = dirEdit->text();
    if (dir.isEmpty()) return;

    for (auto& br : results_) {
        QString baseName = QFileInfo(br.filePath).completeBaseName();
        QString outPath = dir + "/" + baseName + "_ocr." + (asJson ? "json" : "txt");

        QString content;
        if (asJson) {
            content = QString::fromStdString(
                toStructuredJson(br.filePath.toStdString(), br.lines, br.boxes, br.scores));
        } else if (withCoord && !br.lines.empty()) {
            content = QString::fromStdString(
                toStructuredTxt(br.filePath.toStdString(), br.lines, br.boxes, br.scores));
        } else if (withTitle) {
            content = QString::fromStdString(
                toTitledTxt(br.filePath.toStdString(), br.text.toStdString()));
        } else {
            content = QString::fromStdString(toPlainTxt(br.text.toStdString()));
        }

        QFile f(outPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(content.toUtf8());
            f.close();
        }
    }

    mainWindow_->showStatus(QString(_T("已导出 %1 个文件到 %2")).arg(results_.size()).arg(dir));
}

#include "batch_page.moc"
