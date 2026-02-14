#include "binaural/gnauralParser.hpp"
#include <fstream>
#include <sstream>
#include <regex>

namespace binaural {

namespace {

bool parseTxtFormat(std::istream& in, Program& out) {
    float baseFreq = 200.f;
    float noiseVol = 0.f;
    float toneVol = 70.f;
    std::vector<std::tuple<float, float, int>> entries;

    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        if (line.find("[BASEFREQ=") == 0) {
            baseFreq = std::stof(line.substr(10, line.find(']') - 10));
            continue;
        }
        if (line.find("[NOISEVOL=") == 0) {
            noiseVol = std::stof(line.substr(10, line.find(']') - 10)) / 100.f;
            continue;
        }
        if (line.find("[TONEVOL=") == 0) {
            toneVol = std::stof(line.substr(9, line.find(']') - 9)) / 100.f;
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
    const float beatScale = toneVol;
    const bool hasNoise = (noiseVol > 0.001f);

    for (const auto& [fL, fR, sec] : entries) {
        const float beatFreq = std::abs(fL - fR);
        Period p;
        p.lengthSec = sec;
        p.voices.push_back({
            .freqStart = beatFreq,
            .freqEnd = beatFreq,
            .volume = beatScale,
            .pitch = baseFreq,
            .isochronic = false,
        });
        p.background = hasNoise ? Period::Background::PinkNoise : Period::Background::None;
        p.backgroundVol = noiseVol;
        out.seq.push_back(std::move(p));
    }
    return true;
}

bool parseXmlFormat(const std::string& content, Program& out) {
    std::vector<float> beatFreqs;
    std::vector<float> baseFreqs;
    std::vector<int> durations;
    std::vector<float> volL, volR;
    float noiseVol = 0.f;

    std::regex entryRe("<entry[^>]*duration=\"([0-9]+)\"[^>]*beatfreq=\"([0-9.]+)\"[^>]*basefreq=\"([0-9.]+)\"[^>]*volume_left=\"([0-9.]+)\"[^>]*volume_right=\"([0-9.]+)\"[^>]*state=\"([0-9]+)\"");
    std::regex entryRe2("<entry[^>]*beatfreq=\"([0-9.]+)\"[^>]*basefreq=\"([0-9.]+)\"[^>]*duration=\"([0-9]+)\"[^>]*");
    std::regex noiseRe("<noisevol[^>]*>([0-9.]+)</noisevol>");

    std::smatch m;
    std::string s = content;
    while (std::regex_search(s, m, entryRe)) {
        int dur = std::stoi(m[1].str());
        float beat = std::stof(m[2].str());
        float base = std::stof(m[3].str());
        float vl = std::stof(m[4].str());
        float vr = std::stof(m[5].str());
        int state = std::stoi(m[6].str());
        if (state != 0 && dur > 0) {
            durations.push_back(dur);
            beatFreqs.push_back(beat);
            baseFreqs.push_back(base);
            volL.push_back(vl);
            volR.push_back(vr);
        }
        s = m.suffix();
    }
    if (beatFreqs.empty()) {
        s = content;
        while (std::regex_search(s, m, entryRe2)) {
            float beat = std::stof(m[1].str());
            float base = std::stof(m[2].str());
            int dur = std::stoi(m[3].str());
            if (dur > 0) {
                durations.push_back(dur);
                beatFreqs.push_back(beat);
                baseFreqs.push_back(base);
                volL.push_back(0.85f);
                volR.push_back(0.85f);
            }
            s = m.suffix();
        }
    }

    if (beatFreqs.empty()) return false;

    s = content;
    if (std::regex_search(s, m, noiseRe)) {
        noiseVol = std::stof(m[1].str()) / 100.f;
    }

    out.name = "Gnaural";
    out.seq.clear();
    for (size_t i = 0; i < beatFreqs.size(); ++i) {
        Period p;
        p.lengthSec = durations[i];
        const float vol = (i < volL.size()) ? (volL[i] + volR[i]) * 0.5f : 0.85f;
        p.voices.push_back({
            .freqStart = beatFreqs[i],
            .freqEnd = beatFreqs[i],
            .volume = vol,
            .pitch = (i < baseFreqs.size()) ? baseFreqs[i] : 200.f,
            .isochronic = false,
        });
        p.background = (noiseVol > 0.001f) ? Period::Background::PinkNoise : Period::Background::None;
        p.backgroundVol = noiseVol;
        out.seq.push_back(std::move(p));
    }
    return true;
}

}  // namespace

std::optional<Program> parseGnauralFromString(const std::string& content,
                                              const std::string& pathHint) {
    Program out;
    if (content.find("<?xml") != std::string::npos ||
        content.find("<gnaural") != std::string::npos ||
        content.find("<entry") != std::string::npos) {
        if (parseXmlFormat(content, out)) return out;
    }
    std::istringstream iss(content);
    if (parseTxtFormat(iss, out)) return out;
    return std::nullopt;
}

std::optional<Program> parseGnaural(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    f.close();
    return parseGnauralFromString(content, path);
}

}  // namespace binaural
