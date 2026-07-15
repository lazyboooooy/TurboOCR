#include "parsers.h"
#include "../ocr/engine.h"

#include <algorithm>
#include <sstream>
#include <cmath>

// ============================================================
// 文本框排序：自上而下、自左而右
// ============================================================
void sortBoxes(std::vector<std::array<int, 4>>& boxes,
               std::vector<std::string>& lines,
               std::vector<float>& scores)
{
    if (boxes.size() <= 1) return;

    // 创建索引并按 Y 坐标排序
    std::vector<size_t> idx(boxes.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;

    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        int ya = boxes[a][1]; // y0
        int yb = boxes[b][1];
        if (std::abs(ya - yb) > 10) {  // 行间距 > 10px，不同行
            return ya < yb;
        }
        return boxes[a][0] < boxes[b][0]; // 同行，自左而右
    });

    // 重排
    auto sortedBoxes = boxes;
    auto sortedLines = lines;
    auto sortedScores = scores;
    for (size_t i = 0; i < idx.size(); ++i) {
        sortedBoxes[i]  = boxes[idx[i]];
        sortedLines[i]  = lines[idx[i]];
        sortedScores[i] = scores[idx[i]];
    }
    boxes  = std::move(sortedBoxes);
    lines  = std::move(sortedLines);
    scores = std::move(sortedScores);
}

// ============================================================
// 段落合并：检查两行间距是否应合并为同一段
// ============================================================
static bool shouldMerge(const std::array<int, 4>& prevBox,
                        const std::array<int, 4>& currBox)
{
    int prevHeight = prevBox[3] - prevBox[1]; // y1 - y0
    int gap = currBox[1] - prevBox[3];        // curr.y0 - prev.y1
    // 行间距 < 1.5倍行高 → 合并
    return gap < prevHeight * 1.5;
}

// ============================================================
// 计算基准缩进（所有文本框的最小 x0）
// ============================================================
static int baseIndent(const std::vector<std::array<int, 4>>& boxes) {
    if (boxes.empty()) return 0;
    int minX = boxes[0][0];
    for (auto& b : boxes) { if (b[0] < minX) minX = b[0]; }
    return minX;
}

// ============================================================
// ============================================================
// 混合排版：逐框判断横竖排，各自独立处理后合并
// ============================================================
std::string applyParserMixed(const TurboResult& result) {
    if (result.lines.empty()) return "";

    auto& boxes  = result.boxes;
    auto& lines  = result.lines;
    auto& scores = result.scores;

    // 逐框分类：窄高框 (h > w*1.8) → 竖排，其余 → 横排
    std::vector<size_t> hIdx, vIdx;
    for (size_t i = 0; i < boxes.size() && i < lines.size(); ++i) {
        int w = boxes[i][2] - boxes[i][0];
        int h = boxes[i][3] - boxes[i][1];
        if (w <= 0 || h <= 0) { hIdx.push_back(i); continue; }
        if (h > w * 1.8) vIdx.push_back(i);
        else             hIdx.push_back(i);
    }

    // 如果全是同一个方向，直接用原有解析器
    if (vIdx.empty()) {
        return applyParser(result, ParserType::SinglePara);
    }
    if (hIdx.empty()) {
        return applyParser(result, ParserType::None);
    }

    // --- 混合排版 ---
    std::ostringstream oss;

    // 横排部分：sortBoxes + 段落合并
    if (!hIdx.empty()) {
        std::vector<std::array<int, 4>> hBoxes;
        std::vector<std::string> hLines;
        std::vector<float> hScores;
        for (auto i : hIdx) {
            hBoxes.push_back(boxes[i]);
            hLines.push_back(lines[i]);
            if (i < scores.size()) hScores.push_back(scores[i]);
        }
        sortBoxes(hBoxes, hLines, hScores);

        for (size_t i = 0; i < hLines.size(); ++i) {
            if (i > 0) {
                if (shouldMerge(hBoxes[i-1], hBoxes[i]))
                    oss << "";
                else
                    oss << "\n\n";
            }
            oss << hLines[i];
        }
    }

    // 竖排部分：按 x 坐标分列（从右到左），列内从上到下
    if (!vIdx.empty()) {
        if (!hIdx.empty()) oss << "\n\n";  // 分隔

        // 收集竖排框
        struct VBox { int x0, y0, x1, y1; std::string text; };
        std::vector<VBox> vboxes;
        for (auto i : vIdx) {
            vboxes.push_back({boxes[i][0], boxes[i][1], boxes[i][2], boxes[i][3], lines[i]});
        }

        // 按 x 坐标分列（容差 = 平均宽度 * 0.7）
        double avgW = 0;
        for (auto& b : vboxes) avgW += b.x1 - b.x0;
        avgW /= vboxes.size();
        double colThresh = avgW * 0.7;

        // 按 x0 降序排列（右→左），便于分列
        std::sort(vboxes.begin(), vboxes.end(),
            [](auto& a, auto& b) { return a.x0 > b.x0; });

        std::vector<std::vector<VBox>> cols;
        for (auto& b : vboxes) {
            bool placed = false;
            for (auto& col : cols) {
                if (std::abs(b.x0 - col[0].x0) < colThresh) {
                    col.push_back(b); placed = true; break;
                }
            }
            if (!placed) cols.push_back({b});
        }

        // 每列内从上到下排序，按列输出（从右到左）
        for (size_t ci = 0; ci < cols.size(); ++ci) {
            auto& col = cols[ci];
            std::sort(col.begin(), col.end(),
                [](auto& a, auto& b) { return a.y0 < b.y0; });
            if (ci > 0) oss << "\n";
            for (size_t i = 0; i < col.size(); ++i) {
                if (i > 0) oss << "\n";
                oss << col[i].text;
            }
        }
    }

    return oss.str();
}

std::string applyParser(const TurboResult& result, ParserType type) {
    if (result.lines.empty()) return "";

    auto boxes  = result.boxes;
    auto lines  = result.lines;
    auto scores = result.scores;

    // None 模式不排序，保留引擎原始输出顺序（竖排等依赖原始顺序）
    if (type == ParserType::None) {
        std::ostringstream oss;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) oss << "\n";
            oss << lines[i];
        }
        return oss.str();
    }

    // 横排文本：先排序再解析
    sortBoxes(boxes, lines, scores);

    bool isMultiColumn = (type == ParserType::MultiPara ||
                          type == ParserType::MultiLine ||
                          type == ParserType::MultiNone);
    bool preserveParagraph = (type == ParserType::MultiPara ||
                              type == ParserType::SinglePara);
    bool isSingleCode = (type == ParserType::SingleCode);
    bool alwaysNewline = (type == ParserType::MultiLine ||
                          type == ParserType::SingleLine ||
                          isSingleCode);

    int indentBase = isSingleCode ? baseIndent(boxes) : 0;

    std::ostringstream oss;

    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            if (isSingleCode) {
                // SingleCode: 每行独立，保留相对缩进（用空格模拟）
                oss << "\n";
                int x0 = boxes[i][0];
                int relIndent = (x0 - indentBase) / 8; // 每 8px ≈ 1 个空格
                if (relIndent < 0) relIndent = 0;
                if (relIndent > 40) relIndent = 40;    // 最大 40 空格
                oss << std::string(relIndent, ' ');
            } else if (alwaysNewline) {
                oss << "\n";
            } else if (preserveParagraph) {
                // 段落合并判断
                if (shouldMerge(boxes[i-1], boxes[i])) {
                    oss << "";  // 同行合并
                } else {
                    oss << "\n\n"; // 新段落
                }
            } else if (isMultiColumn) {
                // 多栏无换行：检查是否是同一行
                int prevY = boxes[i-1][1];
                int currY = boxes[i][1];
                if (std::abs(prevY - currY) > 10) {
                    oss << " "; // 不同行加空格
                }
            } else {
                // 单栏无换行
                oss << " ";
            }
        } else if (isSingleCode) {
            // 第一行也计算缩进
            int x0 = boxes[0][0];
            int relIndent = (x0 - indentBase) / 8;
            if (relIndent < 0) relIndent = 0;
            if (relIndent > 40) relIndent = 40;
            oss << std::string(relIndent, ' ');
        }
        oss << lines[i];
    }

    return oss.str();
}
