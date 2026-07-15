#pragma once
#include <QtWidgets/QWidget>
#include <QTimer>
#include <QPainter>
#include <QLabel>

/// 半透明遮罩 + 环形旋转 Loading（纯 QPainter 绘制，无外部资源依赖）
/// 自动铺满父控件并居中，跟随父控件 resize 同步尺寸
class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    /// 显示遮罩，可选提示文字（自动铺满父控件并居中）
    void show(const QString& hint = "识别中，请稍候...");
    /// 隐藏遮罩
    void hide();
    /// 显示错误提示，2 秒后自动消失
    void showError(const QString& msg);

protected:
    void paintEvent(QPaintEvent*) override;
    bool event(QEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

private:
    void syncGeometry();  // 铺满父控件

    QTimer*  timer_ = nullptr;
    QLabel*  hintLabel_ = nullptr;
    int      angle_ = 0;
    bool     error_ = false;
};
