#include "settings_page.h"
#include "../main_window.h"
#include "../core/config_manager.h"
#include "../core/loading_overlay.h"
#include "../core/theme.h"
#include "../core/i18n.h"
#include "../core/gpu_checker.h"
#include "../ocr/engine.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QScrollArea>
#include <QStandardItemModel>
#include <QDir>
#include <QThread>
#include <QMessageBox>
#include <QProcess>
#include <QApplication>

SettingsPage::SettingsPage(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow_(mw)
{
    setupUi();
    loadFromConfig();
}

void SettingsPage::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* contentWidget = new QWidget;
    auto* mainLayout = new QVBoxLayout(contentWidget);

    // ---- 推理设备 ----
    auto* devGrp = new QGroupBox(_T("推理设备"));
    auto* devLayout = new QFormLayout(devGrp);
    devLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    devLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    modelSizeCombo_ = new QComboBox;
    modelSizeCombo_->addItems({"tiny", "small", "medium"});
    modelSizeCombo_->setMinimumWidth(160);
    modelSizeCombo_->setToolTip(_T("tiny: 最小最快, small: 平衡, medium: 最高精度"));
    devLayout->addRow(_T("模型大小:"), modelSizeCombo_);

    deviceCombo_ = new QComboBox;
    deviceCombo_->addItems({"cpu", "gpu:0"});
    deviceCombo_->setMinimumWidth(160);
    devLayout->addRow(_T("设备:"), deviceCombo_);

    gpuStatusLabel_ = new QLabel;
    gpuStatusLabel_->setStyleSheet("font:8pt 'Microsoft YaHei';color:#999;padding-left:4px;");
    gpuStatusLabel_->setWordWrap(true);
    devLayout->addRow("", gpuStatusLabel_);

    // 切换设备时更新 GPU 兼容性提示
    connect(deviceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { updateGpuStatus(); });

    threadsSpin_ = new QSpinBox; threadsSpin_->setRange(1, 32); threadsSpin_->setValue(8);
    threadsSpin_->setMinimumWidth(160);
    devLayout->addRow(_T("线程数:"), threadsSpin_);

    recBatchSpin_ = new QSpinBox; recBatchSpin_->setRange(1, 32); recBatchSpin_->setValue(6);
    recBatchSpin_->setMinimumWidth(160);
    devLayout->addRow(_T("识别批大小:"), recBatchSpin_);
    mainLayout->addWidget(devGrp);

    // ---- 文本检测 ----
    auto* detGrp = new QGroupBox(_T("文本检测"));
    auto* detLayout = new QFormLayout(detGrp);
    detLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    detLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    detThreshSpin_ = new QDoubleSpinBox; detThreshSpin_->setRange(0.01, 1.0); detThreshSpin_->setSingleStep(0.05); detThreshSpin_->setValue(0.3);
    detThreshSpin_->setMinimumWidth(160);
    detLayout->addRow(_T("检测阈值:"), detThreshSpin_);
    detBoxThreshSpin_ = new QDoubleSpinBox; detBoxThreshSpin_->setRange(0.01, 1.0); detBoxThreshSpin_->setSingleStep(0.05); detBoxThreshSpin_->setValue(0.6);
    detBoxThreshSpin_->setMinimumWidth(160);
    detLayout->addRow(_T("框阈值:"), detBoxThreshSpin_);
    detUnclipSpin_ = new QDoubleSpinBox; detUnclipSpin_->setRange(1.0, 3.0); detUnclipSpin_->setSingleStep(0.1); detUnclipSpin_->setValue(1.5);
    detUnclipSpin_->setMinimumWidth(160);
    detLayout->addRow(_T("扩展比例:"), detUnclipSpin_);
    mainLayout->addWidget(detGrp);

    // ---- 文本识别 ----
    auto* recGrp = new QGroupBox(_T("文本识别"));
    auto* recLayout = new QFormLayout(recGrp);
    recLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    recLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    recScoreThreshSpin_ = new QDoubleSpinBox; recScoreThreshSpin_->setRange(0.0, 1.0); recScoreThreshSpin_->setSingleStep(0.05);
    recScoreThreshSpin_->setMinimumWidth(160);
    recLayout->addRow(_T("置信度阈值:"), recScoreThreshSpin_);
    mainLayout->addWidget(recGrp);

    // ---- 外观主题 ----
    auto* uiGrp = new QGroupBox(_T("外观主题"));
    auto* uiLayout = new QFormLayout(uiGrp);
    uiLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    uiLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    themeCombo_ = new QComboBox;
    for (const auto& name : ThemeManager::instance().themeNames())
        themeCombo_->addItem(_T(name), name);  // display=翻译, data=原始名
    themeCombo_->setMinimumWidth(160);
    uiLayout->addRow(_T("主题:"), themeCombo_);

    langCombo_ = new QComboBox;
    langCombo_->addItem("English", "en");
    langCombo_->addItem("简体中文", "zh_CN");
    langCombo_->setMinimumWidth(160);
    uiLayout->addRow(_T("语言:"), langCombo_);
    auto* langNote = new QLabel(_T("切换后需重启生效"));
    langNote->setStyleSheet("font:8pt 'Microsoft YaHei';color:#999;");
    uiLayout->addRow("", langNote);
    mainLayout->addWidget(uiGrp);

    // ---- 热键 ----
    auto* hkGrp = new QGroupBox(_T("全局热键（截图 OCR）"));
    auto* hkLayout = new QFormLayout(hkGrp);
    hkLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    hkLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    hotkeyEnableChk_ = new QCheckBox(_T("启用"));
    hkLayout->addRow(_T("状态:"), hotkeyEnableChk_);

    hotkeyEdit_ = new QKeySequenceEdit;
    hotkeyEdit_->setMinimumWidth(200);
    hkLayout->addRow(_T("快捷键:"), hotkeyEdit_);

    backgroundChk_ = new QCheckBox(_T("后台模式（识别结果自动复制到剪贴板，不弹出界面）"));
    hkLayout->addRow(backgroundChk_);
    mainLayout->addWidget(hkGrp);

    // ---- 按钮 ----
    auto* btnLayout = new QHBoxLayout;
    resetBtn_ = new QPushButton(_T("恢复默认"));
    resetBtn_->setStyleSheet("QPushButton { padding: 6px 16px; }");
    applyBtn_ = new QPushButton(_T("应用设置"));
    applyBtn_->setObjectName("applyBtn");
    applyThemePreview();
    btnLayout->addWidget(resetBtn_);
    btnLayout->addStretch();
    btnLayout->addWidget(applyBtn_);
    mainLayout->addLayout(btnLayout);
    mainLayout->addStretch();

    scrollArea->setWidget(contentWidget);
    outerLayout->addWidget(scrollArea);

    // Loading 遮罩
    overlay_ = new LoadingOverlay(this); overlay_->raise();

    connect(applyBtn_, &QPushButton::clicked, this, &SettingsPage::applySettings);
    connect(resetBtn_, &QPushButton::clicked, this, [this]() {
        auto& cfg = ConfigManager::instance();
        OcrParams defaults;  // model_size defaults to "medium", paths derived in initEngine
        cfg.setOcrParams(defaults);
        cfg.setThemeName("白色");
        cfg.save();
        loadFromConfig();
        ThemeManager::instance().setTheme("白色");
        applyThemePreview();
        mainWindow_->showStatus(_T("已恢复默认设置"));
    });

    // 主题预览
    connect(themeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { if (!loadingConfig_) { ThemeManager::instance().setTheme(themeCombo_->currentData().toString()); applyThemePreview(); } });

    // 语言切换 → 保存配置 → 重启软件
    connect(langCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) {
            if (loadingConfig_) return;
            auto& cfg = ConfigManager::instance();
            QString newLang = langCombo_->currentData().toString();
            if (newLang == cfg.language()) return;  // 没变
            cfg.setLanguage(newLang);
            cfg.save();
            QMessageBox::information(this, _T("语言"),
                QString(_T("语言已保存，软件将自动重启以应用「%1」"))
                    .arg(langCombo_->currentText()));
            // 重启自身
            QProcess::startDetached(QApplication::applicationFilePath(), {});
            QApplication::quit();
        });
}

void SettingsPage::loadFromConfig() {
    loadingConfig_ = true;
    auto& cfg = ConfigManager::instance();
    auto p = cfg.ocrParams();

    int modelIdx = modelSizeCombo_->findText(QString::fromStdString(p.model_size));
    if (modelIdx >= 0) modelSizeCombo_->setCurrentIndex(modelIdx);

    int devIdx = (p.device.find("gpu") != std::string::npos) ? 1 : 0;
    deviceCombo_->setCurrentIndex(devIdx);
    threadsSpin_->setValue(p.cpu_threads);
    recBatchSpin_->setValue(p.rec_batch_size);
    detThreshSpin_->setValue(p.det_thresh);
    detBoxThreshSpin_->setValue(p.det_box_thresh);
    detUnclipSpin_->setValue(p.det_unclip_ratio);
    recScoreThreshSpin_->setValue(p.rec_score_thresh);

    // 热键
    auto hk = cfg.hotkey();
    hotkeyEnableChk_->setChecked(hk.enabled);
    hotkeyEdit_->setKeySequence(QKeySequence(hk.modifiers | hk.key));
    backgroundChk_->setChecked(cfg.backgroundMode());

    // 主题
    int tIdx = themeCombo_->findData(cfg.themeName());
    if (tIdx >= 0) themeCombo_->setCurrentIndex(tIdx);

    int lIdx = langCombo_->findData(cfg.language());
    if (lIdx >= 0) langCombo_->setCurrentIndex(lIdx);

    loadingConfig_ = false;
    updateGpuStatus();  // 首次加载时显示 GPU 状态
}

void SettingsPage::applySettings() {
    if (loading_) return; loading_ = true;

    // 识别中禁止切换设备/模型，避免推理引擎崩溃
    auto* engine = mainWindow_->engine();
    if (engine && engine->isPredicting()) {
        QMessageBox::warning(this, _T("提示"),
            _T("OCR 识别正在进行中，请等待识别完成后再切换设备或模型。"));
        loading_ = false;
        return;
    }

    auto& cfg = ConfigManager::instance();
    auto oldParams = cfg.ocrParams();

    OcrParams p;
    p.model_size       = modelSizeCombo_->currentText().toStdString();
    // 根据 model_size 自动推导模型目录（尝试两种命名约定）
    {
        QString base = QApplication::applicationDirPath() + "/models/";
        auto resolveModelDir = [&](const std::string& size, const char* suffix) -> std::string {
            QString pathA = base + "PP-OCRv6_" + size.c_str() + suffix;
            if (QDir(pathA).exists()) return pathA.toStdString();
            QString pathB = base + "PP-OCRv6_" + size.c_str() + suffix + "_infer";
            if (QDir(pathB).exists()) return pathB.toStdString();
            return pathA.toStdString();
        };
        p.det_model_dir = resolveModelDir(p.model_size, "_det");
        p.rec_model_dir = resolveModelDir(p.model_size, "_rec");

        // 文本行方向分类模型（与 model_size 无关，固定模型）
        QString clsPath = base + "PP-LCNet_x1_0_textline_ori";
        if (QDir(clsPath).exists()) p.cls_model_dir = clsPath.toStdString();
    }
    p.device           = deviceCombo_->currentText().toStdString();
    p.cpu_threads      = threadsSpin_->value();
    p.rec_batch_size   = recBatchSpin_->value();
    p.enable_mkldnn    = false;   // 默认强制开启
    p.det_thresh       = static_cast<float>(detThreshSpin_->value());
    p.det_box_thresh   = static_cast<float>(detBoxThreshSpin_->value());
    p.det_unclip_ratio = static_cast<float>(detUnclipSpin_->value());
    p.rec_score_thresh = static_cast<float>(recScoreThreshSpin_->value());

    // 热键
    QKeySequence ks = hotkeyEdit_->keySequence();
    HotkeyConfig hk;
    if (!ks.isEmpty()) {
        int combo = ks[0];
        hk.key = combo & ~Qt::KeyboardModifierMask;
        hk.modifiers = combo & Qt::KeyboardModifierMask;
    }
    hk.enabled = hotkeyEnableChk_->isChecked();
    cfg.setHotkey(hk);
    cfg.setBackgroundMode(backgroundChk_->isChecked());

    cfg.setOcrParams(p);
    cfg.setThemeName(themeCombo_->currentData().toString());
    cfg.setLanguage(langCombo_->currentData().toString());
    cfg.save();

    // 热键立即生效
    mainWindow_->reregisterHotkey();

    // UI 禁用 + 转圈
    applyBtn_->setEnabled(false);
    resetBtn_->setEnabled(false);
    overlay_->show(_T("引擎加载中，请稍候..."));
    mainWindow_->showStatus(_T("正在重新加载引擎..."));

    // 子线程执行引擎初始化，避免 GPU 加载卡死 UI
    QThread::create([this, p]() {
        QMutexLocker l(&mutex_);
        auto* engine = mainWindow_->engine();
        bool ok = engine && engine->initialize(p);

        QMetaObject::invokeMethod(this, [this, ok]() {
            overlay_->hide();
            applyBtn_->setEnabled(true);
            resetBtn_->setEnabled(true);
            loading_ = false;
            if (ok)
                mainWindow_->showStatus(_T("引擎已重新加载"));
            else
                QMessageBox::warning(this, _T("错误"), _T("引擎初始化失败"));
        });
    })->start();
}

void SettingsPage::applyThemePreview() {
    const auto& t = ThemeManager::instance().currentTheme();
    if (applyBtn_) applyBtn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; font-weight: bold; padding: 8px 24px; border-radius: 4px; }")
        .arg(t.settingsApplyBg.name(), t.settingsApplyText.name()));
}

void SettingsPage::updateGpuStatus() {
    if (!gpuStatusLabel_ || !deviceCombo_) return;

    // 检测 GPU 兼容性
    GpuInfo info = GpuChecker::detect();
    bool gpuOk = info.directMlOk;

    // 禁用/启用 GPU 下拉项
    auto* model = qobject_cast<QStandardItemModel*>(deviceCombo_->model());
    if (model) {
        // "gpu:0" 是第二个 item (index 1)
        auto* gpuItem = model->item(1);
        if (gpuItem) {
            if (gpuOk) {
                gpuItem->setFlags(gpuItem->flags() | Qt::ItemIsEnabled);
            } else {
                gpuItem->setFlags(gpuItem->flags() & ~Qt::ItemIsEnabled);
            }
        }
    }

    // 如果当前选了 GPU 但不支持，自动切回 CPU
    if (!gpuOk && deviceCombo_->currentText().contains("gpu")) {
        deviceCombo_->setCurrentIndex(0);  // cpu
    }

    // 显示 GPU 状态标签
    if (deviceCombo_->currentText() == "cpu" && gpuOk) {
        // GPU 可用但选了 CPU — 显示提示
        gpuStatusLabel_->setText(info.summary());
        gpuStatusLabel_->setStyleSheet("font:8pt 'Microsoft YaHei';color:#999;padding-left:4px;");
        gpuStatusLabel_->show();
    } else if (!gpuOk) {
        // GPU 不可用 — 显示原因
        gpuStatusLabel_->setText(info.summary());
        gpuStatusLabel_->setStyleSheet("font:8pt 'Microsoft YaHei';color:#d32f2f;padding-left:4px;");
        gpuStatusLabel_->show();
    } else {
        // GPU 已选中且可用
        gpuStatusLabel_->setText(info.summary());
        gpuStatusLabel_->setStyleSheet("font:8pt 'Microsoft YaHei';color:#22a55e;padding-left:4px;");
        gpuStatusLabel_->show();
    }
}

#include "settings_page.moc"
