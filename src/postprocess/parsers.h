#pragma once
#include <string>
#include <vector>

struct TurboResult;

/// 排版解析方案（对应 Umi-OCR 的 8 种 parser）
enum class ParserType {
    MultiPara,    // 多栏 - 自然段换行
    MultiLine,    // 多栏 - 总是换行
    MultiNone,    // 多栏 - 无换行
    SinglePara,   // 单栏 - 自然段换行
    SingleLine,   // 单栏 - 总是换行
    SingleNone,   // 单栏 - 无换行
    SingleCode,   // 单栏 - 保留缩进
    None          // 不做处理
};

/// 对 OCR 结果应用排版解析，返回格式化文本
std::string applyParser(const TurboResult& result, ParserType type);

/// 文本框排序（自上而下、自左而右）
void sortBoxes(std::vector<std::array<int, 4>>& boxes,
               std::vector<std::string>& lines,
               std::vector<float>& scores);

/// 混合排版（逐框判断横竖排，各自独立处理后再合并）
std::string applyParserMixed(const TurboResult& result);
