// Stem features, measured as the six-stem song corpus measures a separated stem
// (song-corpus/scripts/features.py), so that a render and a record's htdemucs_6s stem compare
// number for number. The input is mixed to mono as the script does (mean of the channels).
//
//   spectral: the long-term power spectrum (STFT n_fft 8192, hop 2048, Hann, centred frames
//             with zero padding) averaged over frames within 40 dB of the loudest: centroid and
//             85% roll-off from 20 Hz, share above 3 kHz and below 250 Hz; and over the same
//             frames the median spectral flatness (librosa's, on the magnitude plus 1e-10
//             squared and floored at 1e-10, every bin), 20 log10 as the script takes it.
//   crest:    the median over non-overlapping 100 ms blocks (RMS within 20 dB of the loudest)
//             of peak over RMS, dB.
//   stereo:   the broadband zero-lag correlation of left and right.
//   wobble:   librosa's pYIN (65..1050 Hz at 22.05 kHz, frame 2048, hop 256, ten bins a
//             semitone, Viterbi-decoded) on the mono sum; inside each voiced run of at least 30
//             frames that never jumps 60 cents, the standard deviation of the pitch in cents
//             from its own 21-frame moving average; the median over runs. A run still open at
//             the end of the signal is not counted, as the script does not count it.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include "tone_features.hpp"

namespace stem {

struct Features {
  double centroid_hz = 0.0, rolloff85_hz = 0.0, hf_share_db = 0.0, low_share_db = 0.0;
  double flatness_db = 0.0, crest100_db = 0.0, corr = 1.0, wobble_cents = 0.0;
  int wobble_runs = 0;
};

namespace detail {

constexpr double kPyinRate = 22050.0, kFmin = 65.0, kFmax = 1050.0;
constexpr int kFrame = 2048, kHop = 256, kBinsPerSemitone = 10;

// 44.1 kHz to 22.05 kHz: a Kaiser-windowed half-band low-pass, then every second sample.
inline std::vector<double> halve_rate(const std::vector<double>& y) {
  constexpr int kHalf = 64;
  std::array<double, 2 * kHalf + 1> h{};
  const double beta = 8.6, i0b = std::cyl_bessel_i(0.0, beta);
  double sum = 0.0;
  for (int n = -kHalf; n <= kHalf; ++n) {
    const double x = 0.45 * n, sinc = n == 0 ? 1.0 : std::sin(fuzzboi::dsp::kPi * x) / (fuzzboi::dsp::kPi * x);
    const double r = static_cast<double>(n) / kHalf;
    h[static_cast<size_t>(n + kHalf)] = 0.45 * sinc * std::cyl_bessel_i(0.0, beta * std::sqrt(1.0 - r * r)) / i0b;
    sum += h[static_cast<size_t>(n + kHalf)];
  }
  for (double& v : h) v /= sum;
  std::vector<double> out((y.size() + 1) / 2);
  const long n = static_cast<long>(y.size());
  for (size_t i = 0; i < out.size(); ++i) {
    const long c = static_cast<long>(2 * i);
    double acc = 0.0;
    for (long k = -kHalf; k <= kHalf; ++k) if (c + k >= 0 && c + k < n) acc += h[static_cast<size_t>(k + kHalf)] * y[static_cast<size_t>(c + k)];
    out[i] = acc;
  }
  return out;
}

// The pYIN threshold prior: Beta(2, 18) over 100 thresholds, as differences of its CDF.
inline std::array<double, 100> beta_probs() {
  const auto cdf = [](double x) { return 1.0 - std::pow(1.0 - x, 19) - 19.0 * x * std::pow(1.0 - x, 18); };
  std::array<double, 100> p{};
  for (int i = 0; i < 100; ++i) p[static_cast<size_t>(i)] = cdf((i + 1) / 100.0) - cdf(i / 100.0);
  return p;
}

// Frame-by-frame pitch in Hz from pYIN, 0 where unvoiced.
inline std::vector<double> pyin(const std::vector<double>& y) {
  const int min_period = static_cast<int>(std::floor(kPyinRate / kFmax));
  const int max_period = std::min(static_cast<int>(std::ceil(kPyinRate / kFmin)), kFrame - 1);
  const int n_periods = max_period - min_period + 1;
  const int n_bins = static_cast<int>(std::floor(12.0 * kBinsPerSemitone * std::log2(kFmax / kFmin))) + 1;
  const size_t frames = 1 + y.size() / kHop;
  const auto beta = beta_probs();
  std::vector<std::vector<double>> obs(frames, std::vector<double>(static_cast<size_t>(2 * n_bins), 0.0));
  std::vector<std::complex<double>> buf(2 * kFrame);
  std::vector<double> frame(kFrame), d(static_cast<size_t>(max_period + 1)), yin(static_cast<size_t>(n_periods)), shift(static_cast<size_t>(n_periods));
  const long n = static_cast<long>(y.size());
  for (size_t t = 0; t < frames; ++t) {
    const long start = static_cast<long>(t) * kHop - kFrame / 2;
    for (int i = 0; i < kFrame; ++i) { const long j = start + i; frame[static_cast<size_t>(i)] = (j >= 0 && j < n) ? y[static_cast<size_t>(j)] : 0.0; }
    std::fill(buf.begin(), buf.end(), std::complex<double>(0.0));
    for (int i = 0; i < kFrame; ++i) buf[static_cast<size_t>(i)] = frame[static_cast<size_t>(i)];
    measure::fft(buf);
    for (auto& v : buf) v = std::norm(v);
    for (auto& v : buf) v = std::conj(v);
    measure::fft(buf);
    const double scale = 1.0 / static_cast<double>(buf.size());
    // librosa's difference function: d(k) = 2 (ACF(0) - ACF(k)) - sum of y(m)^2 for m < k.
    double energy = 0.0;
    d[0] = 0.0;
    for (int k = 1; k <= max_period; ++k) {
      energy += frame[static_cast<size_t>(k - 1)] * frame[static_cast<size_t>(k - 1)];
      d[static_cast<size_t>(k)] = 2.0 * (buf[0].real() - buf[static_cast<size_t>(k)].real()) * scale - energy;
    }
    double running = 0.0;
    for (int k = 1; k <= max_period; ++k) {
      running += d[static_cast<size_t>(k)];
      if (k >= min_period) yin[static_cast<size_t>(k - min_period)] = d[static_cast<size_t>(k)] / (running / k + 1e-300);
    }
    for (int p = 0; p < n_periods; ++p) {
      double s = 0.0;
      if (p > 0 && p < n_periods - 1) {
        const double a = yin[static_cast<size_t>(p + 1)] + yin[static_cast<size_t>(p - 1)] - 2.0 * yin[static_cast<size_t>(p)];
        const double b = 0.5 * (yin[static_cast<size_t>(p + 1)] - yin[static_cast<size_t>(p - 1)]);
        s = std::abs(b) >= std::abs(a) ? 0.0 : -b / a;
      }
      shift[static_cast<size_t>(p)] = s;
    }
    std::vector<int> troughs;
    if (yin[0] < yin[1]) troughs.push_back(0);
    for (int p = 1; p < n_periods - 1; ++p)
      if (yin[static_cast<size_t>(p)] < yin[static_cast<size_t>(p - 1)] && yin[static_cast<size_t>(p)] <= yin[static_cast<size_t>(p + 1)]) troughs.push_back(p);
    if (yin[static_cast<size_t>(n_periods - 1)] < yin[static_cast<size_t>(n_periods - 2)]) troughs.push_back(n_periods - 1);
    if (troughs.empty()) { for (int b = 0; b < n_bins; ++b) obs[t][static_cast<size_t>(n_bins + b)] = 1.0 / n_bins; continue; }
    std::vector<double> probs(troughs.size(), 0.0);
    for (int th = 0; th < 100; ++th) {
      const double threshold = (th + 1) / 100.0;
      int below = 0;
      for (int p : troughs) if (yin[static_cast<size_t>(p)] < threshold) ++below;
      if (below == 0) continue;
      const double lam = 2.0, norm = (1.0 - std::exp(-lam)) / (1.0 - std::exp(-lam * below));
      int position = 0;
      for (size_t i = 0; i < troughs.size(); ++i)
        if (yin[static_cast<size_t>(troughs[i])] < threshold) probs[i] += norm * std::exp(-lam * position++) * beta[static_cast<size_t>(th)];
    }
    size_t global = 0;
    for (size_t i = 1; i < troughs.size(); ++i) if (yin[static_cast<size_t>(troughs[i])] < yin[static_cast<size_t>(troughs[global])]) global = i;
    double unreached = 0.0;
    for (int th = 0; th < 100; ++th) if (!(yin[static_cast<size_t>(troughs[global])] < (th + 1) / 100.0)) unreached += beta[static_cast<size_t>(th)];
    probs[global] += 0.01 * unreached;
    for (size_t i = 0; i < troughs.size(); ++i) {
      if (probs[i] <= 0.0) continue;
      const double period = min_period + troughs[i] + shift[static_cast<size_t>(troughs[i])];
      const long bin = std::clamp(std::lround(12.0 * kBinsPerSemitone * std::log2(kPyinRate / period / kFmin)), 0L, static_cast<long>(n_bins));
      obs[t][static_cast<size_t>(bin)] = probs[i];
    }
    double voiced = 0.0;
    for (int b = 0; b < n_bins; ++b) voiced += obs[t][static_cast<size_t>(b)];
    voiced = std::clamp(voiced, 0.0, 1.0);
    for (int b = 0; b < n_bins; ++b) obs[t][static_cast<size_t>(n_bins + b)] = (1.0 - voiced) / n_bins;
  }
  // Viterbi over pitch bins times voicing: a triangular window 51 bins wide within a voicing
  // state, row-normalised, scaled by 0.99 to stay voiced or unvoiced and 0.01 to switch;
  // transitions under 1e-4 are not considered.
  const int width = static_cast<int>(std::lround(35.92 * 12.0 * kHop / kPyinRate)) * kBinsPerSemitone + 1;
  const int half = width / 2;
  std::vector<double> tri(static_cast<size_t>(width));
  for (int i = 0; i < width; ++i) tri[static_cast<size_t>(i)] = 1.0 - std::abs(i - (width - 1) / 2.0) / ((width + 1) / 2.0);
  std::vector<double> row_sum(static_cast<size_t>(n_bins), 0.0);
  for (int i = 0; i < n_bins; ++i)
    for (int j = std::max(0, i - half); j <= std::min(n_bins - 1, i + half); ++j) row_sum[static_cast<size_t>(i)] += tri[static_cast<size_t>(j - i + half)];
  const auto trans = [&](int from, int to, bool same) {
    return tri[static_cast<size_t>(to - from + half)] / row_sum[static_cast<size_t>(from)] * (same ? 0.99 : 0.01);
  };
  const int states = 2 * n_bins;
  const double tiny = 2.2250738585072014e-308, floor_p = 1e-4;
  std::vector<double> value(static_cast<size_t>(states)), next(static_cast<size_t>(states));
  std::vector<std::vector<uint16_t>> ptr(frames, std::vector<uint16_t>(static_cast<size_t>(states)));
  for (int s = 0; s < states; ++s) value[static_cast<size_t>(s)] = std::log(obs[0][static_cast<size_t>(s)] + tiny) + std::log(1.0 / states + tiny);
  for (size_t t = 1; t < frames; ++t) {
    for (int j = 0; j < states; ++j) {
      const int jb = j % n_bins, jv = j / n_bins;
      double best = -INFINITY;
      int arg = 0;
      for (int kv = 0; kv < 2; ++kv)
        for (int kb = std::max(0, jb - half); kb <= std::min(n_bins - 1, jb + half); ++kb) {
          const double p = trans(kb, jb, kv == jv);
          if (p < floor_p) continue;
          const int k = kv * n_bins + kb;
          const double cost = value[static_cast<size_t>(k)] + std::log(p + tiny);
          if (cost > best) { best = cost; arg = k; }
        }
      ptr[t][static_cast<size_t>(j)] = static_cast<uint16_t>(arg);
      next[static_cast<size_t>(j)] = std::log(obs[t][static_cast<size_t>(j)] + tiny) + best;
    }
    value.swap(next);
  }
  std::vector<int> path(frames);
  path[frames - 1] = static_cast<int>(std::max_element(value.begin(), value.end()) - value.begin());
  for (size_t t = frames - 1; t > 0; --t) path[t - 1] = ptr[t][static_cast<size_t>(path[t])];
  std::vector<double> f0(frames, 0.0);
  for (size_t t = 0; t < frames; ++t)
    if (path[t] < n_bins) f0[t] = kFmin * std::exp2(path[t] / (12.0 * kBinsPerSemitone));
  return f0;
}

}  // namespace detail

// The wobble in cents (see the header) and how many runs it is the median of.
inline double wobble_cents(const std::vector<double>& mono44k, int* runs = nullptr) {
  const auto f0 = detail::pyin(detail::halve_rate(mono44k));
  std::vector<double> wob, run;
  const auto close_run = [&] {
    if (run.size() < 30) return;
    constexpr size_t k = 21;
    double acc = 0.0, mean = 0.0, sq = 0.0;
    std::vector<double> dev;
    for (size_t i = 0; i < run.size(); ++i) {
      acc += run[i];
      if (i >= k) acc -= run[i - k];
      if (i + 1 >= k) dev.push_back(run[i + 1 - k + k / 2] - acc / k);
    }
    for (double v : dev) mean += v;
    mean /= static_cast<double>(dev.size());
    for (double v : dev) sq += (v - mean) * (v - mean);
    wob.push_back(std::sqrt(sq / static_cast<double>(dev.size())));
  };
  for (double hz : f0) {
    const bool voiced = hz > 0.0;
    const double c = voiced ? 1200.0 * std::log2(hz / 440.0) : 0.0;
    if (voiced && (run.empty() || std::abs(c - run.back()) < 60.0)) { run.push_back(c); continue; }
    close_run();
    run.clear();
    if (voiced) run.push_back(c);
  }
  if (runs) *runs = static_cast<int>(wob.size());
  return tone::detail::median(wob);
}

inline Features measure(const std::vector<float>& left, const std::vector<float>& right, double rate) {
  Features f;
  std::vector<float> y(left.size());
  for (size_t i = 0; i < y.size(); ++i) y[i] = 0.5f * (left[i] + right[i]);
  {
    const int n_fft = 8192;
    const auto S = tone::detail::stft_power(y, n_fft, 2048, false);
    double emax = 0.0;
    std::vector<double> e(S.size());
    for (size_t t = 0; t < S.size(); ++t) { for (double v : S[t]) e[t] += v; emax = std::max(emax, e[t]); }
    std::vector<double> P(S.empty() ? 0 : S[0].size(), 0.0);
    std::vector<double> flat;
    for (size_t t = 0; t < S.size(); ++t) {
      if (e[t] <= emax * 1e-4) continue;
      double logsum = 0.0, power = 0.0;
      for (size_t k = 0; k < P.size(); ++k) {
        P[k] += S[t][k];
        const double m = std::sqrt(S[t][k]) + 1e-10, p = std::max(m * m, 1e-10);
        logsum += std::log(p); power += p;
      }
      const double bins = static_cast<double>(P.size());
      flat.push_back(20.0 * std::log10(std::exp(logsum / bins) / (power / bins) + 1e-12));
    }
    f.flatness_db = tone::detail::median(flat);
    const auto hz = [&](size_t k) { return static_cast<double>(k) * rate / n_fft; };
    double sum = 0.0, weighted = 0.0, high = 0.0, low = 0.0;
    for (size_t k = 0; k < P.size(); ++k) {
      if (hz(k) < 20.0) continue;
      sum += P[k]; weighted += P[k] * hz(k);
      if (hz(k) >= 3000.0) high += P[k];
      if (hz(k) < 250.0) low += P[k];
    }
    f.centroid_hz = weighted / sum;
    double running = 0.0;
    for (size_t k = 0; k < P.size(); ++k) if (hz(k) >= 20.0) { running += P[k]; if (running / sum >= 0.85) { f.rolloff85_hz = hz(k); break; } }
    f.hf_share_db = 10.0 * std::log10(std::max(high / sum, 1e-20));
    f.low_share_db = 10.0 * std::log10(std::max(low / sum, 1e-20));
  }
  {
    const size_t hop = static_cast<size_t>(0.1 * rate);
    std::vector<double> rms, pk;
    for (size_t at = 0; at + hop <= y.size(); at += hop) {
      double acc = 0.0, peak = 0.0;
      for (size_t i = at; i < at + hop; ++i) { acc += static_cast<double>(y[i]) * y[i]; peak = std::max(peak, static_cast<double>(std::abs(y[i]))); }
      rms.push_back(std::sqrt(acc / static_cast<double>(hop)) + 1e-12);
      pk.push_back(peak);
    }
    const double loudest = rms.empty() ? 0.0 : *std::max_element(rms.begin(), rms.end());
    std::vector<double> crest;
    for (size_t b = 0; b < rms.size(); ++b) if (rms[b] > loudest * 0.1) crest.push_back(20.0 * std::log10(pk[b] / rms[b]));
    f.crest100_db = tone::detail::median(crest);
  }
  {
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (size_t i = 0; i < left.size(); ++i) { ll += static_cast<double>(left[i]) * left[i]; rr += static_cast<double>(right[i]) * right[i]; lr += static_cast<double>(left[i]) * right[i]; }
    f.corr = lr / (std::sqrt(ll * rr) + 1e-20);
  }
  std::vector<double> mono(y.begin(), y.end());
  f.wobble_cents = wobble_cents(mono, &f.wobble_runs);
  return f;
}

}  // namespace stem
