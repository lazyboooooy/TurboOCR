#pragma once
#include "../ocr/engine.h"
#include <QString>

struct HotkeyConfig {
    int  modifiers = 0;   // Qt::ControlModifier | Qt::ShiftModifier 等
    int  key = 0;         // Qt::Key_O 等
    bool enabled = true;
    QString toString() const;           // "Ctrl+Shift+O"
    static HotkeyConfig fromString(const QString& s); // 反向解析
};

/// 单例配置管理器 —— 读写 config.ini
class ConfigManager {
public:
    static ConfigManager& instance();

    void setIniPath(const QString& path);
    bool load();
    bool save();

    OcrParams  ocrParams() const { return ocrParams_; }
    void       setOcrParams(const OcrParams& p) { ocrParams_ = p; }

    HotkeyConfig hotkey() const { return hotkey_; }
    void         setHotkey(const HotkeyConfig& h) { hotkey_ = h; }

    bool backgroundMode() const { return backgroundMode_; }
    void setBackgroundMode(bool v) { backgroundMode_ = v; }

    QString themeName() const { return themeName_; }
    void    setThemeName(const QString& name) { themeName_ = name; }

    QString language() const { return language_; }
    void    setLanguage(const QString& v) { language_ = v; }

    bool    httpEnabled() const { return httpEnabled_; }
    void    setHttpEnabled(bool v) { httpEnabled_ = v; }
    quint16 httpPort() const { return httpPort_; }
    void    setHttpPort(quint16 p) { httpPort_ = p; }
    bool    httpLanAccess() const { return httpLanAccess_; }
    void    setHttpLanAccess(bool v) { httpLanAccess_ = v; }

private:
    ConfigManager() = default;
    OcrParams    ocrParams_;
    HotkeyConfig hotkey_;
    bool         backgroundMode_ = false;
    QString      themeName_ = "白色";
    QString      language_ = "zh_CN";
    bool         httpEnabled_ = false;
    quint16      httpPort_ = 19527;
    bool         httpLanAccess_ = false;
    QString      iniPath_;
};
