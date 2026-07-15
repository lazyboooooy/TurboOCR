#pragma once
#include <QColor>
#include <QObject>
#include <QString>
#include <QHash>

// ============================================================================
// ThemeColors
// ============================================================================
struct ThemeColors {
    QString name;            // 显示名称
    QColor  centralBg, centralBorder;
    QColor  titleText, titleBtnBg, titleBtnText, titleBtnHoverBg, titleBtnPressedBg;
    QColor  tabUnselectedText, tabSelectedBg, tabSelectedText;
    QColor  tabSelectedBorder, tabSelectedBottomBorder, tabHoverBg, tabHoverText;
    QColor  btnBg, btnBorder, btnText, btnHoverBg, btnHoverBorder, btnPressedBg;
    QColor  btnDisabledBg, btnDisabledBorder, btnDisabledText;
    QColor  statusText;
    QColor  pageBg, rightPanelBg;
    QColor  infoBarBg, infoBarBorder;
    QColor  labelPrimary, labelSecondary, labelTertiary, labelAccent;
    QColor  historyBg, historyItemHoverBg, historyItemSelectedBg;
    QColor  toolbarBg, toolbarBorder, toolBtnText, toolBtnHoverBg, toolBtnHoverText;
    QColor  modeBtnCheckedText, modeBtnUncheckedText;
    QColor  modeBtnPrimaryBg, modeBtnPrimaryBorder, modeBtnPrimaryHoverBg;
    QColor  modeBtnNonPrimaryBg, modeBtnNonPrimaryBorder, modeBtnNonPrimaryHoverBg;
    QColor  pageBtnBg, pageBtnBorder, pageBtnHoverBg, pageBtnHoverBorder;
    QColor  pageBtnDisabledBg, pageBtnDisabledBorder;
    QColor  splitterHandle;
    QColor  imageViewBg, boxNormalPen, boxNormalBrush, boxHoverPen, boxHoverBrush, boxExportPen;
    QColor  textPreviewText, textPreviewBg, textSelectionBg, textHighlightBg;
    QColor  jsonText, jsonKeyword, jsonString, jsonNumber, jsonKey;
    QColor  tableText, tableBg, tableBorder, tableGridline, tableHeaderBg, tableSelectionBg;
    QColor  vertLayoutBg, vertLayoutBorder, vertLayoutText;
    QColor  correctModeBg, correctModeBorder;
    QColor  overlayBg, overlayErrorBg, overlayText, overlayErrorText;
    QColor  spinnerTrack, spinnerArc;
    QColor  captureDimBg, captureBorder, captureLabelText, captureLabelBg;
    QColor  actionPrimaryBg, actionPrimaryText;
    QColor  actionDangerBg, actionDangerText;
    QColor  settingsApplyBg, settingsApplyText;
};

// ============================================================================
// rgba 辅助
// ============================================================================
inline QString rgbaStr(const QColor& c) {
    return QString("rgba(%1,%2,%3,%4%)")
        .arg(c.red()).arg(c.green()).arg(c.blue())
        .arg(c.alphaF() * 100.0, 0, 'f', 0);
}

// ============================================================================
// 白色主题 — 保持所有当前颜色值不变
// ============================================================================
inline ThemeColors makeDefaultTheme() {
    ThemeColors t;
    t.name                  = "白色";
    t.centralBg             = QColor("#F6F4F3");
    t.centralBorder         = QColor("#E9E1DB");
    t.titleText             = QColor("#2E241F");
    t.titleBtnBg            = QColor(62, 48, 40, 13);
    t.titleBtnText          = QColor("#4A3A32");
    t.titleBtnHoverBg       = QColor(62, 48, 40, 26);
    t.titleBtnPressedBg     = QColor(62, 48, 40, 38);
    t.tabUnselectedText     = QColor("#7A6F69");
    t.tabSelectedBg         = QColor("#FFFFFF");
    t.tabSelectedText       = QColor("#B76223");
    t.tabSelectedBorder     = QColor("#E9DED6");
    t.tabSelectedBottomBorder = QColor("#FF7F45");
    t.tabHoverBg            = QColor(255, 255, 255, 128);
    t.tabHoverText          = QColor("#4A3A32");
    t.btnBg                 = QColor("#FFFFFF");
    t.btnBorder             = QColor("#E7DBD3");
    t.btnText               = QColor("#4A3A32");
    t.btnHoverBg            = QColor("#FFF8F3");
    t.btnHoverBorder        = QColor("#E7A880");
    t.btnPressedBg          = QColor("#FFEBDD");
    t.btnDisabledBg         = QColor("#F0EDEA");
    t.btnDisabledBorder     = QColor("#E0D9D3");
    t.btnDisabledText       = QColor("#B0A8A2");
    t.statusText            = QColor("#7A6F69");
    t.pageBg                = QColor("#f5f7fa");
    t.rightPanelBg          = QColor("#fff");
    t.infoBarBg             = QColor("#fff");
    t.infoBarBorder         = QColor("#e5e7eb");
    t.labelPrimary          = QColor("#333");
    t.labelSecondary        = QColor("#888");
    t.labelTertiary         = QColor("#999");
    t.labelAccent           = QColor("#555");
    t.historyBg             = QColor("#fafbfc");
    t.historyItemHoverBg    = QColor("#e8ecf2");
    t.historyItemSelectedBg = QColor("#dceaff");
    t.toolbarBg             = QColor("#fafbfc");
    t.toolbarBorder         = QColor("#e8ecf1");
    t.toolBtnText           = QColor("#555");
    t.toolBtnHoverBg        = QColor("#e8ecf2");
    t.toolBtnHoverText      = QColor("#333");
    t.modeBtnCheckedText    = QColor("#1a1a1a");
    t.modeBtnUncheckedText  = QColor("#888");
    t.modeBtnPrimaryBg      = QColor("#f0f6ff");
    t.modeBtnPrimaryBorder  = QColor("#6096ff");
    t.modeBtnPrimaryHoverBg = QColor("#dceaff");
    t.modeBtnNonPrimaryBg   = QColor("#f5f7fa");
    t.modeBtnNonPrimaryBorder = QColor("#ddd");
    t.modeBtnNonPrimaryHoverBg = QColor("#e8ecf2");
    t.pageBtnBg             = QColor("#d8dce3");
    t.pageBtnBorder         = QColor("#a0a5b0");
    t.pageBtnHoverBg        = QColor("#b8bdc8");
    t.pageBtnHoverBorder    = QColor("#808590");
    t.pageBtnDisabledBg     = QColor("#eceef2");
    t.pageBtnDisabledBorder = QColor("#d0d4da");
    t.splitterHandle        = QColor("#e0e3ea");
    t.imageViewBg           = QColor("#fafbfc");
    t.boxNormalPen          = QColor("#4488ff");
    t.boxNormalBrush        = QColor(68, 136, 255, 25);
    t.boxHoverPen           = QColor("#0066ff");
    t.boxHoverBrush         = QColor(0, 102, 255, 60);
    t.boxExportPen          = QColor("#ff4444");
    t.textPreviewText       = QColor("#1a1a1a");
    t.textPreviewBg         = QColor("#fcfcfc");
    t.textSelectionBg       = QColor("#b8d4fe");
    t.textHighlightBg       = QColor("#b8d4fe");
    t.jsonText              = QColor("#24292e");
    t.jsonKeyword           = QColor("#d73a49");
    t.jsonString            = QColor("#032f62");
    t.jsonNumber            = QColor("#005cc5");
    t.jsonKey               = QColor("#6f42c1");
    t.tableText             = QColor("#1a1a1a");
    t.tableBg               = QColor("#fff");
    t.tableBorder           = QColor("#ddd");
    t.tableGridline         = QColor("#e8e8e8");
    t.tableHeaderBg         = QColor("#f5f7fa");
    t.tableSelectionBg      = QColor("#b8d4fe");
    t.vertLayoutBg          = QColor("#fcfcfc");
    t.vertLayoutBorder      = QColor("#e8e8e8");
    t.vertLayoutText        = QColor("#1a1a1a");
    t.correctModeBg         = QColor("#fffef9");
    t.correctModeBorder     = QColor("#f0a040");
    t.overlayBg             = QColor(0, 0, 0, 80);
    t.overlayErrorBg        = QColor(0, 0, 0, 100);
    t.overlayText           = QColor("#ffffff");
    t.overlayErrorText      = QColor("#ff6b6b");
    t.spinnerTrack          = QColor(255, 255, 255, 40);
    t.spinnerArc            = QColor("#5b8def");
    t.captureDimBg          = QColor(0, 0, 0, 80);
    t.captureBorder         = QColor("#5b8def");
    t.captureLabelText      = QColor(Qt::white);
    t.captureLabelBg        = QColor(0, 0, 0, 160);
    t.actionPrimaryBg       = QColor("#0078d4");
    t.actionPrimaryText     = QColor(Qt::white);
    t.actionDangerBg        = QColor("#d32f2f");
    t.actionDangerText      = QColor(Qt::white);
    t.settingsApplyBg       = QColor("#0078d4");
    t.settingsApplyText     = QColor(Qt::white);
    return t;
}

// ============================================================================
// 暗色主题 — GTA3 工业风深蓝黑底 + 金/黄/红强调色
// ============================================================================
inline ThemeColors makeGtaTheme() {
    ThemeColors t = makeDefaultTheme();
    // GTA III Definitive Edition 菜单风格
    // 深蓝暗底 + 金/黄强调 + 硬朗工业风
    t.name                  = "暗色";
    t.centralBg             = QColor("#0F1923");   // 深蓝黑底
    t.centralBorder         = QColor("#2A3A4A");   // 硬边深灰蓝边框
    t.titleText             = QColor("#FFFFFF");   // 标题纯白粗体
    t.titleBtnBg            = QColor(255, 255, 255, 15);
    t.titleBtnText          = QColor("#CCCCCC");
    t.titleBtnHoverBg       = QColor(255, 255, 255, 30);
    t.titleBtnPressedBg     = QColor(200, 168, 96, 40);  // 金色按压
    t.tabUnselectedText     = QColor("#8899AA");
    t.tabSelectedBg         = QColor("#162231");
    t.tabSelectedText       = QColor("#C8A860");   // 哑光金
    t.tabSelectedBorder     = QColor("#2A3A4A");
    t.tabSelectedBottomBorder = QColor("#C8A860"); // 金底边
    t.tabHoverBg            = QColor(200, 168, 96, 15);
    t.tabHoverText          = QColor("#FFC107");   // 亮黄
    t.btnBg                 = QColor("#162231");
    t.btnBorder             = QColor("#2A3A4A");
    t.btnText               = QColor("#CCCCCC");
    t.btnHoverBg            = QColor("#1A2A3A");
    t.btnHoverBorder        = QColor("#C8A860");
    t.btnPressedBg          = QColor("#0F1A25");
    t.btnDisabledBg         = QColor("#0F1923");
    t.btnDisabledBorder     = QColor("#222222");
    t.btnDisabledText       = QColor("#555555");
    t.statusText            = QColor("#8899AA");
    t.pageBg                = QColor("#0F1923");
    t.rightPanelBg          = QColor("#162231");
    t.infoBarBg             = QColor("#162231");
    t.infoBarBorder         = QColor("#2A3A4A");
    t.labelPrimary          = QColor("#EEEEEE");   // 白色主文字
    t.labelSecondary        = QColor("#8899AA");   // 灰蓝辅助
    t.labelTertiary         = QColor("#667788");
    t.labelAccent           = QColor("#C8A860");   // 金色强调
    t.historyBg             = QColor("#0F1923");
    t.historyItemHoverBg    = QColor(200, 168, 96, 15);
    t.historyItemSelectedBg = QColor(200, 168, 96, 35);
    t.toolbarBg             = QColor("#162231");
    t.toolbarBorder         = QColor("#2A3A4A");
    t.toolBtnText           = QColor("#8899AA");
    t.toolBtnHoverBg        = QColor(200, 168, 96, 15);
    t.toolBtnHoverText      = QColor("#FFC107");
    t.modeBtnCheckedText    = QColor("#FFFFFF");
    t.modeBtnUncheckedText  = QColor("#8899AA");
    t.modeBtnPrimaryBg      = QColor(200, 168, 96, 18);
    t.modeBtnPrimaryBorder  = QColor("#C8A860");
    t.modeBtnPrimaryHoverBg = QColor(200, 168, 96, 30);
    t.modeBtnNonPrimaryBg   = QColor("#0F1923");
    t.modeBtnNonPrimaryBorder = QColor("#2A3A4A");
    t.modeBtnNonPrimaryHoverBg = QColor("#162231");
    t.pageBtnBg             = QColor("#162231");
    t.pageBtnBorder         = QColor("#2A3A4A");
    t.pageBtnHoverBg        = QColor("#0F1923");
    t.pageBtnHoverBorder    = QColor("#C8A860");
    t.pageBtnDisabledBg     = QColor("#0F1923");
    t.pageBtnDisabledBorder = QColor("#222222");
    t.splitterHandle        = QColor("#2A3A4A");
    t.imageViewBg           = QColor("#0A121B");
    t.boxNormalPen          = QColor("#C8A860");   // 金框
    t.boxNormalBrush        = QColor(200, 168, 96, 25);
    t.boxHoverPen           = QColor("#FFC107");   // 亮黄悬停
    t.boxHoverBrush         = QColor(255, 193, 7, 50);
    t.boxExportPen          = QColor("#E02020");   // 红色标注
    t.textPreviewText       = QColor("#EEEEEE");
    t.textPreviewBg         = QColor("#0F1923");
    t.textSelectionBg       = QColor(200, 168, 96, 60);
    t.textHighlightBg       = QColor(200, 168, 96, 60);
    t.jsonText              = QColor("#CCCCCC");
    t.jsonKeyword           = QColor("#E02020");   // 红色关键字
    t.jsonString            = QColor("#C8A860");   // 金色字符串
    t.jsonNumber            = QColor("#FFC107");   // 黄色数字
    t.jsonKey               = QColor("#D4B068");   // 浅金键名
    t.tableText             = QColor("#EEEEEE");
    t.tableBg               = QColor("#162231");
    t.tableBorder           = QColor("#2A3A4A");
    t.tableGridline         = QColor("#2A3A4A");
    t.tableHeaderBg         = QColor("#0F1923");
    t.tableSelectionBg      = QColor(200, 168, 96, 45);
    t.vertLayoutBg          = QColor("#0F1923");
    t.vertLayoutBorder      = QColor("#2A3A4A");
    t.vertLayoutText        = QColor("#EEEEEE");
    t.correctModeBg         = QColor(255, 193, 7, 10);
    t.correctModeBorder     = QColor("#FFC107");
    t.overlayBg             = QColor(0, 0, 0, 110);
    t.overlayErrorBg        = QColor(0, 0, 0, 130);
    t.overlayText           = QColor("#FFFFFF");
    t.overlayErrorText      = QColor("#E02020");
    t.spinnerTrack          = QColor(200, 168, 96, 25);
    t.spinnerArc            = QColor("#FFC107");   // 金色旋转弧
    t.captureDimBg          = QColor(0, 0, 0, 130);
    t.captureBorder         = QColor("#FFC107");   // 黄色选区框
    t.captureLabelText      = QColor("#FFFFFF");
    t.captureLabelBg        = QColor(0, 0, 0, 180);
    t.actionPrimaryBg       = QColor("#FFC107");   // 亮黄色主按钮
    t.actionPrimaryText     = QColor("#000000");   // 黑色文字
    t.actionDangerBg        = QColor("#E02020");   // 红色危险按钮
    t.actionDangerText      = QColor("#FFFFFF");
    t.settingsApplyBg       = QColor("#C8A860");   // 金色应用按钮
    t.settingsApplyText     = QColor("#000000");
    return t;
}

// ============================================================================
// 惰性单例常量
// ============================================================================
inline const ThemeColors& kDefaultTheme() { static ThemeColors v = makeDefaultTheme(); return v; }
inline const ThemeColors& kGtaTheme()     { static ThemeColors v = makeGtaTheme();     return v; }

// ============================================================================
// ThemeManager
// ============================================================================
class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager& instance() {
        static ThemeManager mgr;
        return mgr;
    }

    const ThemeColors& currentTheme() const { return *current_; }
    QString currentThemeName() const { return currentName_; }
    QStringList themeNames() const { return themes_.keys(); }

public slots:
    void setTheme(const QString& name) {
        if (currentName_ == name) return;
        auto it = themes_.find(name);
        if (it == themes_.end()) return;
        current_ = &it.value();
        currentName_ = name;
        emit themeChanged(*current_);
    }

signals:
    void themeChanged(const ThemeColors& theme);

private:
    ThemeManager() {
        registerTheme(kDefaultTheme());
        registerTheme(kGtaTheme());
        current_ = &themes_["白色"];
        currentName_ = "白色";
    }
    void registerTheme(const ThemeColors& t) { themes_.insert(t.name, t); }

    QHash<QString, ThemeColors> themes_;
    const ThemeColors* current_ = nullptr;
    QString currentName_;
};

// ============================================================================
// 主窗口样式表构建
// ============================================================================
inline QString buildMainWindowStylesheet(const ThemeColors& t) {
    return QString(
        "QMainWindow { background: transparent; }"
        "#centralWidget { background-color: %1; border: 1px solid %2; border-radius: 26px; }"
        "#titleBar { background: transparent; border: none; }"
        "#titleLabel { font-size: 20pt; font-weight: 700; color: %3; }"
        "#titleBar QPushButton {"
        "  min-width: 32px; max-width: 32px; min-height: 32px; max-height: 32px;"
        "  border-radius: 16px; padding: 0px; border: none;"
        "  background-color: %4; color: %5; font-size: 14px; font-weight: bold; }"
        "#titleBar QPushButton:hover { background-color: %6; }"
        "#titleBar QPushButton:pressed { background-color: %7; }"
        "#titleBar QPushButton#closeBtn:hover { background-color: #c0392b; color: white; }"
        "QTabWidget::pane { border: none; background: transparent; }"
        "QTabBar::tab { min-width: 100px; max-width: 9999px; padding: 8px 24px;"
        "  font: 9pt 'Microsoft YaHei'; font-weight: 600; background: transparent; border: none;"
        "  color: %8; margin-right: 2px; border-radius: 12px 12px 0px 0px; }"
        "QTabBar::tab:selected { background: %9; color: %10; border: 1px solid %11;"
        "  border-bottom: 2px solid %12; }"
        "QTabBar::tab:hover:!selected { color: %13; background: %14; }"
        "QPushButton { border-radius: 18px; padding: 12px 18px; min-height: 24px;"
        "  font-size: 11pt; font-weight: 600; background-color: %15; border: 1px solid %16;"
        "  color: %17; }"
        "QPushButton:hover { background-color: %18; border-color: %19; }"
        "QPushButton:pressed { background-color: %20; }"
        "QPushButton:disabled { background-color: %21; border-color: %22; color: %23; }"
        "#statusLabel { font: 9pt 'Microsoft YaHei'; color: %24; padding: 4px 12px; min-height: 28px; }"
    )
        .arg(t.centralBg.name(QColor::HexArgb), t.centralBorder.name(QColor::HexArgb),
             t.titleText.name(QColor::HexArgb),
             rgbaStr(t.titleBtnBg), t.titleBtnText.name(QColor::HexArgb),
             rgbaStr(t.titleBtnHoverBg), rgbaStr(t.titleBtnPressedBg),
             t.tabUnselectedText.name(QColor::HexArgb), t.tabSelectedBg.name(QColor::HexArgb),
             t.tabSelectedText.name(QColor::HexArgb), t.tabSelectedBorder.name(QColor::HexArgb),
             t.tabSelectedBottomBorder.name(QColor::HexArgb),
             t.tabHoverText.name(QColor::HexArgb), rgbaStr(t.tabHoverBg),
             t.btnBg.name(QColor::HexArgb), t.btnBorder.name(QColor::HexArgb),
             t.btnText.name(QColor::HexArgb), t.btnHoverBg.name(QColor::HexArgb),
             t.btnHoverBorder.name(QColor::HexArgb), t.btnPressedBg.name(QColor::HexArgb),
             t.btnDisabledBg.name(QColor::HexArgb), t.btnDisabledBorder.name(QColor::HexArgb),
             t.btnDisabledText.name(QColor::HexArgb), t.statusText.name(QColor::HexArgb));
}
