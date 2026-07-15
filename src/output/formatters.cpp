#include "formatters.h"
#include <sstream>
#include <iomanip>
#include <QFileInfo>
#include <QString>

// ======================== TXT 纯文本 ========================
std::string toPlainTxt(const std::string& text) {
    std::ostringstream oss;
    oss << text;
    if (!text.empty() && text.back() != '\n') oss << "\n";
    return oss.str();
}

// ======================== TXT 带标题（对齐 Umi-OCR: ≦ 文件名 ≧） ========================
std::string toTitledTxt(const std::string& filePath, const std::string& text) {
    std::ostringstream oss;
    oss << "《" << QFileInfo(QString::fromStdString(filePath)).fileName().toStdString()
        << "》\n";
    oss << text;
    if (!text.empty() && text.back() != '\n') oss << "\n";
    return oss.str();
}

// ======================== TXT 带坐标（结构化） ========================
std::string toStructuredTxt(const std::string& filePath,
                            const std::vector<std::string>& lines,
                            const std::vector<std::array<int, 4>>& boxes,
                            const std::vector<float>& scores) {
    std::ostringstream oss;
    oss << "《" << QFileInfo(QString::fromStdString(filePath)).fileName().toStdString()
        << "》\n\n";

    size_t n = lines.size();
    for (size_t i = 0; i < n; ++i) {
        oss << "[" << i << "] ";
        if (i < boxes.size()) {
            auto& b = boxes[i];
            oss << "(" << b[0] << "," << b[1] << ")-(" << b[2] << "," << b[3] << ")";
        }
        if (i < scores.size()) {
            oss << " | 置信度:" << std::fixed << std::setprecision(4) << scores[i];
        }
        oss << "\n";
        oss << lines[i] << "\n\n";
    }
    return oss.str();
}

// ======================== JSON 带坐标（结构化） ========================
std::string toStructuredJson(const std::string& filePath,
                             const std::vector<std::string>& lines,
                             const std::vector<std::array<int, 4>>& boxes,
                             const std::vector<float>& scores) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"file\": \""
        << QFileInfo(QString::fromStdString(filePath)).fileName().toStdString()
        << "\",\n";
    oss << "  \"lines\": [\n";
    size_t n = lines.size();
    for (size_t i = 0; i < n; ++i) {
        oss << "    {\n";
        oss << "      \"text\": \"";
        for (char c : lines[i]) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else if (c == '\n') oss << "\\n";
            else oss << c;
        }
        oss << "\"";
        if (i < boxes.size()) {
            auto& b = boxes[i];
            oss << ",\n      \"box\": [" << b[0] << "," << b[1] << ","
                << b[2] << "," << b[3] << "]";
        }
        if (i < scores.size()) {
            oss << ",\n      \"score\": " << std::fixed << std::setprecision(4) << scores[i];
        }
        oss << "\n    }";
        if (i + 1 < n) oss << ",";
        oss << "\n";
    }
    oss << "  ]\n";
    oss << "}\n";
    return oss.str();
}

// ======================== CSV ========================
std::string toCsv(const std::string& text) {
    std::ostringstream oss;
    oss << "line,text\n";
    std::istringstream iss(text);
    std::string line;
    int idx = 0;
    while (std::getline(iss, line)) {
        oss << ++idx << ",";
        if (line.find(',') != std::string::npos ||
            line.find('"') != std::string::npos ||
            line.find('\n') != std::string::npos) {
            oss << "\"";
            for (char c : line) {
                if (c == '"') oss << "\"\"";
                else oss << c;
            }
            oss << "\"";
        } else {
            oss << line;
        }
        oss << "\n";
    }
    return oss.str();
}

// ======================== JSONL ========================
std::string toJsonl(const std::string& filePath, const std::string& text) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"file\":\"" << QFileInfo(QString::fromStdString(filePath))
                             .fileName().toStdString() << "\",";
    oss << "\"text\":\"";
    for (char c : text) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else if (c == '\n') oss << "\\n";
        else oss << c;
    }
    oss << "\"}";
    return oss.str();
}

// ======================== Markdown ========================
std::string toMarkdown(const std::string& filePath, const std::string& text) {
    std::ostringstream oss;
    oss << "## OCR: "
        << QFileInfo(QString::fromStdString(filePath)).fileName().toStdString()
        << "\n\n";
    oss << "```\n" << text << "\n```\n";
    return oss.str();
}
