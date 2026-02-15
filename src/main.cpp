#include "binaural/audioDriver.hpp"
#include "binaural/period.hpp"
#include "binaural/synthesizer.hpp"
#include "binaural/wavDriver.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif

using namespace binaural;

static void printUsage(const char *prog) {
  std::cout
      << "Binaural beats CLI. Plays or writes WAV with configurable "
         "parameters.\n"
      << "Usage: " << prog << " [options]\n\n"
      << "Options:\n"
      << "  --duration N       Play duration in seconds (default: 10)\n"
      << "  --beatFreq F       Binaural beat frequency in Hz (default: 4)\n"
      << "  --baseFreq F       Base/carrier frequency in Hz (default: 161)\n"
      << "  --noise TYPE       Noise type: none, pink, white (default: none)\n"
      << "  --noiseVol N       Noise volume 0-1 (default: 0.01 when noise "
         "set)\n"
      << "  --isochronic       Use isochronic tones instead of binaural\n"
      << "  --volume F         Master volume 0-1.2 (default: 0.7)\n"
      << "  --help             Print this help\n";
}

static bool parseFloat(const char *s, float &out) {
  char *end = nullptr;
  float v = std::strtof(s, &end);
  if (end == s || *end != '\0')
    return false;
  out = v;
  return true;
}

static bool parseInt(const char *s, int &out) {
  char *end = nullptr;
  long v = std::strtol(s, &end, 10);
  if (end == s || *end != '\0' || v < 0 || v > 86400)
    return false;
  out = static_cast<int>(v);
  return true;
}

int main(int argc, char *argv[]) {
  int durationSec = 10;
  float beatFreq = 4.f;
  float baseFreq = 161.f;
  Period::Background noiseType = Period::Background::None;
  float noiseVol = 0.01f;
  bool isochronic = false;
  float volume = 0.7f;

  for (int i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0) {
      printUsage(argv[0]);
      return 0;
    }
    if (std::strcmp(arg, "--duration") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --duration requires a value\n";
        return 1;
      }
      if (!parseInt(argv[++i], durationSec) || durationSec <= 0) {
        std::cerr << "Error: invalid duration\n";
        return 1;
      }
      continue;
    }
    if (std::strcmp(arg, "--beatFreq") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --beatFreq requires a value\n";
        return 1;
      }
      if (!parseFloat(argv[++i], beatFreq) || beatFreq < 0.5f ||
          beatFreq > 40.f) {
        std::cerr << "Error: beatFreq must be 0.5-40\n";
        return 1;
      }
      continue;
    }
    if (std::strcmp(arg, "--baseFreq") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --baseFreq requires a value\n";
        return 1;
      }
      if (!parseFloat(argv[++i], baseFreq) || baseFreq < 40.f ||
          baseFreq > 500.f) {
        std::cerr << "Error: baseFreq must be 40-500\n";
        return 1;
      }
      continue;
    }
    if (std::strcmp(arg, "--noise") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --noise requires none|pink|white\n";
        return 1;
      }
      std::string t(argv[++i]);
      if (t == "none")
        noiseType = Period::Background::None;
      else if (t == "pink")
        noiseType = Period::Background::PinkNoise;
      else if (t == "white")
        noiseType = Period::Background::WhiteNoise;
      else {
        std::cerr << "Error: noise must be none, pink, or white\n";
        return 1;
      }
      continue;
    }
    if (std::strcmp(arg, "--noiseVol") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --noiseVol requires a value\n";
        return 1;
      }
      if (!parseFloat(argv[++i], noiseVol) || noiseVol < 0.f ||
          noiseVol > 1.f) {
        std::cerr << "Error: noiseVol must be 0-1\n";
        return 1;
      }
      continue;
    }
    if (std::strcmp(arg, "--isochronic") == 0) {
      isochronic = true;
      continue;
    }
    if (std::strcmp(arg, "--volume") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "Error: --volume requires a value\n";
        return 1;
      }
      if (!parseFloat(argv[++i], volume) || volume < 0.f || volume > 1.2f) {
        std::cerr << "Error: volume must be 0-1.2\n";
        return 1;
      }
      continue;
    }
    std::cerr << "Unknown option: " << arg << "\nUse --help for usage.\n";
    return 1;
  }

  Program program;
  program.name = "CLI";
  program.seq.push_back({
      .lengthSec = durationSec,
      .voices = {{
          .freqStart = beatFreq,
          .freqEnd = beatFreq,
          .volume = volume,
          .pitch = baseFreq,
          .isochronic = isochronic,
      }},
      .background = noiseType,
      .backgroundVol = (noiseType != Period::Background::None) ? noiseVol : 0.f,
  });

  SynthesizerConfig config;
  config.sampleRate = 44100;
  config.bufferFrames = 2048;
  config.iscale = 1440;

  Synthesizer synth(config);
  synth.setProgram(program);

  auto driver = createPortAudioDriver();
  bool ok = driver->start(config.sampleRate, config.bufferFrames,
                          [&synth, &config](std::vector<int16_t> &buf) {
                            synth.skewVoices(synth.periodElapsedSec());
                            synth.fillSamples(buf);
                            synth.advanceTime(
                                static_cast<float>(config.bufferFrames) /
                                config.sampleRate);
                          });

  if (!ok) {
    std::cerr << "Failed to start audio. Check PortAudio/device.\n";
    return 1;
  }

  if (auto *wav = dynamic_cast<WavFileDriver *>(driver.get())) {
    std::cout << "PortAudio not found. Writing output.wav (" << durationSec
              << " sec)...\n";
    wav->writeToFile("output.wav", static_cast<float>(durationSec));
    std::cout << "Done. Play output.wav to verify.\n";
    return 0;
  }

  auto startPlayback = [&]() {
    return driver->start(config.sampleRate, config.bufferFrames,
                         [&synth, &config](std::vector<int16_t> &buf) {
                           synth.skewVoices(synth.periodElapsedSec());
                           synth.fillSamples(buf);
                           synth.advanceTime(
                               static_cast<float>(config.bufferFrames) /
                               config.sampleRate);
                         });
  };

  std::cout << "Playing: " << beatFreq << " Hz beat, " << baseFreq
            << " Hz base (" << durationSec
            << " s). Press ENTER to pause, Q to quit.\n";
  bool playing = true;
  bool quit = false;
  float totalElapsed = 0.f;
  auto playStart = std::chrono::steady_clock::now();
  int lastDisplayedSec = -1;
  int lastState = -1;

  while (!quit) {
#ifdef _WIN32
    int key = -1;
    if (_kbhit()) {
      key = _getch();
      if (key == 0 || key == 0xE0)
        (void)_getch();
    }
#else
    int key = -1;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
      int c = getchar();
      if (c != EOF)
        key = static_cast<unsigned char>(c);
    }
#endif
    if (key == 'q' || key == 'Q') {
      quit = true;
      break;
    }
    if (key == '\n' || key == '\r') {
      if (playing) {
        totalElapsed += std::chrono::duration<float>(
                            std::chrono::steady_clock::now() - playStart)
                            .count();
        driver->stop();
        playing = false;
      } else {
        if (totalElapsed >= static_cast<float>(durationSec)) {
          continue;
        }
        if (startPlayback()) {
          playing = true;
          playStart = std::chrono::steady_clock::now();
        } else {
          std::cerr << "Failed to resume.\n";
          return 1;
        }
      }
    }
    float elapsed =
        playing
            ? totalElapsed + std::chrono::duration<float>(
                                 std::chrono::steady_clock::now() - playStart)
                                 .count()
            : totalElapsed;
    if (playing && elapsed >= static_cast<float>(durationSec)) {
      driver->stop();
      playing = false;
      totalElapsed = static_cast<float>(durationSec);
      elapsed = totalElapsed;
    }
    int displaySec = static_cast<int>(elapsed + 0.5f);
    int stateCode =
        playing ? 0 : (totalElapsed >= static_cast<float>(durationSec) ? 2 : 1);
    if (displaySec != lastDisplayedSec || stateCode != lastState) {
      lastDisplayedSec = displaySec;
      lastState = stateCode;
      const char *state = (stateCode == 0)   ? "[Playing]"
                          : (stateCode == 2) ? "[Done]   "
                                             : "[Paused] ";
      std::cout << "\rduration " << displaySec << " / " << durationSec << " s  "
                << state << " " << std::flush;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  driver->stop();
  std::cout << "\nQuit.\n";
  return 0;
}
