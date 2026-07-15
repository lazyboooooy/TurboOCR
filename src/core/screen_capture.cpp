#include "screen_capture.h"
#include "theme.h"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGuiApplication>

ScreenCapture::ScreenCapture(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setCursor(Qt::CrossCursor);
}

QPixmap ScreenCapture::capture() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) return {};
    fullScreen_ = screen->grabWindow(0);

    // 高 DPI 适配：widget 用物理像素尺寸，与截图 pixmap 对齐
    QRect geo = screen->geometry();
    qreal dpr = screen->devicePixelRatio();
    setGeometry(geo.x(), geo.y(),
                qRound(geo.width() * dpr), qRound(geo.height() * dpr));
    selection_ = QRect();
    selecting_ = false;
    done_ = false;

    show();
    raise();
    activateWindow();

    // 阻塞直到选区完成
    while (!done_) {
        QApplication::processEvents(QEventLoop::WaitForMoreEvents);
    }
    hide();

    if (selection_.width() < 4 || selection_.height() < 4) return {};
    return fullScreen_.copy(selection_);
}

void ScreenCapture::paintEvent(QPaintEvent*) {
    const auto& t = ThemeManager::instance().currentTheme();
    QPainter p(this);
    p.drawPixmap(0, 0, fullScreen_);
    p.fillRect(rect(), t.captureDimBg);

    if (!selection_.isNull() && selection_.width() > 0 && selection_.height() > 0) {
        p.drawPixmap(selection_, fullScreen_, selection_);
        p.setPen(QPen(t.captureBorder, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(selection_);
        QString label = QString("%1 × %2").arg(selection_.width()).arg(selection_.height());
        p.setPen(t.captureLabelText);
        QFont f("Microsoft YaHei", 10); p.setFont(f);
        QRect labelRect(selection_.left() + 4, selection_.top() - 22, 200, 20);
        p.fillRect(labelRect, t.captureLabelBg);
        p.drawText(labelRect, Qt::AlignCenter, label);
    }
}

void ScreenCapture::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        origin_ = e->pos();
        selection_ = QRect(origin_, QSize());
        selecting_ = true;
    }
}

void ScreenCapture::mouseMoveEvent(QMouseEvent* e) {
    if (selecting_) {
        selection_ = QRect(origin_, e->pos()).normalized();
        update();
    }
}

void ScreenCapture::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton && selecting_) {
        selecting_ = false;
        selection_ = QRect(origin_, e->pos()).normalized();
        done_ = true;
    }
}

void ScreenCapture::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        selection_ = QRect();
        done_ = true;
    }
}
