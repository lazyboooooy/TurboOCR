#include "calligraphy_ocr_page.h"
#include "../main_window.h"
#include "../core/loading_overlay.h"
#include "../core/ocr_history.h"
#include "../core/pdf_render.h"
#include "../core/screen_capture.h"
#include "../core/theme.h"
#include "../core/i18n.h"
#include "../core/logger.h"
#include "../core/qrcode_scanner.h"
#include "../ocr/engine.h"
#include "../postprocess/parsers.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QDialog>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtCore/QFileInfo>
#include <QApplication>
#include <QClipboard>
#include <QShortcut>
#include <QHeaderView>
#include <QThread>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QPainter>
#include <QShowEvent>
#include <QResizeEvent>
#include <QTimer>
#include <cmath>
#include <algorithm>

// ======================== JsonHighlighter ========================
JsonHighlighter::JsonHighlighter(QTextDocument* parent) : QSyntaxHighlighter(parent) {
    const auto& tj = ThemeManager::instance().currentTheme();
    QTextCharFormat kw; kw.setForeground(tj.jsonKeyword); kw.setFontWeight(QFont::Bold);
    rules_.push_back({QRegularExpression("\\b(true|false|null)\\b"), kw});
    QTextCharFormat s; s.setForeground(tj.jsonString);
    rules_.push_back({QRegularExpression("\"[^\"]*\""), s});
    QTextCharFormat n; n.setForeground(tj.jsonNumber);
    rules_.push_back({QRegularExpression("\\b-?[0-9]+\\.?[0-9]*\\b"), n});
    QTextCharFormat p; p.setForeground(tj.jsonKey);
    rules_.push_back({QRegularExpression("\"[^\"]+\"\\s*:"), p});
}
void JsonHighlighter::highlightBlock(const QString& text) {
    for (auto& r : rules_) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) { auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), r.format); }
    }
}

// ======================== ImagePreviewView ========================
ImagePreviewView::ImagePreviewView(QWidget* parent) : QGraphicsView(parent) {
    scene_ = new QGraphicsScene(this); setScene(scene_);
    setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);
    setDragMode(NoDrag); setTransformationAnchor(NoAnchor); setResizeAnchor(NoAnchor);
    setViewportUpdateMode(SmartViewportUpdate); setCacheMode(CacheBackground);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QString("QGraphicsView { border: none; background: %1; }")
        .arg(ThemeManager::instance().currentTheme().imageViewBg.name()));
    pixmapItem_ = new QGraphicsPixmapItem(); scene_->addItem(pixmapItem_);
}
void ImagePreviewView::setImage(const QPixmap& pm) {
    clearTextBoxes();
    pixmapItem_->setPixmap(pm);
    scene_->setSceneRect(pm.rect());
    initialFitDone_ = false;
    fitToWidth();
}
void ImagePreviewView::clearScene() { clearTextBoxes(); pixmapItem_->setPixmap(QPixmap()); scene_->setSceneRect(0,0,400,300); initialFitDone_ = false; }
void ImagePreviewView::fitToWidth() {
    if (pixmapItem_->pixmap().isNull()) return;
    resetTransform();
    QRectF r = pixmapItem_->boundingRect();
    double vw = viewport()->width() - 4;
    double pw = r.width();
    if (pw <= 0) return;
    double s = vw / pw;
    zoomLevel_ = s;
    QTransform t; t.scale(s, s); setTransform(t);
    setSceneRect(r);
}
// ======================== 框样式 ========================
void ImagePreviewView::setBoxStyle(QGraphicsRectItem* item, bool hovered) {
    const auto& t = ThemeManager::instance().currentTheme();
    if (hovered) {
        item->setPen(QPen(t.boxHoverPen, 2.5));
        item->setBrush(t.boxHoverBrush);
    } else {
        item->setPen(QPen(t.boxNormalPen, 1.5));
        item->setBrush(t.boxNormalBrush);
    }
}

void ImagePreviewView::drawTextBoxes(const std::vector<std::array<int,4>>& boxes) {
    clearTextBoxes();
    for (size_t i = 0; i < boxes.size(); ++i) {
        auto& b = boxes[i]; int w = b[2]-b[0], h = b[3]-b[1]; if (w<=0||h<=0) continue;
        auto* r = new QGraphicsRectItem(b[0],b[1],w,h);
        setBoxStyle(r, false);
        r->setZValue(1); r->setData(0,(int)i);
        r->setAcceptHoverEvents(true);
        scene_->addItem(r); rectItems_.push_back(r);
    }
    hoveredIndex_ = -1;
}

void ImagePreviewView::drawQrBoxes(const std::vector<std::array<int,4>>& boxes) {
    for (auto& b : boxes) {
        int w = b[2], h = b[3]; if (w <= 0 || h <= 0) continue;
        auto* r = new QGraphicsRectItem(b[0], b[1], w, h);
        r->setPen(QPen(QColor("#22c55e"), 2.5));
        r->setBrush(QColor(34, 197, 94, 50));
        r->setZValue(2);
        scene_->addItem(r); rectItems_.push_back(r);
    }
}

void ImagePreviewView::clearTextBoxes() { hoveredIndex_=-1; for(auto* r:rectItems_){scene_->removeItem(r);delete r;} rectItems_.clear(); }
void ImagePreviewView::fitToWindow() { if(!pixmapItem_->pixmap().isNull()){fitInView(pixmapItem_,Qt::KeepAspectRatio);zoomLevel_=transform().m11();} }

int ImagePreviewView::itemIndexAtPos(const QPoint& pos) const {
    QGraphicsItem* it = scene_->itemAt(mapToScene(pos), transform());
    if (it && it != pixmapItem_ && it->data(0).isValid())
        return it->data(0).toInt();
    return -1;
}

void ImagePreviewView::wheelEvent(QWheelEvent* e) {
    // 兼容普通滚轮 (y) 和 Ctrl+滚轮产生的水平事件 (x)
    QPoint ad = e->angleDelta();
    int delta = (abs(ad.y()) >= abs(ad.x())) ? ad.y() : ad.x();
    if (delta == 0) { e->accept(); return; }

    double f = (delta > 0) ? 1.15 : 1.0 / 1.15;
    zoomLevel_ *= f;
    if (zoomLevel_ < 0.005) zoomLevel_ = 0.005;
    QTransform t; t.scale(zoomLevel_, zoomLevel_); setTransform(t);
    e->accept();
}

void ImagePreviewView::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        panning_ = false;
        lastPanPoint_ = e->pos();
        clickedBoxIndex_ = itemIndexAtPos(e->pos());
    }
    QGraphicsView::mousePressEvent(e);
}

void ImagePreviewView::mouseMoveEvent(QMouseEvent* e) {
    QPoint d = e->pos() - lastPanPoint_;
    if (panning_) {
        // 正在拖动平移
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
        lastPanPoint_ = e->pos();
    } else if ((e->buttons() & Qt::LeftButton) && (abs(d.x()) > 3 || abs(d.y()) > 3)) {
        // 按住左键 + 超过阈值 → 开始平移
        panning_ = true;
        clickedBoxIndex_ = -1;
        setCursor(Qt::ClosedHandCursor);
    }
    if (!panning_) lastPanPoint_ = e->pos();
    int idx = itemIndexAtPos(e->pos());
    if (idx != hoveredIndex_) {
        if (hoveredIndex_ >= 0 && hoveredIndex_ < (int)rectItems_.size())
            setBoxStyle(rectItems_[hoveredIndex_], false);
        hoveredIndex_ = idx;
        if (hoveredIndex_ >= 0 && hoveredIndex_ < (int)rectItems_.size())
            setBoxStyle(rectItems_[hoveredIndex_], true);
        emit textBoxHovered(hoveredIndex_);
    }
    QGraphicsView::mouseMoveEvent(e);
}

void ImagePreviewView::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        if (!panning_ && clickedBoxIndex_ >= 0) {
            emit textBoxClicked(clickedBoxIndex_);
        }
        panning_ = false;
        clickedBoxIndex_ = -1;
        setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseReleaseEvent(e);
}

void ImagePreviewView::leaveEvent(QEvent* e) {
    if (hoveredIndex_ >= 0 && hoveredIndex_ < (int)rectItems_.size())
        setBoxStyle(rectItems_[hoveredIndex_], false);
    hoveredIndex_ = -1;
    emit textBoxHovered(-1);
    QGraphicsView::leaveEvent(e);
}

void ImagePreviewView::highlightTextBox(int index) {
    // 还原所有框
    for (auto* r : rectItems_) setBoxStyle(r, false);
    if (index >= 0 && index < (int)rectItems_.size()) {
        setBoxStyle(rectItems_[index], true);
        hoveredIndex_ = index;
    }
}

void ImagePreviewView::resizeEvent(QResizeEvent* e) {
    QGraphicsView::resizeEvent(e);
    // 只在首次加载时适配宽度，之后保留用户缩放状态
    if (!initialFitDone_) {
        initialFitDone_ = true;
        fitToWidth();
    }
}

// ======================== CalligraphyOcrPage ========================
CalligraphyOcrPage::CalligraphyOcrPage(MainWindow* mw, QWidget* parent) : QWidget(parent), mainWindow_(mw) {
    setAcceptDrops(true);
    tts_ = new QTextToSpeech(this);

    // 初始化 OCR 历史数据库
    history_ = new OcrHistory();
    QString dbPath = QApplication::applicationDirPath() + "/ocr_history.db";
    history_->init(dbPath);

    setupUi();
    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
        this, &CalligraphyOcrPage::applyTheme);
}

void CalligraphyOcrPage::setupUi() {
    auto* ml = new QHBoxLayout(this); ml->setContentsMargins(0,0,0,0); ml->setSpacing(0);
    mainSplitter_ = new QSplitter(Qt::Horizontal); mainSplitter_->setHandleWidth(4);
    mainSplitter_->setStyleSheet(QString("QSplitter::handle { background: %1; }")
        .arg(ThemeManager::instance().currentTheme().splitterHandle.name()));

    // ==== 左侧面板 ====
    leftPanel_ = new QWidget; leftPanel_->setMinimumWidth(280);
    auto* ll = new QVBoxLayout(leftPanel_); ll->setContentsMargins(0,0,0,0); ll->setSpacing(0);
    auto* infoBar = new QWidget; infoBar->setObjectName("infoBar"); infoBar->setFixedHeight(38);
    const auto& ct = ThemeManager::instance().currentTheme();
    infoBar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
        .arg(ct.infoBarBg.name(), ct.infoBarBorder.name()));
    auto* il = new QHBoxLayout(infoBar); il->setContentsMargins(10,6,10,6); il->setSpacing(6);
    sourceTitleLabel_ = new QLabel(_T("源文件")); sourceTitleLabel_->setStyleSheet(QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;").arg(ct.labelSecondary.name())); il->addWidget(sourceTitleLabel_);
    fileNameLabel_ = new QLabel(_T("未选择文件")); fileNameLabel_->setStyleSheet(QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;").arg(ct.labelPrimary.name())); fileNameLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); il->addWidget(fileNameLabel_);
    fileSizeLabel_ = new QLabel(""); fileSizeLabel_->setStyleSheet(QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;").arg(ct.labelTertiary.name())); il->addWidget(fileSizeLabel_);
    // PDF 翻页按钮
    il->addStretch();
    pageLabel_ = new QLabel; pageLabel_->setStyleSheet(QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;").arg(ct.labelAccent.name())); pageLabel_->setVisible(false);
    il->addWidget(pageLabel_);
    // PDF 翻页按钮 — 从图标文件加载并正确缩放
    auto makePageBtn = [&](const QString& iconPath, const QString& tip)->QPushButton* {
        auto* btn = new QPushButton; btn->setFixedSize(30,30); btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        QPixmap pix(iconPath);
        if (!pix.isNull()) {
            QPixmap scaled = pix.scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            btn->setIcon(QIcon(scaled));
            btn->setIconSize(QSize(18, 18));
        } else {
            btn->setText(tip == _T("上一页") ? "◀" : "▶");
        }
        btn->setStyleSheet(QString(
            "QPushButton{padding:0;background:%1;border:1px solid %2;border-radius:4px;}"
            "QPushButton:hover{background:%3;border-color:%4;}"
            "QPushButton:disabled{background:%5;border-color:%6;}")
            .arg(ct.pageBtnBg.name(), ct.pageBtnBorder.name(),
                 ct.pageBtnHoverBg.name(), ct.pageBtnHoverBorder.name(),
                 ct.pageBtnDisabledBg.name(), ct.pageBtnDisabledBorder.name()));
        return btn;
    };
    QString iconsDir = QApplication::applicationDirPath() + "/icons/";
    prevPageBtn_ = makePageBtn(iconsDir + "page_prev.png", _T("上一页"));
    prevPageBtn_->setVisible(false);
    il->addWidget(prevPageBtn_);
    nextPageBtn_ = makePageBtn(iconsDir + "page_next.png", _T("下一页"));
    nextPageBtn_->setVisible(false);
    il->addWidget(nextPageBtn_);
    ll->addWidget(infoBar);
    imageView_ = new ImagePreviewView; imageView_->setAcceptDrops(false); imageView_->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding); ll->addWidget(imageView_);
    overlay_ = new LoadingOverlay(leftPanel_); overlay_->raise();

    // ==== 历史记录面板（最左侧） ====
    auto* historyPanel = new QWidget;
    historyPanel->setMinimumWidth(120); historyPanel->setMaximumWidth(200);
    historyPanel->setStyleSheet("background:#fafbfc;");
    auto* hl = new QVBoxLayout(historyPanel); hl->setContentsMargins(4,4,4,4); hl->setSpacing(4);
    auto* histLabel = new QLabel(_T("历史记录"));
    histLabel->setStyleSheet("font:9pt 'Microsoft YaHei';font-weight:bold;color:#555;background:transparent;border:none;padding:4px;");
    hl->addWidget(histLabel);
    historyList_ = new QListWidget;
    historyList_->setMouseTracking(true);
    historyList_->setViewMode(QListView::IconMode);
    historyList_->setIconSize(QSize(100, 75));
    historyList_->setSpacing(2);
    historyList_->setGridSize(QSize(108, 83));
    historyList_->setResizeMode(QListView::Adjust);
    historyList_->setWrapping(true);
    historyList_->setContextMenuPolicy(Qt::CustomContextMenu);
    historyList_->setStyleSheet("QListWidget{background:transparent;border:none;}QListWidget::item{border-radius:4px;padding:2px;}QListWidget::item:hover{background:#e8ecf2;}QListWidget::item:selected{background:#dceaff;}");
    connect(historyList_, &QListWidget::itemClicked, this, &CalligraphyOcrPage::onHistoryClicked);
    connect(historyList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = historyList_->itemAt(pos);
        if (!item) return;
        int id = item->data(Qt::UserRole).toInt();
        QMenu menu;
        QAction* delAction = menu.addAction(_T("删除"));
        if (menu.exec(historyList_->mapToGlobal(pos)) == delAction) {
            if (history_) { history_->remove(id); refreshHistory(); }
        }
    });
    hl->addWidget(historyList_);
    mainSplitter_->addWidget(historyPanel);

    mainSplitter_->addWidget(leftPanel_);

    // ==== 右侧面板 ====
    rightPanel_ = new QWidget; rightPanel_->setObjectName("rightPanel"); rightPanel_->setMinimumWidth(380);
    rightPanel_->setStyleSheet(QString("background:%1;").arg(ct.rightPanelBg.name()));
    auto* rl = new QVBoxLayout(rightPanel_); rl->setContentsMargins(0,0,0,0); rl->setSpacing(0);
    // 工具栏
    auto* tb = new QWidget; tb->setObjectName("ocrToolbar"); tb->setFixedHeight(48);
    tb->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
        .arg(ct.toolbarBg.name(), ct.toolbarBorder.name()));
    auto* tbl = new QHBoxLayout(tb); tbl->setContentsMargins(10,8,10,8); tbl->setSpacing(8);
    auto mkBtn = [&](const QString& txt, bool chk, bool primary)->QPushButton*{
        auto* b = new QPushButton(txt); b->setCheckable(chk); b->setMinimumWidth(72); b->setFixedHeight(32); b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton{font:9pt 'Microsoft YaHei';color:%1;background:%2;border:1px solid %3;border-radius:4px;padding:4px 14px;}"
            "QPushButton:hover{background:%4;}%5")
            .arg(primary ? ct.modeBtnCheckedText.name() : ct.modeBtnUncheckedText.name())
            .arg(primary ? ct.modeBtnPrimaryBg.name()      : ct.modeBtnNonPrimaryBg.name())
            .arg(primary ? ct.modeBtnPrimaryBorder.name()  : ct.modeBtnNonPrimaryBorder.name())
            .arg(primary ? ct.modeBtnPrimaryHoverBg.name() : ct.modeBtnNonPrimaryHoverBg.name())
            .arg(chk ? QString("QPushButton:checked{background:%1;color:%2;border:1px solid %3;}")
                .arg(ct.modeBtnPrimaryBg.name(), ct.modeBtnCheckedText.name(), ct.modeBtnPrimaryBorder.name()) : ""));
        return b;
    };
    textModeBtn_ = mkBtn(_T("文字识别"), true, true); textModeBtn_->setChecked(true); tbl->addWidget(textModeBtn_);
    jsonModeBtn_ = mkBtn("JSON", true, false); tbl->addWidget(jsonModeBtn_);
    tbl->addStretch();
    auto mkTb = [&](const QString& t, const QString& tip)->QToolButton*{
        auto* b = new QToolButton; b->setText(t); b->setFixedSize(34,34); b->setCursor(Qt::PointingHandCursor); b->setToolTip(tip);
        b->setStyleSheet(QString("QToolButton{font:10pt 'Microsoft YaHei';color:%1;background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:%2;color:%3;}")
            .arg(ct.toolBtnText.name(), ct.toolBtnHoverBg.name(), ct.toolBtnHoverText.name()));
        return b;
    };
    auto* snapshotBtn = new QToolButton; snapshotBtn->setText(_T("截图")); snapshotBtn->setFixedSize(42,34); snapshotBtn->setCursor(Qt::PointingHandCursor); snapshotBtn->setToolTip(_T("截图OCR"));
    snapshotBtn->setStyleSheet(QString("QToolButton{font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:%2;color:%3;}")
        .arg(ct.toolBtnText.name(), ct.toolBtnHoverBg.name(), ct.toolBtnHoverText.name()));
    speakBtn_   = mkTb("▶", _T("语音朗读"));
    listBtn_    = mkTb("≡", _T("结果列表"));
    correctBtn_ = mkTb("✎", _T("纠正文字"));
    copyBtn_    = mkTb("📋", _T("复制结果 (Ctrl+C)"));
    clearBtn_   = mkTb("✕", _T("清空"));
    exportBtn_  = mkTb("↓", _T("导出结果"));
    tbl->addWidget(snapshotBtn); tbl->addWidget(speakBtn_); tbl->addWidget(listBtn_); tbl->addWidget(correctBtn_); tbl->addWidget(copyBtn_); tbl->addWidget(clearBtn_); tbl->addWidget(exportBtn_);
    rl->addWidget(tb);

    // QStackedWidget: page0=文字, page1=JSON, page2=表格
    resultStack_ = new QStackedWidget;
    textPreview_ = new QTextEdit; textPreview_->setReadOnly(true); textPreview_->setAcceptDrops(false);
    textPreview_->setStyleSheet(QString("QTextEdit{font:12pt 'SimHei','Microsoft YaHei';color:%1;background:%2;border:none;padding:12px;selection-background-color:%3;}")
        .arg(ct.textPreviewText.name(), ct.textPreviewBg.name(), rgbaStr(ct.textSelectionBg)));
    resultStack_->addWidget(textPreview_);   // index 0
    jsonPreview_ = new QTextEdit; jsonPreview_->setReadOnly(true); jsonPreview_->setAcceptDrops(false);
    jsonPreview_->setStyleSheet(QString("QTextEdit{font:9pt 'Consolas',monospace;color:%1;background:%2;border:none;padding:10px;selection-background-color:%3;}")
        .arg(ct.jsonText.name(), ct.textPreviewBg.name(), rgbaStr(ct.textSelectionBg)));
    jsonHighlighter_ = new JsonHighlighter(jsonPreview_->document());
    resultStack_->addWidget(jsonPreview_);   // index 1
    tableWidget_ = new QTableWidget;
    tableWidget_->setStyleSheet(QString("QTableWidget{font:10pt 'Microsoft YaHei';color:%1;background:%2;border:1px solid %3;gridline-color:%4;}"
        "QTableWidget::item:selected{background:%5;}QHeaderView::section{background:%6;font:9pt 'Microsoft YaHei';padding:4px;border:1px solid %3;}")
        .arg(ct.tableText.name(), ct.tableBg.name(), ct.tableBorder.name(),
             ct.tableGridline.name(), rgbaStr(ct.tableSelectionBg), ct.tableHeaderBg.name()));
    tableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_->horizontalHeader()->setVisible(false);
    tableWidget_->verticalHeader()->setVisible(false);
    resultStack_->addWidget(tableWidget_);   // index 2
    connect(tableWidget_, &QTableWidget::cellClicked, this, &CalligraphyOcrPage::onTableCellClicked);
    rl->addWidget(resultStack_);

    mainSplitter_->addWidget(rightPanel_);
    ml->addWidget(mainSplitter_);

    // 信号
    connect(textModeBtn_,&QPushButton::toggled,[this](bool c){if(c){jsonModeBtn_->setChecked(false);onModeToggled(true);}});
    connect(jsonModeBtn_,&QPushButton::toggled,[this](bool c){if(c){textModeBtn_->setChecked(false);onModeToggled(false);}});
    connect(imageView_,&ImagePreviewView::textBoxClicked,this,&CalligraphyOcrPage::onTextBoxClicked);
    connect(imageView_,&ImagePreviewView::textBoxHovered,this,&CalligraphyOcrPage::onTextBoxHovered);
    connect(snapshotBtn,&QToolButton::clicked,this,&CalligraphyOcrPage::snapshotOcr);
    connect(speakBtn_,&QToolButton::clicked,this,&CalligraphyOcrPage::speakResult);
    connect(listBtn_,&QToolButton::clicked,this,&CalligraphyOcrPage::showResultList);
    connect(correctBtn_,&QToolButton::clicked,this,&CalligraphyOcrPage::toggleCorrection);
    connect(copyBtn_,&QToolButton::clicked,this,&CalligraphyOcrPage::copyAll);
    connect(prevPageBtn_,&QPushButton::clicked,this,[this](){ goToPage(pdfPageIndex_-1); });
    connect(nextPageBtn_,&QPushButton::clicked,this,[this](){ goToPage(pdfPageIndex_+1); });
    connect(clearBtn_,&QToolButton::clicked,this,&CalligraphyOcrPage::clearAll);
    connect(exportBtn_,&QToolButton::clicked,this,&CalligraphyOcrPage::exportResult);

    // Ctrl+C 快捷键复制结果
    auto* copyShortcut = new QShortcut(QKeySequence("Ctrl+C"), this);
    connect(copyShortcut, &QShortcut::activated, this, &CalligraphyOcrPage::copyAll);
}

// ======================== 语音朗读 ========================
void CalligraphyOcrPage::speakResult() {
    QString txt = textMode_ ? textPreview_->toPlainText() : jsonPreview_->toPlainText();
    if (txt.isEmpty()) { mainWindow_->showStatus(_T("无可朗读文本")); return; }
    if (tts_->state() == QTextToSpeech::Speaking) { tts_->stop(); mainWindow_->showStatus(_T("已停止朗读")); return; }
    tts_->say(txt);
    mainWindow_->showStatus(_T("正在朗读..."));
}

// ======================== 结果列表 ========================
void CalligraphyOcrPage::showResultList() {
    if (items_.empty()) { mainWindow_->showStatus(_T("无识别结果")); return; }
    QDialog dlg(this); dlg.setWindowTitle(_T("OCR 结果列表")); dlg.resize(500,400);
    auto* dl = new QVBoxLayout(&dlg);
    auto* lw = new QListWidget; lw->setAlternatingRowColors(true);
    for (size_t i = 0; i < items_.size(); ++i) {
        lw->addItem(QString("[%1] %2  (%3:%4)").arg(i+1).arg(QString::fromStdString(items_[i].text)).arg(_T("置信度"), QString::number(items_[i].score,'f',2)));
    }
    dl->addWidget(lw);
    auto* cpBtn = new QPushButton(_T("复制选中")); cpBtn->setStyleSheet("QPushButton{padding:4px 12px;}");
    connect(cpBtn,&QPushButton::clicked,[&](){ auto* it=lw->currentItem(); if(it) QApplication::clipboard()->setText(it->text()); });
    dl->addWidget(cpBtn);
    dlg.exec();
}

// ======================== 纠正模式 ========================
void CalligraphyOcrPage::toggleCorrection() {
    correctionActive_ = !correctionActive_;
    if (correctionActive_) {
        textPreview_->setReadOnly(false);
        const auto& th = ThemeManager::instance().currentTheme();
        textPreview_->setStyleSheet(QString("QTextEdit{font:12pt 'SimHei';color:%1;background:%2;border:2px dashed %3;padding:12px;}")
            .arg(th.textPreviewText.name(), th.correctModeBg.name(), th.correctModeBorder.name()));
        mainWindow_->showStatus(_T("纠正模式：直接修改文字，完成后再次点击 ✎ 保存"));
    } else {
        textPreview_->setReadOnly(true);
        const auto& th2 = ThemeManager::instance().currentTheme();
        textPreview_->setStyleSheet(QString("QTextEdit{font:12pt 'SimHei','Microsoft YaHei';color:%1;background:%2;border:none;padding:12px;selection-background-color:%3;}")
            .arg(th2.textPreviewText.name(), th2.textPreviewBg.name(), rgbaStr(th2.textSelectionBg)));
        fullText_ = textPreview_->toPlainText();
        mainWindow_->showStatus(_T("纠正已保存"));
    }
}

void CalligraphyOcrPage::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    QTimer::singleShot(0, this, [this]() {
        mainSplitter_->setSizes({140, 480, 760});
        refreshHistory();
    });
}

void CalligraphyOcrPage::refreshHistory() {
    if (!history_ || !historyList_) return;
    historyList_->clear();
    auto items = history_->list();
    for (auto& h : items) {
        auto* item = new QListWidgetItem(
            h.thumbnail.isNull() ? QIcon() : QIcon(h.thumbnail), "");
        item->setData(Qt::UserRole, h.id);
        item->setToolTip(h.fileName + "\n" + h.uploadTime);
        historyList_->addItem(item);
    }
}

void CalligraphyOcrPage::onHistoryClicked(QListWidgetItem* item) {
    if (!item || !history_) return;
    int id = item->data(Qt::UserRole).toInt();
    auto items = history_->list();
    for (auto& h : items) {
        if (h.id != id) continue;

        // 文档：渐进式渲染目标页（不一次性渲染全部页面）
        if (h.isDocument && QFile::exists(h.filePath)) {
            historyFilePath_ = h.filePath;
            int pageCount = PdfRender::getPageCount(h.filePath);
            if (pageCount > 0) {
                pdfPages_.clear();
                for (int i = 0; i < pageCount; ++i) pdfPages_ << QString();
                int targetPage = qBound(0, h.pageIndex, pageCount - 1);
                pdfPageIndex_ = targetPage;
                ensurePageRendered(targetPage);
                currentPath_ = pdfPages_[targetPage];
                currentPixmap_ = QPixmap(currentPath_);
                imageView_->setImage(currentPixmap_);
                fileNameLabel_->setText(h.fileName);
                fileSizeLabel_->setText("");
                if (pageLabel_) pageLabel_->setText(
                    QString("%1/%2").arg(targetPage + 1).arg(pageCount));
                prevPageBtn_->setVisible(pageCount > 1);
                nextPageBtn_->setVisible(pageCount > 1);
                pageLabel_->setVisible(pageCount > 1);
                prevPageBtn_->setEnabled(targetPage > 0);
                nextPageBtn_->setEnabled(targetPage < pageCount - 1);
            }
        } else {
            pdfPages_.clear(); pdfPageIndex_ = 0;
            prevPageBtn_->setVisible(false); nextPageBtn_->setVisible(false);
            pageLabel_->setVisible(false);
            currentPath_ = h.filePath;
            if (!h.pageImage.isNull()) {
                currentPixmap_ = QPixmap::fromImage(h.pageImage);
                imageView_->setImage(currentPixmap_);
            }
            fileNameLabel_->setText(h.fileName);
            fileSizeLabel_->setText("");
        }
        // 恢复多页文档的全部页面缓存数据
        pdfPagesData_.clear();
        if (!h.pagesJson.isEmpty()) {
            QJsonDocument pd = QJsonDocument::fromJson(h.pagesJson.toUtf8());
            QJsonArray pagesArr = pd.object()["pages"].toArray();
            for (const auto& pv : pagesArr) {
                QJsonObject po = pv.toObject();
                pdfPagesData_.append({po["text"].toString(), po["json"].toString()});
            }
        }
        // 恢复结构化数据（boxes/lines/scores）以支持点击高亮
        items_.clear();
        imageView_->clearTextBoxes();
        bool hasJson = !h.resultJson.isEmpty();
        if (hasJson) {
            QJsonDocument doc = QJsonDocument::fromJson(h.resultJson.toUtf8());
            QJsonObject root = doc.object();
            QString layout = root["layout"].toString();

            if (layout == "qrcode") {
                // 二维码结果 — 从 qrcodes 数组恢复
                QJsonArray qrArr = root["qrcodes"].toArray();
                std::vector<std::array<int, 4>> qrBoxes;
                fullText_ = "";
                for (int i = 0; i < qrArr.size(); ++i) {
                    QJsonObject qo = qrArr[i].toObject();
                    if (i > 0) fullText_ += "\n";
                    fullText_ += QString("[%1] %2")
                        .arg(qo["type"].toString(), qo["text"].toString());
                    QJsonArray qb = qo["box"].toArray();
                    if (qb.size() >= 4) {
                        std::array<int, 4> b = {qb[0].toInt(), qb[1].toInt(),
                                                qb[2].toInt(), qb[3].toInt()};
                        if (b[2] > 0 && b[3] > 0) qrBoxes.push_back(b);
                    }
                }
                if (!qrBoxes.empty()) imageView_->drawQrBoxes(qrBoxes);
                textPreview_->setPlainText(fullText_);
                jsonPreview_->setPlainText(h.resultJson);
                jsonHighlighter_->rehighlight();
                jsonStr_ = h.resultJson;
                isTable_ = false;
                resultStack_->setCurrentIndex(0);
                textModeBtn_->setChecked(true);
                jsonModeBtn_->setChecked(false);
                tableWidget_->clear();
                tableWidget_->setRowCount(0); tableWidget_->setColumnCount(0);
                return;
            }

            QJsonArray arr = root["ocrResults"].toArray();
            std::vector<std::array<int, 4>> bxs;
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject o = arr[i].toObject();
                std::string text = o["text"].toString().toStdString();
                float score = (float)o["score"].toDouble();
                QJsonArray b = o["box"].toArray();
                std::array<int, 4> box = {b[0].toInt(), b[1].toInt(), b[2].toInt(), b[3].toInt()};
                TextItem item{text, box, score};
                item.tableRow = o["tableRow"].toInt(-1);
                item.tableCol = o["tableCol"].toInt(-1);
                items_.push_back(item);
                bxs.push_back(box);
            }
            imageView_->drawTextBoxes(bxs);

            // 用 items_ 重建文本（保证行数与 boxes 一致）
            bool isVert = (layout == "vertical");
            fullText_ = "";
            for (size_t i = 0; i < items_.size(); ++i) {
                if (i > 0) fullText_ += "\n";
                fullText_ += QString::fromStdString(items_[i].text);
            }
            // 按原始排版渲染
            if (layout == "table") {
                isTable_ = true;
                renderTable();
                fullText_ = h.ocrResult;
                textPreview_->setPlainText(h.ocrResult);
            } else {
                isTable_ = false;
                resultStack_->setCurrentIndex(0);
                updateOcrTextLayout(isVert);
            }
            jsonPreview_->setPlainText(h.resultJson);
            jsonHighlighter_->rehighlight();
            textModeBtn_->setChecked(true);
            jsonModeBtn_->setChecked(false);
        } else {
            resultStack_->setCurrentIndex(0);
            textPreview_->setPlainText(h.ocrResult);
            fullText_ = h.ocrResult;
            jsonPreview_->clear();
        }
        textModeBtn_->setChecked(true);
        jsonModeBtn_->setChecked(false);
        break;
    }
}

// ======================== 拖放 ========================
void CalligraphyOcrPage::dragEnterEvent(QDragEnterEvent* e) { if(e->mimeData()->hasUrls())e->acceptProposedAction(); }
void CalligraphyOcrPage::dragMoveEvent(QDragMoveEvent* e) { if(e->mimeData()->hasUrls())e->acceptProposedAction(); }
void CalligraphyOcrPage::dropEvent(QDropEvent* e) { for(auto& u:e->mimeData()->urls())if(u.isLocalFile()){onImageDropped(u.toLocalFile());return;} }

void CalligraphyOcrPage::onImageDropped(const QString& path) {
    abortOcrAll_ = true;  // 中断上一个文档的后台 OCR
    running_ = false;     // 强制清除上轮 OCR 的 running 标志（回调可能尚未到达主线程）
    QString ext = QFileInfo(path).suffix().toLower();
    if (PdfRender::isDocument(ext)) {
        // PDF/DOCX 等文档：渐进式渲染（首帧立即显示，后续按需加载）
        int pageCount = PdfRender::getPageCount(path);
        if (pageCount <= 0) {
            QMessageBox::warning(this, _T("加载失败"), _T("无法打开文档: ") + QFileInfo(path).fileName());
            return;
        }
        historyFilePath_ = path;
        // 初始化稀疏数组：全部为空，按需填充
        pdfPages_.clear();
        pdfPages_.reserve(pageCount);
        for (int i = 0; i < pageCount; ++i) pdfPages_ << QString();

        // 渲染首页，立即显示
        QString tmpDir = QApplication::applicationDirPath() + "/_pdf_tmp";
        pdfPages_[0] = pdfRender_.renderSinglePage(path, 0, tmpDir);
        if (pdfPages_[0].isEmpty()) {
            QMessageBox::warning(this, _T("加载失败"), _T("无法渲染文档首页: ") + QFileInfo(path).fileName());
            return;
        }
        pdfPageIndex_ = 0;
        currentPath_ = pdfPages_[0];
        currentPixmap_ = QPixmap(currentPath_);
        imageView_->setImage(currentPixmap_);
        QFileInfo fi(path);
        fileNameLabel_->setText(fi.fileName() + QString(_T(" (第1/%1页)")).arg(pageCount));
        fileSizeLabel_->setText("");
        if (pageLabel_) pageLabel_->setText(QString("1/%1").arg(pageCount));
        // 显示翻页按钮
        if (prevPageBtn_) prevPageBtn_->setVisible(pageCount > 1);
        if (nextPageBtn_) nextPageBtn_->setVisible(pageCount > 1);
        if (pageLabel_)   pageLabel_->setVisible(pageCount > 1);
        prevPageBtn_->setEnabled(false);
        nextPageBtn_->setEnabled(pageCount > 1);

        // 渐进式 OCR：后台逐页渲染+识别（不阻塞首页浏览）
        textPreview_->clear(); jsonPreview_->clear(); imageView_->clearTextBoxes();
        mainWindow_->showStatus(_T("正在 OCR 全部页面..."));
        overlay_->show(_T("正在识别全部页面，请稍候..."));
        saveHistory_ = true;
        QThread::create([this]() { ocrAllPages(); })->start();
        return;
    }

    // 图片文件直接加载
    pdfPages_.clear(); pdfPagesData_.clear(); pdfPageIndex_ = 0;
    prevPageBtn_->setVisible(false); nextPageBtn_->setVisible(false); pageLabel_->setVisible(false);
    historyFilePath_ = path;
    saveHistory_ = true;  // 确保图片识别结果保存到历史
    currentPath_ = path; QPixmap pm(path);
    if (pm.isNull()) { QMessageBox::warning(this, _T("加载失败"), _T("加载图片失败")); return; }
    currentPixmap_ = pm;
    QFileInfo fi(path); qint64 sz = fi.size(); QString ss;
    if (sz < 1024) ss = QString("%1 B").arg(sz);
    else if (sz < 1048576) ss = QString("%1 KB").arg(sz / 1024., 0, 'f', 2);
    else ss = QString("%1 MB").arg(sz / 1048576., 0, 'f', 2);
    fileNameLabel_->setText(fi.fileName()); fileSizeLabel_->setText(ss);
    imageView_->setImage(pm);
    mainWindow_->showStatus(QString(_T("已加载 %1×%2")).arg(pm.width()).arg(pm.height()));
    startOcr();
}

void CalligraphyOcrPage::onModeToggled(bool tm) {
    if (isTable_) return;  // 表格模式：禁止切换到文字/JSON视图，保持 QTableWidget
    textMode_=tm; resultStack_->setCurrentIndex(tm?0:1);
}

void CalligraphyOcrPage::copyAll() {
    if (isTable_) {
        // 表格模式：复制完整表格（Tab 分隔，行换行）
        int rows = tableWidget_->rowCount();
        int cols = tableWidget_->columnCount();
        if (rows == 0 || cols == 0) return;
        QStringList rowTexts;
        for (int r = 0; r < rows; ++r) {
            QStringList cellTexts;
            for (int c = 0; c < cols; ++c) {
                auto* item = tableWidget_->item(r, c);
                cellTexts << (item ? item->text() : QString());
            }
            rowTexts << cellTexts.join("\t");
        }
        QApplication::clipboard()->setText(rowTexts.join("\n"));
        mainWindow_->showStatus(QString(_T("已复制表格 (%1行×%2列)")).arg(rows).arg(cols));
        return;
    }
    QString t = textMode_ ? textPreview_->toPlainText() : jsonPreview_->toPlainText();
    if (!t.isEmpty()) {
        QApplication::clipboard()->setText(t);
        mainWindow_->showStatus(_T("已复制"));
    }
}

void CalligraphyOcrPage::clearAll() {
    currentPath_.clear(); currentPixmap_=QPixmap(); items_.clear(); fullText_.clear(); jsonStr_.clear();
    pdfPages_.clear(); pdfPagesData_.clear(); pdfPageIndex_=0;
    prevPageBtn_->setVisible(false); nextPageBtn_->setVisible(false); pageLabel_->setVisible(false);
    imageView_->clearScene(); fileNameLabel_->setText(_T("未选择文件")); fileSizeLabel_->setText("");
    textPreview_->clear(); jsonPreview_->clear(); tableWidget_->clear();
    tableWidget_->setRowCount(0); tableWidget_->setColumnCount(0);
    isTable_ = false; resultStack_->setCurrentIndex(0);
    mainWindow_->showStatus(_T("已清空"));
}

void CalligraphyOcrPage::exportResult() {
    if(fullText_.isEmpty()&&jsonStr_.isEmpty()){mainWindow_->showStatus(_T("无结果"));return;}
    QString dir=QFileDialog::getExistingDirectory(this,_T("导出目录")); if(dir.isEmpty())return;
    if(!fullText_.isEmpty()){QFile f(dir+"/ocr_result.txt"); if(f.open(QIODevice::WriteOnly|QIODevice::Text)){f.write(fullText_.toUtf8());}}
    if(!jsonStr_.isEmpty()){QFile f(dir+"/ocr_result.json"); if(f.open(QIODevice::WriteOnly|QIODevice::Text)){f.write(jsonStr_.toUtf8());}}
    // 同时导出标注图片
    if(!currentPixmap_.isNull()){QPixmap annotated=currentPixmap_;QPainter p(&annotated);p.setPen(QPen(ThemeManager::instance().currentTheme().boxExportPen,2));for(auto& it:items_){int w=it.box[2]-it.box[0],h=it.box[3]-it.box[1];if(w>0&&h>0)p.drawRect(it.box[0],it.box[1],w,h);}p.end();annotated.save(dir+"/ocr_annotated.png","PNG");}
    mainWindow_->showStatus(_T("已导出到 ")+dir);
}

void CalligraphyOcrPage::onTextBoxClicked(int idx) {
    if (idx < 0 || idx >= (int)items_.size()) return;
    highlightLine(idx);
}

void CalligraphyOcrPage::onTextBoxHovered(int idx) {
    if (idx < 0 || idx >= (int)items_.size()) return;
    highlightLine(idx);
}

void CalligraphyOcrPage::highlightLine(int idx) {
    if (idx < 0 || idx >= (int)items_.size()) return;
    QString target = QString::fromStdString(items_[idx].text).trimmed();
    if (target.isEmpty()) return;

    // === 表格模式：在 QTableWidget 中定位单元格 ===
    if (isTable_ && items_[idx].tableRow >= 0 && items_[idx].tableCol >= 0) {
        tableWidget_->clearSelection();
        tableWidget_->setCurrentCell(items_[idx].tableRow, items_[idx].tableCol);
        auto* item = tableWidget_->item(items_[idx].tableRow, items_[idx].tableCol);
        if (item) { item->setSelected(true); tableWidget_->scrollToItem(item); }
        resultStack_->setCurrentIndex(2);
        mainWindow_->showStatus(QString(_T("表格[%1,%2]: %3"))
            .arg(items_[idx].tableRow+1).arg(items_[idx].tableCol+1).arg(target.left(30)));
        return;
    }

    // === 文本模式：在 QTextEdit 中搜索定位 ===
    QTextDocument* doc = textPreview_->document();

    // 清除之前的高亮
    QTextCursor clearAll(doc);
    clearAll.select(QTextCursor::Document);
    QTextCharFormat clearFmt; clearFmt.setBackground(Qt::transparent);
    clearAll.mergeCharFormat(clearFmt);

    // 搜索目标文字
    QTextCursor c = doc->find(target);
    if (!c.isNull()) {
        c.movePosition(QTextCursor::StartOfBlock);
        c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QTextCharFormat fmt; fmt.setBackground(ThemeManager::instance().currentTheme().textHighlightBg);
        c.mergeCharFormat(fmt);

        QTextCursor vis = doc->find(target);
        textPreview_->setTextCursor(vis);
        textPreview_->ensureCursorVisible();
    }

    resultStack_->setCurrentIndex(0);
    mainWindow_->showStatus(QString(_T("定位: %1")).arg(target.left(40)));
}

// ======================== 排版引擎 ========================
bool CalligraphyOcrPage::detectIsVerticalWriting(const std::vector<std::array<int,4>>& boxes) {
    if(boxes.empty())return false; int vc=0,hc=0;
    for(auto& b:boxes){int w=b[2]-b[0],h=b[3]-b[1];if(w<=0||h<=0)continue;(h>w*1.8)?vc++:hc++;}
    return vc>hc;
}

QStringList CalligraphyOcrPage::layoutVerticalCalligraphy() {
    if(items_.empty())return{};
    struct C{int x0,y0,x1,y1;QString t;}; std::vector<C> blk;
    for(auto& it:items_){int w=it.box[2]-it.box[0],h=it.box[3]-it.box[1];if(w<=0||h<=0)continue;QString t=QString::fromStdString(it.text).trimmed();if(t.isEmpty())continue;blk.push_back({it.box[0],it.box[1],it.box[2],it.box[3],t});}
    if(blk.empty())return{};
    double avgW=0; for(auto& b:blk)avgW+=b.x1-b.x0; avgW/=blk.size();
    std::sort(blk.begin(),blk.end(),[](auto&a,auto&b){return a.x0>b.x0;});
    std::vector<std::vector<C>> cols; double th=avgW*0.7;
    for(auto& b:blk){bool ok=false;for(auto&c:cols)if(std::abs(b.x0-c[0].x0)<th){c.push_back(b);ok=true;break;}if(!ok)cols.push_back({b});}
    QStringList r; for(auto&c:cols){std::sort(c.begin(),c.end(),[](auto&a,auto&b){return a.y0<b.y0;});QString s;for(auto&b:c){if(!s.isEmpty())s+='\n';s+=b.t;}r.append(s);}
    return r;
}

QString CalligraphyOcrPage::layoutHorizontalDocument() {
    if(items_.empty())return{};
    struct R{int x0,y0,x1,y1;QString t;}; std::vector<R> blk;
    for(auto& it:items_){int w=it.box[2]-it.box[0],h=it.box[3]-it.box[1];if(w<=0||h<=0)continue;QString t=QString::fromStdString(it.text).trimmed();if(t.isEmpty())continue;blk.push_back({it.box[0],it.box[1],it.box[2],it.box[3],t});}
    if(blk.empty())return{};
    double avgH=0; for(auto& b:blk)avgH+=b.y1-b.y0; avgH/=blk.size();
    std::sort(blk.begin(),blk.end(),[](auto&a,auto&b){return std::abs(a.y0-b.y0)<5?a.x0<b.x0:a.y0<b.y0;});
    std::vector<std::vector<R>> rows; double th=avgH*0.5;
    for(auto& b:blk){bool ok=false;for(auto&r:rows)if(std::abs(b.y0-r[0].y0)<th){r.push_back(b);ok=true;break;}if(!ok)rows.push_back({b});}
    QStringList lines; for(auto&r:rows){std::sort(r.begin(),r.end(),[](auto&a,auto&b){return a.x0<b.x0;});QString l;for(auto&b:r)l+=b.t;lines.append(l);}
    return lines.join("\n");
}

bool CalligraphyOcrPage::isTableLayout() {
    if(items_.size()<6)return false;
    std::vector<int> xc,yc;
    for(auto& it:items_){int w=it.box[2]-it.box[0],h=it.box[3]-it.box[1];if(w<=0||h<=0)continue;xc.push_back((it.box[0]+it.box[2])/2);yc.push_back((it.box[1]+it.box[3])/2);}
    if(xc.size()<6)return false;
    std::sort(xc.begin(),xc.end()); std::sort(yc.begin(),yc.end());
    auto gaps=[](auto& v){std::vector<int> g;for(size_t i=1;i<v.size();++i){int d=v[i]-v[i-1];if(d>2)g.push_back(d);}return g;};
    auto gx=gaps(xc),gy=gaps(yc); if(gx.size()<3||gy.size()<3)return false;
    auto cv=[](auto& v){if(v.empty())return 999.;double s=0,s2=0;for(int g:v){s+=g;s2+=g*g;}double m=s/v.size(),var=s2/v.size()-m*m;return m>0?std::sqrt(std::max(0.,var))/m:999.;};
    return cv(gx)<0.5&&cv(gy)<0.5;
}

QString CalligraphyOcrPage::layoutTableDocument() {
    if(items_.empty())return{};
    struct C{int x0,y0,x1,y1;QString t;}; std::vector<C> cs;
    for(auto& it:items_){int w=it.box[2]-it.box[0],h=it.box[3]-it.box[1];if(w<=0||h<=0)continue;cs.push_back({it.box[0],it.box[1],it.box[2],it.box[3],QString::fromStdString(it.text).trimmed()});}
    std::sort(cs.begin(),cs.end(),[](auto&a,auto&b){return std::abs(a.y0-b.y0)<5?a.x0<b.x0:a.y0<b.y0;});
    double avgH=0; for(auto&c:cs)avgH+=c.y1-c.y0; avgH/=cs.size();
    QStringList rows; QString cur; int lastY=-1;
    for(auto&c:cs){if(lastY>=0&&std::abs(c.y0-lastY)>avgH*0.6){if(!cur.isEmpty())rows.append(cur);cur.clear();lastY=c.y0;}else if(lastY<0)lastY=c.y0;if(!cur.isEmpty())cur+='\t';cur+=c.t;}
    if(!cur.isEmpty())rows.append(cur);
    return rows.join("\n");
}

void CalligraphyOcrPage::updateOcrTextLayout(bool isVertical) {
    isVerticalLayout_=isVertical;
    if(isVertical){
        QStringList cols=layoutVerticalCalligraphy(); if(cols.isEmpty()){textPreview_->clear();return;}
        const auto& tv = ThemeManager::instance().currentTheme();
        QString html=QString("<html><body style='background:%1;display:flex;flex-direction:row-reverse;gap:14px;padding:16px;'>")
            .arg(tv.vertLayoutBg.name());
        for(auto&c:cols){QString e=c.toHtmlEscaped();e.replace('\n',"<br>");html+=QString("<div style='writing-mode:vertical-rl;letter-spacing:6px;line-height:40px;font:18pt SimHei;color:%1;background:%2;border:1px solid %3;border-radius:4px;padding:8px 4px;'>%4</div>")
            .arg(tv.vertLayoutText.name(), tv.rightPanelBg.name(), tv.vertLayoutBorder.name(), e);}
        html+="</body></html>"; textPreview_->setHtml(html);
    }else{
        QString txt=isTableLayout()?layoutTableDocument():layoutHorizontalDocument();
        if(txt.isEmpty())txt=fullText_; textPreview_->setPlainText(txt);
    }
}

// ======================== 表格网格聚类 ========================
void CalligraphyOcrPage::buildTableGrid() {
    if (items_.size() < 4) { isTable_ = false; return; }

    // 收集所有框的中心 X 和 Y
    std::vector<int> cx, cy;
    for (auto& it : items_) {
        if (it.box[2] <= it.box[0] || it.box[3] <= it.box[1]) continue;
        cx.push_back((it.box[0] + it.box[2]) / 2);
        cy.push_back((it.box[1] + it.box[3]) / 2);
    }
    if (cx.size() < 4) { isTable_ = false; return; }

    // 对中心点排序后找间隙，拆分行列
    auto cluster = [](std::vector<int> vals, int minGap) {
        std::sort(vals.begin(), vals.end());
        std::vector<int> boundaries;
        boundaries.push_back(vals[0]);
        for (size_t i = 1; i < vals.size(); ++i)
            if (vals[i] - vals[i - 1] > minGap) boundaries.push_back(vals[i]);
        return boundaries;
    };

    int avgW = 0, avgH = 0;
    for (auto& it : items_) { avgW += it.box[2]-it.box[0]; avgH += it.box[3]-it.box[1]; }
    avgW /= (int)items_.size(); avgH /= (int)items_.size();
    int minGap = std::min(avgW, avgH) / 3; // 间隙阈值 = 平均尺寸的 1/3
    if (minGap < 8) minGap = 8;

    auto colBounds = cluster(cx, minGap);
    auto rowBounds = cluster(cy, minGap);

    int nCols = (int)colBounds.size();
    int nRows = (int)rowBounds.size();

    // 至少 2×2 才算表格
    if (nRows < 2 || nCols < 2) { isTable_ = false; return; }

    // 给每个 item 分配最近的行列
    for (auto& it : items_) {
        int icx = (it.box[0] + it.box[2]) / 2;
        int icy = (it.box[1] + it.box[3]) / 2;
        int bestCol = 0, bestRow = 0;
        int bestDist = INT_MAX;
        for (int r = 0; r < nRows; ++r) {
            for (int c = 0; c < nCols; ++c) {
                int dx = icx - colBounds[c];
                int dy = icy - rowBounds[r];
                int d = dx*dx + dy*dy;
                if (d < bestDist) { bestDist = d; bestRow = r; bestCol = c; }
            }
        }
        it.tableRow = bestRow; it.tableCol = bestCol;
    }

    isTable_ = true;
}

void CalligraphyOcrPage::renderTable() {
    if (!isTable_ || items_.empty()) return;

    // 统计行列数
    int maxRow = 0, maxCol = 0;
    for (auto& it : items_) {
        if (it.tableRow > maxRow) maxRow = it.tableRow;
        if (it.tableCol > maxCol) maxCol = it.tableCol;
    }

    tableWidget_->clear();
    tableWidget_->setRowCount(maxRow + 1);
    tableWidget_->setColumnCount(maxCol + 1);

    // 填充单元格
    for (auto& it : items_) {
        if (it.tableRow < 0 || it.tableCol < 0) continue;
        QString txt = QString::fromStdString(it.text).trimmed();
        auto* item = new QTableWidgetItem(txt);
        item->setTextAlignment(Qt::AlignCenter);
        // 用 UserRole 存 items_ 索引，点击时反向定位
        int itemIdx = (int)(&it - &items_[0]);
        item->setData(Qt::UserRole, itemIdx);
        tableWidget_->setItem(it.tableRow, it.tableCol, item);
    }

    tableWidget_->resizeColumnsToContents();
    tableWidget_->resizeRowsToContents();

    resultStack_->setCurrentIndex(2); // 切换到表格页
}

void CalligraphyOcrPage::onTableCellClicked(int row, int col) {
    auto* item = tableWidget_->item(row, col);
    if (!item) return;
    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= (int)items_.size()) return;
    // 左侧定位
    imageView_->highlightTextBox(idx);
    mainWindow_->showStatus(QString(_T("表格[%1,%2]: %3")).arg(row+1).arg(col+1)
        .arg(QString::fromStdString(items_[idx].text).left(30)));
}

// ======================== 渐进式 PDF 渲染 ========================
void CalligraphyOcrPage::ensurePageRendered(int index) {
    if (index < 0 || index >= pdfPages_.size()) return;
    if (!pdfPages_[index].isEmpty()) return;  // 已渲染
    if (historyFilePath_.isEmpty()) return;

    QString tmpDir = QApplication::applicationDirPath() + "/_pdf_tmp";
    pdfPages_[index] = pdfRender_.renderSinglePage(historyFilePath_, index, tmpDir);
}

// ======================== 截图 OCR ========================
void CalligraphyOcrPage::goToPage(int index) {
    if (index < 0 || index >= pdfPages_.size()) return;
    pdfPageIndex_ = index;
    // 渐进式：按需渲染当前页
    ensurePageRendered(index);
    currentPath_ = pdfPages_[index];
    QPixmap pm(currentPath_);
    if (pm.isNull()) return;
    currentPixmap_ = pm;
    imageView_->setImage(pm);
    QFileInfo fi(historyFilePath_.isEmpty() ? currentPath_ : historyFilePath_);
    int total = pdfPages_.size();
    fileNameLabel_->setText(fi.fileName() + QString(_T(" (第%1/%2页)")).arg(index + 1).arg(total));
    if (pageLabel_) pageLabel_->setText(QString("%1/%2").arg(index + 1).arg(total));
    prevPageBtn_->setEnabled(index > 0);
    nextPageBtn_->setEnabled(index < total - 1);
    // 后台预渲染下一页（让翻页更流畅）
    if (index + 1 < total && pdfPages_[index + 1].isEmpty()) {
        int next = index + 1;
        QThread::create([this, next]() { ensurePageRendered(next); })->start();
    }
    // 从缓存读取（ocrAllPages 已预处理全部页面）
    if (index < pdfPagesData_.size() && !pdfPagesData_[index].json.isEmpty()) {
        auto& d = pdfPagesData_[index];
        fullText_ = d.text;
        jsonStr_ = d.json;
        textPreview_->setPlainText(d.text);
        jsonPreview_->setPlainText(d.json);
        jsonHighlighter_->rehighlight();
        QJsonDocument doc = QJsonDocument::fromJson(d.json.toUtf8());
        QJsonObject root = doc.object();
        QJsonArray arr = root["ocrResults"].toArray();
        // 二维码结果也画框
        QJsonArray qrArr = root["qrcodes"].toArray();
        std::vector<std::array<int,4>> qrBoxes;
        for (int j = 0; j < qrArr.size(); ++j) {
            QJsonArray qb = qrArr[j].toObject()["box"].toArray();
            if (qb.size() >= 4) {
                std::array<int,4> b = {qb[0].toInt(), qb[1].toInt(), qb[2].toInt(), qb[3].toInt()};
                if (b[2] > 0 && b[3] > 0) qrBoxes.push_back(b);
            }
        }
        items_.clear();
        std::vector<std::array<int,4>> bxs;
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject o = arr[i].toObject();
            QJsonArray b = o["box"].toArray();
            std::array<int,4> box = {b[0].toInt(),b[1].toInt(),b[2].toInt(),b[3].toInt()};
            items_.push_back({o["text"].toString().toStdString(), box, (float)o["score"].toDouble()});
            bxs.push_back(box);
        }
        imageView_->drawTextBoxes(bxs);
        if (!qrBoxes.empty()) imageView_->drawQrBoxes(qrBoxes);
        resultStack_->setCurrentIndex(0);
        textModeBtn_->setChecked(true);
        jsonModeBtn_->setChecked(false);
    } else {
        // 后台 OCR 尚未完成此页 — 清空面板等 OCR 线程更新
        textPreview_->clear();
        jsonPreview_->clear();
        imageView_->clearTextBoxes();
    }
}

void CalligraphyOcrPage::ocrAllPages() {
    abortOcrAll_ = false;
    int total = pdfPages_.size();
    pdfPagesData_.resize(total);

    // ---- 遍历全部页面，仅存储到缓存，每页更新状态栏 ----
    for (int i = 0; i < total; ++i) {
        if (abortOcrAll_) {
            QMetaObject::invokeMethod(this, [this]() {
                overlay_->hide();
                mainWindow_->showStatus(_T("已取消"));
            });
            return;
        }
        auto* engine = mainWindow_->engine();
        if (!engine || !engine->isReady()) {
            QMetaObject::invokeMethod(this, [this]() {
                overlay_->showError(_T("引擎未就绪"));
            });
            return;
        }

        // 更新状态栏进度
        QMetaObject::invokeMethod(this, [this, i, total]() {
            mainWindow_->showStatus(QString(_T("正在 OCR 第 %1/%2 页...")).arg(i + 1).arg(total));
        });

        // 仿 Umi-OCR 混合模式：先检查是否有内嵌文字
        auto pageData = pdfRender_.extractPageText(historyFilePath_, i);
        bool hasExtractedText = pageData.hasEmbeddedText;

        // 渐进式：按需渲染此页
        ensurePageRendered(i);
        if (pdfPages_[i].isEmpty()) {
            pdfPagesData_[i] = {QString(_T("[渲染失败] 第%1页")).arg(i + 1), "{}"};
            continue;
        }

        // 快速预检：二维码图片跳过 OCR
        std::string qrQuick = QrScanner::quickDetect(pdfPages_[i]);
        if (!qrQuick.empty()) {
            QJsonObject qo; qo["type"] = "QRCode"; qo["text"] = QString::fromStdString(qrQuick);
            qo["box"] = QJsonArray{0, 0, 0, 0};
            QJsonArray qrArr; qrArr.append(qo);
            QJsonObject root;
            root["qrcodes"] = qrArr;
            root["ocrResults"] = QJsonArray();
            root["layout"] = "qrcode";
            QString js = QJsonDocument(root).toJson(QJsonDocument::Indented);
            pdfPagesData_[i] = {QString("[QRCode] %1").arg(QString::fromStdString(qrQuick)), js};
            continue;
        }

        auto r = engine->predict(pdfPages_[i].toStdString());
        // 构建 TurboResult 用于 parser
        TurboResult tr; tr.lines = r.lines; tr.scores = r.scores; tr.boxes = r.boxes;
        std::vector<std::array<int,4>> bxs;
        for (auto& b : r.boxes) bxs.push_back(b);

        // 逐框判断竖排 + 表格
        bool isVert = detectIsVerticalWriting(bxs);
        bool tableDetected = isTableLayout();
        if (!isTable_) tableDetected = false;

        // 应用合适的解析器
        QString txt;
        if (tableDetected) {
            txt = QString::fromStdString(applyParser(tr, ParserType::None));
        } else {
            txt = QString::fromStdString(applyParserMixed(tr));
        }

        QJsonArray ja;
        for (size_t j = 0; j < r.lines.size(); ++j) {
            QJsonObject o;
            o["text"] = QString::fromStdString(r.lines[j]);
            o["score"] = j < r.scores.size() ? (double)r.scores[j] : 0.0;
            if (j < r.boxes.size()) {
                QJsonArray b;
                for (int v : r.boxes[j]) b.append(v);
                o["box"] = b;
            }
            ja.append(o);
        }
        QJsonObject root;
        root["ocrResults"] = ja;
        root["total_ms"] = r.total_ms;
        root["layout"] = tableDetected ? "table" : (isVert ? "vertical" : "mixed");
        // 仿 Umi-OCR：有 PDF 内嵌文字时，保留 OCR 框（hover 用），文字用提取的
        if (hasExtractedText && !pageData.extractedText.trimmed().isEmpty()) {
            root["extracted_text"] = true;
            txt = pageData.extractedText.trimmed();
        }
        QString js = QJsonDocument(root).toJson(QJsonDocument::Indented);
        pdfPagesData_[i] = {txt, js};
    }

    // ---- 全部页面完成 → 一次性显示首页结果 + 保存历史 ----
    if (abortOcrAll_ || total == 0 || !pdfPagesData_.size()) {
        QMetaObject::invokeMethod(this, [this]() {
            overlay_->hide();
            mainWindow_->showStatus(_T("无识别结果"));
        });
        return;
    }

    // 首页数据
    QString saveText   = pdfPagesData_[0].text;
    QString saveJson   = pdfPagesData_[0].json;
    QString saveImgPath = pdfPages_[0];

    // 首页 TurboResult（用于画框 + 竖排/表格检测）
    QJsonDocument doc = QJsonDocument::fromJson(saveJson.toUtf8());
    QJsonArray arr = doc.object()["ocrResults"].toArray();
    std::vector<std::string> lines;
    std::vector<std::array<int, 4>> bxs;
    std::vector<float> scores;
    for (int j = 0; j < arr.size(); ++j) {
        QJsonObject o = arr[j].toObject();
        lines.push_back(o["text"].toString().toStdString());
        scores.push_back((float)o["score"].toDouble());
        QJsonArray b = o["box"].toArray();
        if (b.size() >= 4)
            bxs.push_back({b[0].toInt(), b[1].toInt(), b[2].toInt(), b[3].toInt()});
        else
            bxs.push_back({0, 0, 0, 0});
    }

    // 全部页面 JSON
    QJsonArray pagesArr;
    for (int i = 0; i < pdfPagesData_.size(); ++i) {
        QJsonObject p;
        p["page"] = i;
        p["text"] = pdfPagesData_[i].text;
        p["json"] = pdfPagesData_[i].json;
        pagesArr.append(p);
    }
    QJsonObject proot;
    proot["pages"] = pagesArr;
    QString allPagesJson = QJsonDocument(proot).toJson(QJsonDocument::Compact);

    // 主线程：显示首页 + 存历史
    QMetaObject::invokeMethod(this, [this, saveText, saveJson, saveImgPath,
                                     lines, bxs, scores, allPagesJson]() {
        if (abortOcrAll_) return;
        // 填充 items_
        items_.clear();
        for (size_t j = 0; j < lines.size(); ++j) {
            auto box = j < bxs.size() ? bxs[j] : std::array<int, 4>{};
            float sc = j < scores.size() ? scores[j] : 0.f;
            items_.push_back({lines[j], box, sc});
        }
        // 画框
        imageView_->drawTextBoxes(bxs);
        // 竖排/表格检测
        bool isVert = detectIsVerticalWriting(bxs);
        bool tableDetected = isTableLayout();
        if (tableDetected) buildTableGrid();
        if (!isTable_) tableDetected = false;
        if (tableDetected) {
            buildTableGrid();
            renderTable();
        } else {
            fullText_ = saveText;
            jsonStr_  = saveJson;
            textModeBtn_->setChecked(true); jsonModeBtn_->setChecked(false);
            if (textMode_) updateOcrTextLayout(isVert);
        }
        textPreview_->setPlainText(fullText_);
        jsonPreview_->setPlainText(saveJson);
        jsonHighlighter_->rehighlight();
        overlay_->hide();
        bool hasH = false, hasV2 = false;
        for (auto& b : bxs) {
            int w = b[2]-b[0], h = b[3]-b[1];
            if (w <= 0 || h <= 0) continue;
            (h > w * 1.8) ? (hasV2 = true) : (hasH = true);
        }
        QString layoutLabel = tableDetected ? _T("表格") :
            (hasH && hasV2) ? _T("混合排版") :
            isVert ? _T("竖排") : _T("横排");
        mainWindow_->showStatus(QString(_T("OCR 完成 — %1 · %2项 · %3页"))
            .arg(layoutLabel).arg(items_.size()).arg(pdfPages_.size()));

        // 保存历史
        QString savePath = historyFilePath_.isEmpty() ? saveImgPath : historyFilePath_;
        if (history_ && !savePath.isEmpty()) {
            QImage saveImg = QPixmap(saveImgPath).toImage();
            history_->add(savePath, saveText, saveJson, saveImg,
                          0, pdfPages_.size(), allPagesJson);
            refreshHistory();
        }
    });
}

void CalligraphyOcrPage::snapshotOcr() {
    mainWindow_->hide();
    QApplication::processEvents();
    QThread::msleep(200);  // 等窗口完全隐藏

    ScreenCapture cap;
    QPixmap shot = cap.capture();

    mainWindow_->show();

    if (shot.isNull()) return;

    // 保存到临时文件
    QString tmpPath = QApplication::applicationDirPath() + "/_snapshot_tmp.png";
    shot.save(tmpPath, "PNG");
    onImageDropped(tmpPath);
    startOcr();
}

// ======================== OCR ========================
void CalligraphyOcrPage::startOcr() {
    if(currentPath_.isEmpty()||running_)return; running_=true;
    textPreview_->clear(); jsonPreview_->clear(); imageView_->clearTextBoxes();

    // 先用 OpenCV 快速检测是否是纯二维码图片
    std::string qrQuick = QrScanner::quickDetect(currentPath_);
    if (!qrQuick.empty()) {
        // 是二维码 — 跳过 OCR，直接显示结果
        running_ = false;
        isTable_ = false;
        resultStack_->setCurrentIndex(0);
        textModeBtn_->setChecked(true); jsonModeBtn_->setChecked(false);
        QJsonArray ja;
        QJsonObject qo;
        qo["type"]  = "QRCode";
        qo["text"]  = QString::fromStdString(qrQuick);
        qo["box"]   = QJsonArray{0, 0, 0, 0};  // OpenCV quickDetect 不返位置
        ja.append(qo);
        QJsonObject root;
        root["qrcodes"] = ja;
        root["ocrResults"] = QJsonArray();
        root["layout"] = "qrcode";
        QString js = QJsonDocument(root).toJson(QJsonDocument::Indented);
        fullText_ = QString::fromStdString(qrQuick);
        jsonStr_ = js;
        textPreview_->setPlainText(fullText_);
        jsonPreview_->setPlainText(jsonStr_);
        jsonHighlighter_->rehighlight();
        mainWindow_->showStatus(QString(_T("二维码识别完成 — %1")).arg(QString::fromStdString(qrQuick)));

        // 保存到历史记录
        QString savePath = historyFilePath_.isEmpty() ? currentPath_ : historyFilePath_;
        QString saveJson = js;
        QString saveText = fullText_;
        QImage  savePageImg = currentPixmap_.toImage();
        if (history_ && !savePath.isEmpty()) {
            history_->add(savePath, saveText, saveJson, savePageImg, 0, 1, "");
            refreshHistory();
        }

        // 拿 ZXing 解析完整位置信息（异步补画框）
        QThread::create([this]() {
            auto qrResults = QrScanner::scanFile(currentPath_);
            if (!qrResults.empty() && qrResults[0].box[2] > 0) {
                std::vector<std::array<int,4>> qrBoxes = {qrResults[0].box};
                QMetaObject::invokeMethod(this, [this, qrBoxes]() {
                    if (!qrBoxes.empty()) imageView_->drawQrBoxes(qrBoxes);
                });
            }
        })->start();
        return;
    }

    mainWindow_->showStatus(_T("OCR 识别中..."));
    overlay_->show(_T("OCR 识别中，请稍候..."));
    QThread::create([this](){
        QMutexLocker l(&ocrMutex_);
        auto* e=mainWindow_->engine(); if(!e||!e->isReady()){QMetaObject::invokeMethod(this,[this](){overlay_->showError(_T("引擎未就绪"));running_=false;});return;}
        auto r=e->predict(currentPath_.toStdString());
        items_.clear(); std::vector<std::array<int,4>> bxs;
        for(size_t i=0;i<r.lines.size();++i){items_.push_back({r.lines[i],i<r.boxes.size()?r.boxes[i]:std::array<int,4>{},i<r.scores.size()?r.scores[i]:0.f});if(i<r.boxes.size())bxs.push_back(r.boxes[i]);}
        TurboResult tr; tr.lines=r.lines;tr.scores=r.scores;tr.boxes=r.boxes;

        // 先检测表格
        bool isVert = detectIsVerticalWriting(bxs);       // UI 显示用
        bool tableDetected = isTableLayout();
        if (tableDetected) buildTableGrid();
        if (!isTable_) tableDetected = false;

        // 解析文本：混合排版（逐框判断横竖排），表格保持原始顺序
        QString ft;
        if (tableDetected) {
            ft = QString::fromStdString(applyParser(tr, ParserType::None));
        } else {
            ft = QString::fromStdString(applyParserMixed(tr));
        }
        QJsonArray ja; for(size_t i=0;i<r.lines.size();++i){
            QJsonObject o; o["text"]=QString::fromStdString(r.lines[i]);
            o["score"]=i<r.scores.size()?r.scores[i]:0.;
            if(i<r.boxes.size()){QJsonArray b;for(int v:r.boxes[i])b.append(v);o["box"]=b;}
            if(tableDetected){o["tableRow"]=items_[i].tableRow;o["tableCol"]=items_[i].tableCol;}
            ja.append(o);
        }
        // 二维码/条形码扫描
        QString qrText;
        QJsonArray qrArr;
        std::vector<std::array<int,4>> qrBoxes;
        auto qrResults = QrScanner::scanFile(currentPath_);
        for (const auto& qr : qrResults) {
            if (!qr.error.empty()) continue;
            QJsonObject qo;
            qo["type"]  = QString::fromStdString(qr.format);
            qo["text"]  = QString::fromStdString(qr.text);
            QJsonArray qb;
            qb.append(qr.box[0]); qb.append(qr.box[1]);
            qb.append(qr.box[2]); qb.append(qr.box[3]);
            qo["box"] = qb;
            qrArr.append(qo);
            if (qr.box[2] > 0 && qr.box[3] > 0)
                qrBoxes.push_back(qr.box);
            if (!qrText.isEmpty()) qrText += "\n";
            qrText += QString("[%1] %2").arg(QString::fromStdString(qr.format),
                                               QString::fromStdString(qr.text));
        }

        QJsonObject root; root["ocrResults"]=ja; root["total_ms"]=r.total_ms;
        root["layout"]=tableDetected?"table":(isVert?"vertical":"horizontal");
        if (!qrArr.isEmpty()) root["qrcodes"] = qrArr;
        QString js=QJsonDocument(root).toJson(QJsonDocument::Indented);
        fullText_=ft;
        if (!qrText.isEmpty()) fullText_ += "\n\n" + _T("--- 二维码/条形码 ---") + "\n" + qrText;
        jsonStr_=js;

        // 保存到历史记录（含完整页面图 + 原始文件路径）
        // QPixmap 必须在主线程操作，只传路径进 lambda
        QString savePath    = historyFilePath_.isEmpty() ? currentPath_ : historyFilePath_;
        QString saveText    = fullText_;
        QString saveJson    = jsonStr_;
        QString saveImgPath = currentPath_;          // 只在主线程里加载
        int     savePageIdx = pdfPageIndex_;
        int     saveTotal   = pdfPages_.isEmpty() ? 1 : pdfPages_.size();
        bool doSave = saveHistory_;
        saveHistory_ = true;
        // 累积当前页数据
        if (pdfPageIndex_ >= 0 && pdfPageIndex_ < pdfPagesData_.size()) {
            pdfPagesData_[pdfPageIndex_] = {saveText, saveJson};
        }
        // 构建全部页面 JSON
        QString allPagesJson;
        if (!pdfPages_.isEmpty()) {
            QJsonArray pagesArr;
            for (int i = 0; i < pdfPagesData_.size(); ++i) {
                QJsonObject p;
                p["page"] = i;
                p["text"] = pdfPagesData_[i].text;
                p["json"] = pdfPagesData_[i].json;
                pagesArr.append(p);
            }
            QJsonObject root;
            root["pages"] = pagesArr;
            allPagesJson = QJsonDocument(root).toJson(QJsonDocument::Compact);
        }
        QMetaObject::invokeMethod(this, [this, doSave, savePath, saveText, saveJson, saveImgPath,
                                         savePageIdx, saveTotal, allPagesJson]() {
            if (doSave && history_ && !savePath.isEmpty()) {
                QImage saveImg = QPixmap(saveImgPath).toImage();  // 主线程安全加载
                history_->add(savePath, saveText, saveJson, saveImg,
                              savePageIdx, saveTotal, allPagesJson);
                refreshHistory();
            }
        });

        QMetaObject::invokeMethod(this,[this,isVert,bxs,qrBoxes,r,tableDetected](){
            if (!running_) return;  // 已被新文件替换，放弃回调
            overlay_->hide();
            jsonPreview_->setPlainText(jsonStr_); jsonHighlighter_->rehighlight();
            imageView_->drawTextBoxes(bxs);
            if (!qrBoxes.empty()) imageView_->drawQrBoxes(qrBoxes);
            if (tableDetected) {
                renderTable(); // 表格 → QTableWidget（不触发按钮切换）
            } else {
                if (textMode_) updateOcrTextLayout(isVert);
                textModeBtn_->setChecked(true); jsonModeBtn_->setChecked(false);
            }
            // 检测是否混合排版（横竖排框都有）
            bool hasH = false, hasV = false;
            for (auto& b : bxs) {
                int w = b[2]-b[0], h = b[3]-b[1];
                if (w <= 0 || h <= 0) continue;
                (h > w * 1.8) ? (hasV = true) : (hasH = true);
            }
            QString layoutLabel = tableDetected ? _T("表格") :
                (hasH && hasV) ? _T("混合排版") :
                isVert ? _T("竖排") : _T("横排");
            mainWindow_->showStatus(QString(_T("OCR 完成 — %1 · %2项 · %3ms"))
                .arg(layoutLabel).arg(items_.size()).arg(r.total_ms,0,'f',0));
            running_=false;
        });
    })->start();
}

void CalligraphyOcrPage::applyTheme() {
    const auto& t = ThemeManager::instance().currentTheme();
    currentTheme_ = &t;

    // 页面背景
    setStyleSheet(QString("CalligraphyOcrPage { background: %1; }").arg(t.pageBg.name()));

    // 分割条
    if (mainSplitter_) mainSplitter_->setStyleSheet(
        QString("QSplitter::handle { background: %1; }").arg(t.splitterHandle.name()));

    // 信息栏
    if (auto* ib = findChild<QWidget*>("infoBar"))
        ib->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
            .arg(t.infoBarBg.name(), t.infoBarBorder.name()));

    // 标签
    if (sourceTitleLabel_) sourceTitleLabel_->setStyleSheet(
        QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;")
            .arg(t.labelSecondary.name()));
    if (fileNameLabel_) fileNameLabel_->setStyleSheet(
        QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;")
            .arg(t.labelPrimary.name()));
    if (fileSizeLabel_) fileSizeLabel_->setStyleSheet(
        QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;")
            .arg(t.labelTertiary.name()));
    if (pageLabel_) pageLabel_->setStyleSheet(
        QString("font:9pt 'Microsoft YaHei';color:%1;background:transparent;border:none;")
            .arg(t.labelAccent.name()));

    // PDF 翻页按钮
    auto stylePageBtn = [&](QPushButton* b) {
        if (!b) return;
        b->setStyleSheet(QString(
            "QPushButton{padding:0;background:%1;border:1px solid %2;border-radius:4px;}"
            "QPushButton:hover{background:%3;border-color:%4;}"
            "QPushButton:disabled{background:%5;border-color:%6;}")
            .arg(t.pageBtnBg.name(), t.pageBtnBorder.name(),
                 t.pageBtnHoverBg.name(), t.pageBtnHoverBorder.name(),
                 t.pageBtnDisabledBg.name(), t.pageBtnDisabledBorder.name()));
    };
    stylePageBtn(prevPageBtn_);
    stylePageBtn(nextPageBtn_);

    // 历史面板
    if (auto* hp = findChild<QWidget*>("historyPanel"))
        hp->setStyleSheet(QString("background:%1;").arg(t.historyBg.name()));
    if (historyList_) historyList_->setStyleSheet(QString(
        "QListWidget{background:transparent;border:none;}"
        "QListWidget::item{border-radius:4px;padding:2px;}"
        "QListWidget::item:hover{background:%1;}"
        "QListWidget::item:selected{background:%2;}")
        .arg(t.historyItemHoverBg.name(), t.historyItemSelectedBg.name()));

    // 右侧面板
    if (auto* rp = findChild<QWidget*>("rightPanel"))
        rp->setStyleSheet(QString("background:%1;").arg(t.rightPanelBg.name()));

    // 工具栏
    if (auto* tb = findChild<QWidget*>("ocrToolbar"))
        tb->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;")
            .arg(t.toolbarBg.name(), t.toolbarBorder.name()));

    // 文字预览
    if (textPreview_) textPreview_->setStyleSheet(QString(
        "QTextEdit{font:12pt 'SimHei','Microsoft YaHei';color:%1;background:%2;border:none;"
        "padding:12px;selection-background-color:%3;}")
        .arg(t.textPreviewText.name(), t.textPreviewBg.name(), rgbaStr(t.textSelectionBg)));

    // JSON 预览
    if (jsonPreview_) jsonPreview_->setStyleSheet(QString(
        "QTextEdit{font:9pt 'Consolas',monospace;color:%1;background:%2;border:none;"
        "padding:10px;selection-background-color:%3;}")
        .arg(t.jsonText.name(), t.textPreviewBg.name(), rgbaStr(t.textSelectionBg)));

    // 表格
    if (tableWidget_) tableWidget_->setStyleSheet(QString(
        "QTableWidget{font:10pt 'Microsoft YaHei';color:%1;background:%2;border:1px solid %3;"
        "gridline-color:%4;}"
        "QTableWidget::item:selected{background:%5;}"
        "QHeaderView::section{background:%6;font:9pt 'Microsoft YaHei';padding:4px;border:1px solid %3;}")
        .arg(t.tableText.name(), t.tableBg.name(), t.tableBorder.name(),
             t.tableGridline.name(), rgbaStr(t.tableSelectionBg), t.tableHeaderBg.name()));

    // 图片预览
    if (imageView_) imageView_->setStyleSheet(
        QString("QGraphicsView { border: none; background: %1; }").arg(t.imageViewBg.name()));

    // 检测框颜色 — 由 ImagePreviewView::setBoxStyle 管理，在 drawTextBoxes 时重新应用
}
#include "calligraphy_ocr_page.moc"
