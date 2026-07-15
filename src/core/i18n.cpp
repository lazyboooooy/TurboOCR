#include "i18n.h"

I18n& I18n::instance() { static I18n i; return i; }

I18n::I18n() {
    // ======================== 英语翻译字典 ========================
    QHash<QString, QString> en;

    // ---- MainWindow ----
    en["OCR 识别"]       = "OCR";
    en["批量 OCR"]       = "Batch OCR";
    en["设置"]           = "Settings";
    en["开发者"]         = "Developer";
    en["关于"]           = "About";
    en["关闭"]           = "Close";
    en["显示"]           = "Show";
    en["退出"]           = "Quit";
    en["最小化"]         = "Minimize";
    en["显示主窗口"]     = "Show Main Window";
    en["关于 TOCR"]      = "About TOCR";
    en["复制邮箱"]       = "Copy Email";

    // ---- Status messages ----
    en["OCR 识别中..."]           = "OCR in progress...";
    en["OCR 识别中，请稍候..."]    = "Recognizing, please wait...";
    en["引擎未就绪"]              = "Engine not ready";
    en["OCR 引擎就绪"]            = "OCR Engine Ready";
    en["引擎初始化失败"]          = "Engine init failed";
    en["已恢复默认设置"]          = "Defaults restored";
    en["批量 OCR 已暂停"]         = "Batch OCR paused";
    en["批量 OCR 已继续"]         = "Batch OCR resumed";
    en["正在 OCR 全部页面..."]     = "OCR all pages...";
    en["正在 OCR 第 %1/%2 页..."]  = "OCR page %1/%2...";
    en["已取消"]                   = "Cancelled";
    en["无识别结果"]               = "No results";
    en["正在重新加载引擎..."]      = "Reloading engine...";
    en["正在识别全部页面，请稍候..."] = "Recognizing all pages, please wait...";
    en["引擎已重新加载"]          = "Engine reloaded";
    en["引擎加载中，请稍候..."]    = "Loading engine, please wait...";
    en["热键截图已识别"]          = "Hotkey screenshot recognized";
    en["剪贴板图片已识别"]        = "Clipboard image recognized";
    en["批量 OCR 正在停止..."]     = "Batch OCR stopping...";
    en["批量 OCR 已停止"]         = "Batch OCR stopped";
    en["批量 OCR 完成"]           = "Batch OCR complete";

    // ---- CalligraphyOcrPage ----
    en["源文件"]       = "Source";
    en["未选择文件"]   = "No file selected";
    en["历史记录"]     = "History";
    en["上一页"]       = "Prev Page";
    en["下一页"]       = "Next Page";
    en["文字识别"]     = "Text";
    en["语音朗读"]     = "Read Aloud";
    en["结果列表"]     = "Result List";
    en["纠正文字"]     = "Correction";
    en["复制结果 (Ctrl+C)"] = "Copy (Ctrl+C)";
    en["清空"]         = "Clear";
    en["导出结果"]     = "Export";
    en["删除"]         = "Delete";
    en["截图"]         = "Cut";
    en["截图OCR"]      = "Cut OCR";
    en["无可朗读文本"] = "No text to read";
    en["已停止朗读"]   = "Reading stopped";
    en["正在朗读..."]  = "Reading aloud...";
    en["无识别结果"]   = "No results";
    en["OCR 结果列表"] = "OCR Results List";
    en["复制选中"]     = "Copy Selected";
    en["纠正模式：直接修改文字，完成后再次点击 ✎ 保存"] = "Correction mode: Edit text directly, click ✎ again to save";
    en["纠正已保存"]   = "Correction saved";
    en["加载失败"]     = "Load Failed";
    en["无法渲染文档: "] = "Cannot render document: ";
    en["加载图片失败"] = "Failed to load image";
    en["已加载 %1×%2"] = "Loaded %1×%2";
    en["已复制表格 (%1行×%2列)"] = "Copied table (%1 rows × %2 cols)";
    en["已复制"]       = "Copied";
    en["已清空"]       = "Cleared";
    en["无结果"]       = "No results";
    en["导出目录"]     = "Export Directory";
    en["已导出到 "]    = "Exported to ";
    en["定位: %1"]     = "Locate: %1";
    en["二维码 — %1"]  = "QR Code — %1";
    en["OCR 完成 — 共 %1 页"]    = "OCR done — %1 pages";
    en["二维码识别完成 — %1"]    = "QR Code scanned — %1";
    en["表格"]                     = "Table";
    en["竖排"]                     = "Vertical";
    en["横排"]                     = "Horizontal";
    en["OCR 完成 — %1 · %2项 · %3ms"] = "OCR done — %1 · %2 items · %3ms";
    en["--- 二维码/条形码 ---"]   = "--- QR/Barcode ---";
    en[" (第%1/%2页)"]            = " (Page %1/%2)";
    en["表格[%1,%2]: %3"]         = "Table[%1,%2]: %3";

    // ---- BatchPage ----
    en["添加文件"]     = "Add Files";
    en["添加文件夹"]   = "Add Folder";
    en["清空列表"]     = "Clear List";
    en["开始批量 OCR"] = "Start Batch OCR";
    en["停止"]         = "Stop";
    en["暂停"]         = "Pause";
    en["继续"]         = "Resume";
    en["并行处理"]     = "Parallel";
    en["并行 worker 数"] = "Parallel workers";
    en["导出"]         = "Export";
    en["待识别文件"]   = "Files to Process";
    en["处理日志"]     = "Log";
    en["选择文件"]     = "Select Files";
    en["选择文件夹"]   = "Select Folder";
    en["已添加 %1 个文件"]       = "Added %1 files";
    en["⏹ 正在停止..."]          = "⏹ Stopping...";
    en["识别进度 %v/%m"]         = "Progress %v/%m";
    en["完成！共 %1 个文件"]     = "Done! %1 files total";
    en["开始处理 %1 个文件%2..."] = "Processing %1 files%2...";
    en["（并行 × %1）"]          = "(parallel × %1)";
    en["⏸ 已暂停..."]            = "⏸ Paused...";
    en["▶ 继续处理..."]          = "▶ Resuming...";
    en["导出结果"]     = "Export Results";
    en["导出选项："]   = "Export Options:";
    en["带标题 (《文件名》)"]     = "With title";
    en["带坐标"]       = "With coordinates";
    en["导出位置："]   = "Export Location:";
    en["浏览..."]      = "Browse...";
    en["取消"]         = "Cancel";
    en["选择导出目录"] = "Select Export Directory";
    en["已导出 %1 个文件到 %2"]   = "Exported %1 files to %2";
    en["行"]           = "rows"; // used in "[N] rows"
    en["失败，已跳过"] = "Failed, skipped";
    // file filter strings
    en["所有支持格式 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.pdf *.xps *.epub *.mobi *.fb2 *.cbz *.docx *.doc);;图片 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;文档 (*.pdf *.xps *.epub *.mobi *.fb2 *.cbz *.docx *.doc)"] =
        "All supported formats (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.pdf *.xps *.epub *.mobi *.fb2 *.cbz *.docx *.doc);;Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;Documents (*.pdf *.xps *.epub *.mobi *.fb2 *.cbz *.docx *.doc)";

    // ---- SettingsPage ----
    en["推理设备"]     = "Inference Device";
    en["设备:"]        = "Device:";
    en["线程数:"]      = "Threads:";
    en["识别批大小:"]  = "Rec Batch Size:";
    en["文本检测"]     = "Text Detection";
    en["检测阈值:"]    = "Det Threshold:";
    en["框阈值:"]      = "Box Threshold:";
    en["扩展比例:"]    = "Unclip Ratio:";
    en["文本识别"]     = "Text Recognition";
    en["置信度阈值:"]  = "Score Threshold:";
    en["全局热键（截图 OCR）"] = "Global Hotkey (Screenshot OCR)";
    en["启用"]         = "Enable";
    en["状态:"]        = "Status:";
    en["快捷键:"]      = "Hotkey:";
    en["后台模式（识别结果自动复制到剪贴板，不弹出界面）"] = "Background mode (auto-copy to clipboard)";
    en["外观主题"]     = "Appearance";
    en["白色"]         = "White";
    en["暗色"]         = "Dark";
    en["主题:"]        = "Theme:";
    en["语言:"]        = "Language:";
    en["切换后需重启生效"] = "Restart to apply";
    en["恢复默认"]     = "Reset Defaults";
    en["应用设置"]     = "Apply";
    en["语言"]         = "Language";
    en["语言已保存，软件将自动重启以应用「%1」"] = "Language saved. Restarting to apply \"%1\"";
    en["错误"]         = "Error";
    en["引擎初始化失败"] = "Engine init failed";

    // ---- DeveloperPage ----
    en["开发者模式"]   = "Developer Mode";
    en["HTTP API 服务"] = "HTTP API Service";
    en["监听端口:"]    = "Port:";
    en["暴露到局域网 (0.0.0.0)，取消则仅本机 (127.0.0.1)"] = "Expose to LAN (0.0.0.0)";
    en["启动服务"]     = "Start Service";
    en["停止服务"]     = "Stop Service";
    en["服务未启动"]   = "Service stopped";
    en["运行日志"]     = "Log";
    en["HTTP API 供其他程序调用 OCR 引擎。\n端点: /api/ocr/image  /api/ocr/pdf  /api/ocr/qrcode  /api/health"] =
        "HTTP API for external programs to call OCR engine.\nEndpoints: /api/ocr/image  /api/ocr/pdf  /api/ocr/qrcode  /api/health";
    en["[HTTP] 已启动: "]   = "[HTTP] Started: ";
    en["[HTTP] 启动失败"]   = "[HTTP] Start failed";
    en["[HTTP] 已停止"]     = "[HTTP] Stopped";
    en["● 运行中 — http://"] = "● Running — http://";
    en["[HTTP] 服务已在运行中"] = "[HTTP] Service already running";

    // ---- Clipboard / Hotkey ----
    en["剪贴板图片"]   = "Clipboard Image";
    en["检测到剪贴板有新图片，是否进行 OCR 识别？"] = "New image detected in clipboard. Perform OCR?";
    en["截图已识别，已复制到剪贴板"]       = "Screenshot OCR done, copied to clipboard";
    en["剪贴板图片已识别，已复制到剪贴板"] = "Clipboard image recognized, copied to clipboard";

    // ---- HTTP API status ----
    en["HTTP API: http://%1:%2"]          = "HTTP API: http://%1:%2";
    en["HTTP API 启动失败 (端口 %1)"]      = "HTTP API start failed (port %1)";

    // ---- Export formats ----
    en["带标题 (《%1》)"] = "With Title (%1)";

    // ---- File size units (not translated, kept for completeness) ----
    en[" B"]   = " B";
    en[" KB"]  = " KB";
    en[" MB"]  = " MB";

    dicts_["en"] = en;
    current_ = "zh_CN";
}

void I18n::setLanguage(const QString& code) {
    current_ = code;

    // Qt 内置翻译（对话框按钮等）
    qApp->removeTranslator(&qtTrans_);
    QString qmDir = QApplication::applicationDirPath() + "/translations";
    if (code != "zh_CN") {
        (void)qtTrans_.load("qt_" + code + ".qm", qmDir);
        qApp->installTranslator(&qtTrans_);
    }
}

void I18n::loadDict(const QString& code) {
    // 字典在构造函数中已加载
}

QString I18n::t(const QString& src) const {
    if (current_ == "zh_CN") return src;
    auto it = dicts_.find(current_);
    if (it == dicts_.end()) return src;
    auto found = it->find(src);
    return found != it->end() ? *found : src;
}
