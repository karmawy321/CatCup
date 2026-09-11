#include "ai/CaptionEngine.hpp"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <regex>

namespace editor::ai {

namespace {

core::Rational parseTimestamp(const std::string& ts) {
    // Format: [HH:]MM:SS[,.]mmm
    std::string clean = ts;
    for (char& c : clean) {
        if (c == ',' || c == '.') c = ':';
    }
    std::stringstream ss(clean);
    std::string token;
    std::vector<int> parts;
    while (std::getline(ss, token, ':')) {
        if (!token.empty()) {
            parts.push_back(std::stoi(token));
        }
    }
    int h = 0, m = 0, s = 0, ms = 0;
    if (parts.size() == 4) {
        h = parts[0]; m = parts[1]; s = parts[2]; ms = parts[3];
    } else if (parts.size() == 3) {
        m = parts[0]; s = parts[1]; ms = parts[2];
    }
    double totalSec = h * 3600.0 + m * 60.0 + s + (ms / 1000.0);
    if (!std::isfinite(totalSec) || totalSec < 0.0) return core::Rational(0);
    auto totalMs = static_cast<std::int64_t>(std::llround(totalSec * 1000.0));
    return core::Rational(totalMs, 1000);
}

std::string formatSrtTimestamp(const core::Rational& r) {
    double totalSec = static_cast<double>(r);
    if (totalSec < 0.0) totalSec = 0.0;
    int totalMs = static_cast<int>(std::round(totalSec * 1000.0));
    int ms = totalMs % 1000;
    int totalS = totalMs / 1000;
    int s = totalS % 60;
    int totalM = totalS / 60;
    int m = totalM % 60;
    int h = totalM / 60;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d,%03d", h, m, s, ms);
    return std::string(buf);
}

} // namespace

std::vector<CaptionCue> CaptionEngine::parseSrt(const std::string& srtContent) {
    std::vector<CaptionCue> cues;
    std::istringstream stream(srtContent);
    std::string line;

    const std::regex timeRegex(R"((\d{1,2}:\d{2}:\d{2}[,\.]\d{3})\s*-->\s*(\d{1,2}:\d{2}:\d{2}[,\.]\d{3}))");

    core::Rational currentStart{0};
    core::Rational currentEnd{0};
    std::string currentText;
    bool inCue = false;

    while (std::getline(stream, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::smatch match;
        if (std::regex_search(line, match, timeRegex)) {
            if (inCue && !currentText.empty() && currentEnd > currentStart) {
                cues.push_back(CaptionCue{currentStart, currentEnd - currentStart, currentText});
            }
            currentStart = parseTimestamp(match[1].str());
            currentEnd = parseTimestamp(match[2].str());
            currentText.clear();
            inCue = true;
        } else if (inCue) {
            if (line.empty()) {
                if (!currentText.empty() && currentEnd > currentStart) {
                    cues.push_back(CaptionCue{currentStart, currentEnd - currentStart, currentText});
                }
                inCue = false;
                currentText.clear();
            } else {
                // If it's not a pure number (index line)
                bool isIndex = true;
                for (char c : line) {
                    if (c < '0' || c > '9') { isIndex = false; break; }
                }
                if (!isIndex) {
                    if (!currentText.empty()) currentText += "\n";
                    currentText += line;
                }
            }
        }
    }

    if (inCue && !currentText.empty() && currentEnd > currentStart) {
        cues.push_back(CaptionCue{currentStart, currentEnd - currentStart, currentText});
    }

    return cues;
}

std::vector<CaptionCue> CaptionEngine::parseVtt(const std::string& vttContent) {
    // VTT is structurally very similar to SRT, reusing time regex
    return parseSrt(vttContent);
}

std::string CaptionEngine::exportSrt(const std::vector<CaptionCue>& cues) {
    std::ostringstream ss;
    for (std::size_t i = 0; i < cues.size(); ++i) {
        ss << (i + 1) << "\n";
        const auto startTs = formatSrtTimestamp(cues[i].startSec);
        const auto endTs = formatSrtTimestamp(cues[i].startSec + cues[i].durationSec);
        ss << startTs << " --> " << endTs << "\n";
        ss << cues[i].text << "\n\n";
    }
    return ss.str();
}

std::vector<CaptionCue> CaptionEngine::chunkTranscript(
    const std::string& transcript,
    core::Rational totalDuration,
    std::size_t wordsPerCue
) {
    std::vector<CaptionCue> result;
    if (transcript.empty() || totalDuration.num() <= 0 || wordsPerCue == 0) {
        return result;
    }

    std::istringstream stream(transcript);
    std::vector<std::string> words;
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }

    if (words.empty()) {
        return result;
    }

    std::vector<std::string> chunks;
    std::string currentChunk;
    std::size_t count = 0;

    for (const auto& w : words) {
        if (!currentChunk.empty()) currentChunk += " ";
        currentChunk += w;
        if (++count >= wordsPerCue) {
            chunks.push_back(currentChunk);
            currentChunk.clear();
            count = 0;
        }
    }
    if (!currentChunk.empty()) {
        chunks.push_back(currentChunk);
    }

    const core::Rational cueDuration = totalDuration / core::Rational(static_cast<std::int64_t>(chunks.size()), 1);
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        core::Rational start = cueDuration * core::Rational(static_cast<std::int64_t>(i), 1);
        result.push_back(CaptionCue{start, cueDuration, chunks[i]});
    }

    return result;
}

} // namespace editor::ai
