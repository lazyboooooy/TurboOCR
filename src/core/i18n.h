#pragma once
#include <QString>
#include <QHash>
#include <QApplication>
#include <QTranslator>

/// 轻量国际化 — 运行时字典，零 tr() 改造
/// 用法: _T("中文") → 当前语言下返回对应翻译（中文即原样返回）
class I18n {
public:
    static I18n& instance();
    void setLanguage(const QString& code);  // 安装翻译字典 + Qt 翻译器
    QString currentCode() const { return current_; }
    QString t(const QString& src) const;    // 查字典

    // 便捷函数，直接调用 I18n::instance().t(src)
    static QString tr(const QString& src) { return instance().t(src); }

private:
    I18n();
    void loadDict(const QString& code);
    QHash<QString, QHash<QString, QString>> dicts_; // lang → src → trans
    QString current_;
    QTranslator qtTrans_;   // Qt 自带翻译 (对话框按钮等)
};

/// 全局快捷宏
inline QString _T(const QString& src) { return I18n::tr(src); }
