#pragma once
#include <QtWidgets/QWidget>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QKeySequenceEdit>
#include <QtWidgets/QLabel>
#include <QMutex>

class MainWindow;
class LoadingOverlay;

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(MainWindow* mainWindow, QWidget* parent = nullptr);

private slots:
    void applySettings();

private:
    void setupUi();
    void loadFromConfig();
    void applyThemePreview();
    void updateGpuStatus();

    MainWindow* mainWindow_;

    QComboBox*      modelSizeCombo_ = nullptr;  // 模型大小选择
    QComboBox*      deviceCombo_ = nullptr;
    QLabel*         gpuStatusLabel_ = nullptr;  // GPU 兼容性提示
    QSpinBox*       threadsSpin_ = nullptr;
    QDoubleSpinBox* detThreshSpin_ = nullptr;
    QDoubleSpinBox* detBoxThreshSpin_ = nullptr;
    QDoubleSpinBox* detUnclipSpin_ = nullptr;
    QDoubleSpinBox* recScoreThreshSpin_ = nullptr;
    QSpinBox*       recBatchSpin_ = nullptr;
    QPushButton*    applyBtn_ = nullptr;
    QPushButton*    resetBtn_ = nullptr;

    // 主题 + 语言
    QComboBox*      themeCombo_ = nullptr;
    QComboBox*      langCombo_ = nullptr;
    bool            loadingConfig_ = false;

    // 热键
    QKeySequenceEdit* hotkeyEdit_ = nullptr;
    QCheckBox*        hotkeyEnableChk_ = nullptr;
    QCheckBox*        backgroundChk_ = nullptr;

    LoadingOverlay* overlay_ = nullptr;
    QMutex          mutex_;
    bool            loading_ = false;
};
