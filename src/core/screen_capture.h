#pragma once
#include <QWidget>
#include <QPixmap>
#include <QRect>

/// 全屏截图 + 橡皮筋选区覆盖层
class ScreenCapture : public QWidget {
public:
    explicit ScreenCapture(QWidget* parent = nullptr);

    /// 返回选中的区域图片，若用户取消则返回 null
    QPixmap capture();

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    QPixmap fullScreen_;
    QRect   selection_;
    QPoint  origin_;
    bool    selecting_ = false;
    bool    done_ = false;
};
