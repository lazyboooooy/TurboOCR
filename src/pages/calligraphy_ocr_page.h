#pragma once
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsRectItem>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtGui/QSyntaxHighlighter>
#include <QtWidgets/QTableWidget>
#include "../core/pdf_render.h"
#include <QtTextToSpeech/QTextToSpeech>
#include <QRegularExpression>
#include <QtCore/QMutex>
#include <atomic>
#include <vector>
#include <array>
#include <string>

class MainWindow;
class LoadingOverlay;
class OcrHistory;

// ---- JSON 语法高亮 ----
class JsonHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit JsonHighlighter(QTextDocument* parent = nullptr);
protected:
    void highlightBlock(const QString& text) override;
private:
    struct Rule { QRegularExpression pattern; QTextCharFormat format; };
    std::vector<Rule> rules_;
};

// ---- 可缩放/拖拽图片预览 ----
class ImagePreviewView : public QGraphicsView {
    Q_OBJECT
public:
    explicit ImagePreviewView(QWidget* parent = nullptr);
    void setImage(const QPixmap& pixmap);
    void clearScene();
    void drawTextBoxes(const std::vector<std::array<int, 4>>& boxes);
    void drawQrBoxes(const std::vector<std::array<int, 4>>& boxes);
    void clearTextBoxes();
    void fitToWindow();
    void fitToWidth();
    void highlightTextBox(int index);  // 高亮指定框（反向联动：点击右侧→左侧画框）
signals:
    void textBoxClicked(int index);
    void textBoxHovered(int index);    // 悬停信号
protected:
    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void leaveEvent(QEvent* e) override;  // 鼠标离开 → 取消悬停
private:
    int  itemIndexAtPos(const QPoint& pos) const;
    void setBoxStyle(QGraphicsRectItem* item, bool hovered);

    QGraphicsScene* scene_ = nullptr;
    QGraphicsPixmapItem* pixmapItem_ = nullptr;
    std::vector<QGraphicsRectItem*> rectItems_;
    bool panning_ = false;
    QPoint lastPanPoint_;
    QPoint pressPos_;
    int clickedBoxIndex_ = -1;
    double zoomLevel_ = 1.0;
    bool initialFitDone_ = false;
    int hoveredIndex_ = -1;
};

// ---- 主 OCR 页面 ----
class CalligraphyOcrPage : public QWidget {
    Q_OBJECT
public:
    explicit CalligraphyOcrPage(MainWindow* mainWindow, QWidget* parent = nullptr);

public slots:
    void onImageDropped(const QString& path);
    void startOcr();

private slots:
    void onModeToggled(bool textMode);
    void copyAll();
    void clearAll();
    void exportResult();
    void onTextBoxClicked(int index);
    void snapshotOcr();
    void goToPage(int index);  // PDF 多页导航
    void speakResult();
    void showResultList();
    void toggleCorrection();
    void onTextBoxHovered(int idx);
    void highlightLine(int idx);
    void onHistoryClicked(QListWidgetItem* item);
    void refreshHistory();

private:
    void setupUi();
    void showEvent(QShowEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    bool detectIsVerticalWriting(const std::vector<std::array<int, 4>>& boxes);
    void updateOcrTextLayout(bool isVertical);
    QStringList layoutVerticalCalligraphy();
    QString layoutHorizontalDocument();
    QString layoutTableDocument();
    bool isTableLayout();
    void applyTheme();

    MainWindow* mainWindow_;

    const struct ThemeColors* currentTheme_ = nullptr;

    QSplitter*     mainSplitter_ = nullptr;
    // 左侧
    QWidget*       leftPanel_ = nullptr;
    QLabel*        sourceTitleLabel_ = nullptr;
    QLabel*        fileNameLabel_ = nullptr;
    QLabel*        fileSizeLabel_ = nullptr;
    ImagePreviewView* imageView_ = nullptr;
    // 历史记录
    QListWidget*   historyList_ = nullptr;
    class OcrHistory* history_ = nullptr;
    PdfRender  pdfRender_;
    QStringList pdfPages_;       // 每页的 PNG 路径，未渲染的项为空字符串（渐进式加载）
    int         pdfPageIndex_ = 0;
    struct PdfPageData { QString text; QString json; };
    QVector<PdfPageData> pdfPagesData_;
    std::atomic<bool> abortOcrAll_{false};  // 中断后台 OCR 线程
    void ocrAllPages();
    /// 确保第 index 页已渲染（渐进式：按需渲染单页）
    void ensurePageRendered(int index);
    // 右侧
    QWidget*       rightPanel_ = nullptr;
    QPushButton*   textModeBtn_ = nullptr;
    QPushButton*   jsonModeBtn_ = nullptr;
    QToolButton*   speakBtn_ = nullptr;
    QToolButton*   listBtn_ = nullptr;
    QToolButton*   correctBtn_ = nullptr;
    QPushButton*   prevPageBtn_ = nullptr;  // PDF 上一页
    QPushButton*   nextPageBtn_ = nullptr;  // PDF 下一页
    QLabel*        pageLabel_ = nullptr;     // "第1/5页"
    QToolButton*   copyBtn_ = nullptr;
    QToolButton*   clearBtn_ = nullptr;
    QToolButton*   exportBtn_ = nullptr;
    QStackedWidget* resultStack_ = nullptr;
    QTextEdit*      textPreview_ = nullptr;
    QTextEdit*      jsonPreview_ = nullptr;
    QTableWidget*   tableWidget_ = nullptr;   // 表格渲染
    JsonHighlighter* jsonHighlighter_ = nullptr;
    bool isTable_ = false;                    // 当前是否为表格版面

    QTextToSpeech*   tts_ = nullptr;
    LoadingOverlay*  overlay_ = nullptr;
    bool textMode_ = true;
    bool isVerticalLayout_ = false;
    bool correctionActive_ = false;

    QString      currentPath_;
    QString      historyFilePath_;  // 存历史记录用的原始路径（PDF 为原始 PDF 路径，图片为自身路径）
    QPixmap      currentPixmap_;
    void buildTableGrid();                     // 表格行列聚类
    void renderTable();                        // 渲染 QTableWidget
    void onTableCellClicked(int row, int col); // 单元格点击 → 左侧定位

    struct TextItem { std::string text; std::array<int,4> box; float score;
                      int tableRow=-1, tableCol=-1; };
    std::vector<TextItem> items_;
    QString      fullText_, jsonStr_;
    QMutex       ocrMutex_;
    bool         running_ = false;
    bool         saveHistory_ = true;   // 是否保存本次 OCR 到历史记录
};
