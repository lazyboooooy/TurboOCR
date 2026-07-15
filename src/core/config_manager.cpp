#include "config_manager.h"
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QKeySequence>

// ======================== HotkeyConfig ========================
QString HotkeyConfig::toString() const {
    if (key == 0) return "";
    QKeySequence ks(modifiers | key);
    return ks.toString(QKeySequence::NativeText);
}

HotkeyConfig HotkeyConfig::fromString(const QString& s) {
    HotkeyConfig h;
    if (s.isEmpty()) return h;
    QKeySequence ks(s, QKeySequence::NativeText);
    if (ks.isEmpty()) return h;
    int combo = ks[0];
    h.key = combo & ~Qt::KeyboardModifierMask;
    h.modifiers = combo & Qt::KeyboardModifierMask;
    h.enabled = true;
    return h;
}

// ======================== ConfigManager ========================
ConfigManager& ConfigManager::instance() {
    static ConfigManager mgr;
    return mgr;
}

void ConfigManager::setIniPath(const QString& path) {
    iniPath_ = path;
}

bool ConfigManager::load() {
    if (iniPath_.isEmpty()) return false;

    QSettings s(iniPath_, QSettings::IniFormat);

    s.beginGroup("common");
    ocrParams_.model_size           = s.value("model_size", "medium").toString().toStdString();
    ocrParams_.device              = s.value("device", "cpu").toString().toStdString();
    ocrParams_.cpu_threads         = s.value("cpu_threads", 8).toInt();
    ocrParams_.enable_mkldnn       = s.value("enable_mkldnn", true).toBool();
    ocrParams_.mkldnn_cache_capacity = s.value("mkldnn_cache_capacity", 10).toInt();
    ocrParams_.rec_batch_size      = s.value("text_recognition_batch_size", 6).toInt();
    ocrParams_.det_thresh          = s.value("text_det_thresh", 0.3f).toFloat();
    ocrParams_.det_box_thresh      = s.value("text_det_box_thresh", 0.2f).toFloat();
    ocrParams_.det_unclip_ratio    = s.value("text_det_unclip_ratio", 2.0f).toFloat();
    ocrParams_.rec_score_thresh    = s.value("text_rec_score_thresh", 0.0f).toFloat();
    // 模型路径：优先从配置显式读取，未设置则留空由 initEngine 根据 model_size 推导
    ocrParams_.det_model_dir = s.value("v6/det_model_dir", "").toString().toStdString();
    ocrParams_.rec_model_dir = s.value("v6/rec_model_dir", "").toString().toStdString();
    s.endGroup();

    // 热键
    s.beginGroup("hotkey");
    QString hkStr = s.value("screenshot", "Ctrl+Q").toString();
    hotkey_ = HotkeyConfig::fromString(hkStr);
    hotkey_.enabled = s.value("enabled", true).toBool();
    backgroundMode_ = s.value("background_mode", false).toBool();
    s.endGroup();

    s.beginGroup("ui");
    themeName_ = s.value("theme", "白色").toString();
    language_  = s.value("language", "zh_CN").toString();
    s.endGroup();

    s.beginGroup("http");
    httpEnabled_  = s.value("enabled", false).toBool();
    httpPort_     = s.value("port", 19527).toUInt();
    httpLanAccess_ = s.value("lan_access", false).toBool();
    s.endGroup();

    return true;
}

bool ConfigManager::save() {
    if (iniPath_.isEmpty()) return false;

    QSettings s(iniPath_, QSettings::IniFormat);

    s.beginGroup("common");
    s.setValue("model_size",  QString::fromStdString(ocrParams_.model_size));
    s.setValue("device",       QString::fromStdString(ocrParams_.device));
    s.setValue("cpu_threads",  ocrParams_.cpu_threads);
    s.setValue("enable_mkldnn", ocrParams_.enable_mkldnn);
    s.setValue("mkldnn_cache_capacity", ocrParams_.mkldnn_cache_capacity);
    s.setValue("text_recognition_batch_size", ocrParams_.rec_batch_size);
    s.setValue("text_det_thresh",       (double)ocrParams_.det_thresh);
    s.setValue("text_det_box_thresh",   (double)ocrParams_.det_box_thresh);
    s.setValue("text_det_unclip_ratio", (double)ocrParams_.det_unclip_ratio);
    s.setValue("text_rec_score_thresh", (double)ocrParams_.rec_score_thresh);
    s.endGroup();

    // 热键
    s.beginGroup("hotkey");
    s.setValue("screenshot", hotkey_.toString());
    s.setValue("enabled", hotkey_.enabled);
    s.setValue("background_mode", backgroundMode_);
    s.endGroup();

    s.beginGroup("http");
    s.setValue("enabled", httpEnabled_);
    s.setValue("port", httpPort_);
    s.setValue("lan_access", httpLanAccess_);
    s.endGroup();

    s.beginGroup("ui");
    s.setValue("theme", themeName_);
    s.setValue("language", language_);
    s.endGroup();

    return true;
}
