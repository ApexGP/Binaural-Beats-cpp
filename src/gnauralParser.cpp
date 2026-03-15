#include "binaural/gnauralParser.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#endif

namespace binaural {

namespace {

/// 安全的 stof：失败返回 defaultVal
float safeStof(const std::string& s, float defaultVal = 0.f) {
    try {
        return std::stof(s);
    } catch (...) {
        return defaultVal;
    }
}

/// 安全的 stoi：失败返回 defaultVal
int safeStoi(const std::string& s, int defaultVal = 0) {
    try {
        return std::stoi(s);
    } catch (...) {
        return defaultVal;
    }
}

/// 从 line 中提取 "[TAG=VALUE]" 格式的值；失败返回 defaultVal
float extractBracketFloat(const std::string& line, size_t tagLen, float defaultVal) {
    size_t closePos = line.find(']');
    if (closePos == std::string::npos || closePos <= tagLen) return defaultVal;
    return safeStof(line.substr(tagLen, closePos - tagLen), defaultVal);
}

bool parseTxtFormat(std::istream& in, Program& out) {
    float baseFreq = 200.f;
    float noiseVol = 0.f;
    float toneVol  = 1.0f;   // M5 修复：初始值为 1.0（100%），而非 70.f
    std::vector<std::tuple<float, float, int>> entries;

    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        if (line.find("[BASEFREQ=") == 0) {
            baseFreq = extractBracketFloat(line, 10, baseFreq);
            continue;
        }
        if (line.find("[NOISEVOL=") == 0) {
            noiseVol = extractBracketFloat(line, 10, noiseVol * 100.f) / 100.f;
            continue;
        }
        if (line.find("[TONEVOL=") == 0) {
            toneVol = extractBracketFloat(line, 9, toneVol * 100.f) / 100.f;
            continue;
        }
        if (line.find('[') == 0) continue;

        float freqL, freqR;
        int dur;
        if (std::sscanf(line.c_str(), "%f, %f, %d", &freqL, &freqR, &dur) == 3 &&
            dur > 0) {
            entries.emplace_back(freqL, freqR, dur);
        }
    }

    if (entries.empty()) return false;

    out.name = "Gnaural";
    out.seq.clear();
    const bool hasNoise = (noiseVol > 0.001f);

    for (const auto& [fL, fR, sec] : entries) {
        const float beatFreq = std::abs(fL - fR);
        Period p;
        p.lengthSec = sec;
        p.voices.push_back({
            .freqStart = beatFreq,
            .freqEnd   = beatFreq,
            .volume    = toneVol,
            .pitch     = baseFreq,
            .isochronic = false,
        });
        p.background    = hasNoise ? Period::Background::PinkNoise : Period::Background::None;
        p.backgroundVol = noiseVol;
        out.seq.push_back(std::move(p));
    }
    return true;
}

bool parseXmlFormat(const std::string& content, Program& out) {
    std::vector<float> beatFreqs;
    std::vector<float> baseFreqs;
    std::vector<int>   durations;
    std::vector<float> volL, volR;
    float noiseVol = 0.f;

    // 三套正则以兼容不同属性顺序的 gnaural XML
    std::regex entryRe("<entry[^>]*duration=\"([0-9.]+)\"[^>]*beatfreq=\"([0-9.]+)\"[^>]*basefreq=\"([0-9.]+)\"[^>]*volume_left=\"([0-9.]+)\"[^>]*volume_right=\"([0-9.]+)\"[^>]*state=\"([0-9]+)\"");
    std::regex entryRe2("<entry[^>]*beatfreq=\"([0-9.]+)\"[^>]*basefreq=\"([0-9.]+)\"[^>]*duration=\"([0-9.]+)\"[^>]*");
    std::regex entryRe3("<entry[^>]*parent=\"[^\"]*\"[^>]*duration=\"([0-9.]+)\"[^>]*volume_left=\"([0-9.]+)\"[^>]*volume_right=\"([0-9.]+)\"[^>]*beatfreq=\"([0-9.]+)\"[^>]*basefreq=\"([0-9.]+)\"[^>]*state=\"([0-9]+)\"");
    std::regex noiseRe("<noisevol[^>]*>([0-9.]+)</noisevol>");

    auto addEntry = [&](float durSec, float beat, float base, float vl, float vr, int /*state*/) {
        if (durSec > 0.0001f && (beat > 0.001f || base > 1.f)) {
            int sec = static_cast<int>(durSec + 0.5f);
            if (sec < 1) sec = 1;
            durations.push_back(sec);
            beatFreqs.push_back(beat);
            baseFreqs.push_back(base);
            volL.push_back(vl);
            volR.push_back(vr);
        }
    };

    std::smatch m;
    std::string s = content;

    // 正则匹配中的数值转换均通过 safeStof/safeStoi 保护
    while (std::regex_search(s, m, entryRe)) {
        addEntry(safeStof(m[1].str()), safeStof(m[2].str()),
                 safeStof(m[3].str()), safeStof(m[4].str()),
                 safeStof(m[5].str()), safeStoi(m[6].str(), 1));
        s = m.suffix();
    }
    if (beatFreqs.empty()) {
        s = content;
        while (std::regex_search(s, m, entryRe3)) {
            addEntry(safeStof(m[1].str()), safeStof(m[4].str()),
                     safeStof(m[5].str()), safeStof(m[2].str()),
                     safeStof(m[3].str()), safeStoi(m[6].str(), 1));
            s = m.suffix();
        }
    }
    if (beatFreqs.empty()) {
        s = content;
        while (std::regex_search(s, m, entryRe2)) {
            addEntry(safeStof(m[3].str()), safeStof(m[1].str()),
                     safeStof(m[2].str()), 0.85f, 0.85f, 1);
            s = m.suffix();
        }
    }

    if (beatFreqs.empty()) return false;

    s = content;
    if (std::regex_search(s, m, noiseRe)) {
        noiseVol = safeStof(m[1].str()) / 100.f;
    }

    out.name = "Gnaural";
    out.seq.clear();
    for (size_t i = 0; i < beatFreqs.size(); ++i) {
        Period p;
        p.lengthSec = durations[i];
        const float vol  = (i < volL.size()) ? (volL[i] + volR[i]) * 0.5f : 0.85f;
        const float beat = beatFreqs[i];
        const bool  isPinkNoiseOnly = (beat < 0.001f);
        p.voices.push_back({
            .freqStart  = beat,
            .freqEnd    = beat,
            .volume     = isPinkNoiseOnly ? 0.f : vol,
            .pitch      = (i < baseFreqs.size()) ? baseFreqs[i] : 200.f,
            .isochronic = false,
        });
        if (isPinkNoiseOnly) {
            p.background    = Period::Background::PinkNoise;
            p.backgroundVol = vol;
        } else {
            p.background    = (noiseVol > 0.001f) ? Period::Background::PinkNoise
                                                   : Period::Background::None;
            p.backgroundVol = noiseVol;
        }
        out.seq.push_back(std::move(p));
    }
    return true;
}

}  // namespace

std::optional<Program> parseGnauralFromString(const std::string& content,
                                              const std::string& pathHint) {
    Program out;
    if (content.find("<?xml")    != std::string::npos ||
        content.find("<gnaural") != std::string::npos ||
        content.find("<entry")   != std::string::npos) {
        if (parseXmlFormat(content, out)) return out;
    }
    std::istringstream iss(content);
    if (parseTxtFormat(iss, out)) return out;
    return std::nullopt;
}

std::optional<Program> parseGnaural(const std::string& path) {
#ifdef _WIN32
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) return std::nullopt;
    std::vector<wchar_t> pathW(static_cast<size_t>(wideLen));
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, pathW.data(), wideLen);
    FILE* fp = _wfopen(pathW.data(), L"rb");
    if (!fp) return std::nullopt;
    std::string content;
    char buf[4096];
    while (size_t n = fread(buf, 1, sizeof(buf), fp))
        content.append(buf, n);
    fclose(fp);
#else
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    f.close();
#endif
    return parseGnauralFromString(content, path);
}

}  // namespace binaural
