#include "binaural/pinkNoise.hpp"

namespace binaural {

namespace {
// Trammell coefficients
constexpr float A[] = {0.02109238f, 0.07113478f, 0.68873558f};
constexpr float P[] = {0.3190f, 0.7756f, 0.9613f};
constexpr float OFFSET = A[0] + A[1] + A[2];
} // namespace

float PinkNoise::randFloat(unsigned int &seed) {
  seed = seed * 1103515245u + 12345u;
  return (seed >> 16) / 65536.0f;
}

PinkNoise::PinkNoise() : seed_(1u) { clear(); }

void PinkNoise::clear() {
  for (int i = 0; i < NUM_STAGES; ++i) {
    state_[i] = 0.f;
  }
}

float PinkNoise::tick() {
  constexpr float RMI2 = 2.0f / 32767.0f;
  float temp = randFloat(seed_) * 32767.0f;
  state_[0] = P[0] * (state_[0] - temp) + temp;
  temp = randFloat(seed_) * 32767.0f;
  state_[1] = P[1] * (state_[1] - temp) + temp;
  temp = randFloat(seed_) * 32767.0f;
  state_[2] = P[2] * (state_[2] - temp) + temp;
  return (A[0] * state_[0] + A[1] * state_[1] + A[2] * state_[2]) * RMI2 -
         OFFSET;
}

} // namespace binaural
