#include "loading_overlay.h"
#include "theme.h"
#include <QtWidgets/QVBoxLayout>
#include <QAbstractScrollArea>
#include <QEvent>
#include <QResizeEvent>

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false); // 拦截点击防重复操作
    setVisible(false);

    auto* lay = new QVBoxLayout(this);
    lay->setAlignment(Qt::AlignCenter);
    lay->setContentsMargins(0, 0, 0, 0);

    hintLabel_ = new QLabel(this);
    hintLabel_->setAlignment(Qt::AlignCenter);
    hintLabel_->setStyleSheet(
        "color: #ffffff; font: 11pt 'Microsoft YaHei';"
        "background: transparent; border: none;"
    );
    lay->addStretch();
    lay->addWidget(hintLabel_);
    lay->addStretch();

    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, [this]() {
        angle_ = (angle_ + 12) % 360;
        update();
    });

    // 监听父控件 resize 事件，自动同步遮罩尺寸
    if (parent) {
        parent->installEventFilter(this);
    }
}

void LoadingOverlay::syncGeometry() {
    QWidget* p = parentWidget();
    if (!p) return;
    // QAbstractScrollArea 子类（QGraphicsView, QScrollArea）使用 viewport rect
    if (auto* sa = qobject_cast<QAbstractScrollArea*>(p)) {
        if (sa->viewport()) {
            setGeometry(sa->viewport()->rect());
            return;
        }
    }
    setGeometry(p->rect());
}

void LoadingOverlay::show(const QString& hint) {
    error_ = false;
    hintLabel_->setText(hint);
    const auto& t = ThemeManager::instance().currentTheme();
    hintLabel_->setStyleSheet(QString("color:%1;font:11pt 'Microsoft YaHei';background:transparent;border:none;")
        .arg(t.overlayText.name()));
    syncGeometry();  // 关键：铺满父控件
    setVisible(true);
    raise();
    timer_->start(30);
}

void LoadingOverlay::hide() {
    timer_->stop();
    setVisible(false);
}

void LoadingOverlay::showError(const QString& msg) {
    error_ = true;
    hintLabel_->setText(msg);
    const auto& t = ThemeManager::instance().currentTheme();
    hintLabel_->setStyleSheet(QString("color:%1;font:11pt 'Microsoft YaHei';background:transparent;border:none;")
        .arg(t.overlayErrorText.name()));
    timer_->stop();
    syncGeometry();
    setVisible(true);
    raise();
    update();
    QTimer::singleShot(2000, this, &LoadingOverlay::hide);
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);

    // 获取实际绘制区域（相对于遮罩自身的坐标）
    QRect drawRect = rect();
    int cx = drawRect.width() / 2;
    int cy = drawRect.height() / 2;

    // 半透明遮罩
    const auto& t = ThemeManager::instance().currentTheme();
    p.fillRect(drawRect, error_ ? t.overlayErrorBg : t.overlayBg);

    if (!error_) {
        p.setRenderHint(QPainter::Antialiasing);
        int r = 24, penW = 3;
        int ringY = cy - 30;

        // 底色环
        QPen basePen(t.spinnerTrack, penW);
        basePen.setCapStyle(Qt::RoundCap);
        p.setPen(basePen);
        p.drawArc(cx - r, ringY - r, r * 2, r * 2, 0, 360 * 16);

        // 旋转前景弧
        QPen fgPen(t.spinnerArc, penW);
        fgPen.setCapStyle(Qt::RoundCap);
        p.setPen(fgPen);
        p.drawArc(cx - r, ringY - r, r * 2, r * 2, angle_ * 16, 100 * 16);
    }
}

bool LoadingOverlay::event(QEvent* e) {
    // 拦截鼠标事件，防止穿透触发重复操作
    if (e->type() == QEvent::MouseButtonPress ||
        e->type() == QEvent::MouseButtonRelease ||
        e->type() == QEvent::MouseMove ||
        e->type() == QEvent::Wheel) {
        return true;
    }
    return QWidget::event(e);
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* e) {
    // 父控件 resize 时同步遮罩尺寸
    if (obj == parentWidget() && e->type() == QEvent::Resize) {
        syncGeometry();
    }
    return QWidget::eventFilter(obj, e);
}

#include "loading_overlay.moc"
