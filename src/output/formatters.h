#pragma once
#include <string>
#include <vector>
#include <array>

/// TXT 纯文本——仅文本内容
std::string toPlainTxt(const std::string& text);

/// TXT 带标题——《文件名》头 + 纯文本
std::string toTitledTxt(const std::string& filePath, const std::string& text);

/// TXT 带坐标——《文件名》头 + 每行坐标/置信度 + 文本
std::string toStructuredTxt(const std::string& filePath,
                            const std::vector<std::string>& lines,
                            const std::vector<std::array<int, 4>>& boxes,
                            const std::vector<float>& scores);

/// JSON 带坐标——结构化 JSON 输出
std::string toStructuredJson(const std::string& filePath,
                             const std::vector<std::string>& lines,
                             const std::vector<std::array<int, 4>>& boxes,
                             const std::vector<float>& scores);

/// 输出为 CSV 格式
std::string toCsv(const std::string& text);

/// 输出为 JSONL 格式
std::string toJsonl(const std::string& filePath, const std::string& text);

/// 输出为 Markdown 格式
std::string toMarkdown(const std::string& filePath, const std::string& text);
