// Guitar-tone features, measured exactly as the reference corpus measured real records
// (guitar-corpus/scripts/analyze.py, documented in its README): so a render of a preset and a
// separated guitar stem from a record can be compared number for number. Only the features that
// can be reproduced exactly without librosa's onset detector and pitch tracker are here; those
// were also the ones the corpus analysis found reliable on dense mixes.
//
//   spectral: the long-term power spectrum (STFT n_fft 8192, hop 2048, Hann, centred frames with
//             reflect padding) averaged over frames within 40 dB of the loudest; the 24
//             third-octave bands 63 Hz to 12.5 kHz as a shape (dB against their sum), the
//             centroid and the 85% roll-off (from 20 Hz), and the share above 3 kHz.
//   flatness: librosa's spectral flatness (n_fft 2048, hop 512, power spectrum floored at 1e-10),
//             median over frames whose RMS is within 30 dB of the loudest, in dB.
//   crest:    the median over 100 ms blocks (within 20 dB of the loudest) of peak over RMS, dB.
//   stereo:   both channels band-passed 200 Hz to 5 kHz (fourth-order Butterworth high- and
//             low-pass sections), their zero-lag correlation.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include "audio_measure.hpp"
#include "dsp/filters.hpp"

namespace tone {

constexpr int kBands = 24;
inline constexpr std::array<int, kBands> kNominal{63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
                                                  1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500};

struct Features {
  std::array<double, kBands> band_db{};
  double centroid_hz = 0.0, rolloff85_hz = 0.0, hf_share_db = 0.0;
  double flatness_db = 0.0, crest100_db = 0.0, stereo_corr = 1.0;
};

namespace detail {

// Power spectra of centred, reflect-padded Hann frames, as librosa.stft computes them.
inline std::vector<std::vector<double>> stft_power(const std::vector<float>& y, int n_fft, int hop) {
  const int pad = n_fft / 2;
  const auto at = [&](long i) -> double {
    const long n = static_cast<long>(y.size());
    if (n == 1) return y[0];
    while (i < 0 || i >= n) { if (i < 0) i = -i; if (i >= n) i = 2 * (n - 1) - i; }
    return y[static_cast<size_t>(i)];
  };
  const long frames = 1 + static_cast<long>(y.size()) / hop;
  std::vector<double> window(static_cast<size_t>(n_fft));
  for (int i = 0; i < n_fft; ++i) window[static_cast<size_t>(i)] = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / n_fft);  // periodic Hann
  std::vector<std::vector<double>> out;
  out.reserve(static_cast<size_t>(frames));
  std::vector<std::complex<double>> buffer(static_cast<size_t>(n_fft));
  for (long f = 0; f < frames; ++f) {
    const long start = f * hop - pad;
    for (int i = 0; i < n_fft; ++i) buffer[static_cast<size_t>(i)] = at(start + i) * window[static_cast<size_t>(i)];
    measure::fft(buffer);
    std::vector<double> p(static_cast<size_t>(n_fft / 2 + 1));
    for (size_t k = 0; k < p.size(); ++k) p[k] = std::norm(buffer[k]);
    out.push_back(std::move(p));
  }
  return out;
}

inline double median(std::vector<double> v) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  const size_t n = v.size();
  return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

}  // namespace detail

inline Features measure(const std::vector<float>& left, const std::vector<float>& right, double rate) {
  Features f;
  std::vector<float> y(left.size());
  for (size_t i = 0; i < y.size(); ++i) y[i] = 0.5f * (left[i] + right[i]);
  // spectral
  {
    const int n_fft = 8192;
    const auto S = detail::stft_power(y, n_fft, 2048);
    double emax = 0.0;
    std::vector<double> e(S.size());
    for (size_t t = 0; t < S.size(); ++t) { for (double v : S[t]) e[t] += v; emax = std::max(emax, e[t]); }
    std::vector<double> P(S.empty() ? 0 : S[0].size(), 0.0);
    size_t active = 0;
    for (size_t t = 0; t < S.size(); ++t) if (e[t] > emax * 1e-4) { ++active; for (size_t k = 0; k < P.size(); ++k) P[k] += S[t][k]; }
    for (auto& v : P) v /= static_cast<double>(std::max<size_t>(active, 1));
    const auto hz = [&](size_t k) { return static_cast<double>(k) * rate / n_fft; };
    std::array<double, kBands> bands{};
    double total = 0.0;
    for (int b = 0; b < kBands; ++b) {
      const double fc = 1000.0 * std::exp2((b - 12) / 3.0), lo = fc * std::exp2(-1.0 / 6.0), hi = fc * std::exp2(1.0 / 6.0);
      for (size_t k = 0; k < P.size(); ++k) if (hz(k) >= lo && hz(k) < hi) bands[static_cast<size_t>(b)] += P[k];
      total += bands[static_cast<size_t>(b)];
    }
    for (int b = 0; b < kBands; ++b) f.band_db[static_cast<size_t>(b)] = 10.0 * std::log10(std::max(bands[static_cast<size_t>(b)] / total, 1e-12));
    double sum = 0.0, weighted = 0.0, high = 0.0;
    for (size_t k = 0; k < P.size(); ++k) if (hz(k) >= 20.0) { sum += P[k]; weighted += P[k] * hz(k); if (hz(k) >= 3000.0) high += P[k]; }
    f.centroid_hz = weighted / sum;
    double running = 0.0;
    for (size_t k = 0; k < P.size(); ++k) if (hz(k) >= 20.0) { running += P[k]; if (running / sum >= 0.85) { f.rolloff85_hz = hz(k); break; } }
    f.hf_share_db = 10.0 * std::log10(std::max(high / sum, 1e-12));
  }
  // flatness over frames within 30 dB of the loudest (frame RMS, 2048 / 512, centred)
  {
    const auto S = detail::stft_power(y, 2048, 512);
    std::vector<double> rms(S.size());
    const long n = static_cast<long>(y.size());
    double rmax = 0.0;
    for (size_t t = 0; t < S.size(); ++t) {
      const long c = static_cast<long>(t) * 512;
      double acc = 0.0;
      for (long i = c - 1024; i < c + 1024; ++i) { const double v = (i >= 0 && i < n) ? y[static_cast<size_t>(i)] : 0.0; acc += v * v; }
      rms[t] = std::sqrt(acc / 2048.0);
      rmax = std::max(rmax, rms[t]);
    }
    std::vector<double> flat;
    for (size_t t = 0; t < S.size(); ++t) {
      if (rms[t] <= rmax * 0.03) continue;
      double logsum = 0.0, sum = 0.0;
      for (double v : S[t]) { const double p = std::max(v, 1e-10); logsum += std::log(p); sum += p; }
      const double m = static_cast<double>(S[t].size());
      flat.push_back(std::exp(logsum / m) / (sum / m));
    }
    f.flatness_db = 10.0 * std::log10(std::max(detail::median(flat), 1e-12));
  }
  // crest over 100 ms blocks (RMS frames centred as analyze.py's rms_env pads them)
  {
    const size_t win = static_cast<size_t>(std::lround(rate * 0.1));
    const size_t blocks = (y.size() + win - 1) / win;
    std::vector<double> fr(blocks), pk(blocks);
    double fmax = 0.0;
    for (size_t b = 0; b < blocks; ++b) {
      double acc = 0.0;
      for (size_t i = 0; i < win; ++i) {
        const long j = static_cast<long>(b * win + i) - static_cast<long>(win / 2);
        if (j >= 0 && j < static_cast<long>(y.size())) acc += static_cast<double>(y[static_cast<size_t>(j)]) * y[static_cast<size_t>(j)];
      }
      fr[b] = std::sqrt(acc / static_cast<double>(win));
      fmax = std::max(fmax, fr[b]);
      for (size_t i = b * win; i < std::min(y.size(), (b + 1) * win); ++i) pk[b] = std::max(pk[b], static_cast<double>(std::abs(y[i])));
    }
    std::vector<double> crest;
    for (size_t b = 0; b < blocks; ++b) if (fr[b] > fmax * 0.1) crest.push_back(20.0 * std::log10(std::max(pk[b], 1e-9) / std::max(fr[b], 1e-9)));
    f.crest100_db = detail::median(crest);
  }
  // stereo correlation, 200 Hz to 5 kHz
  {
    using fuzzboi::dsp::Biquad;
    std::array<Biquad, 4> fl, fr;
    for (auto* bank : {&fl, &fr}) {
      (*bank)[0].set(Biquad::Type::HighPass, 200.0, 0.5412, 0.0, rate);
      (*bank)[1].set(Biquad::Type::HighPass, 200.0, 1.3066, 0.0, rate);
      (*bank)[2].set(Biquad::Type::LowPass, 5000.0, 0.5412, 0.0, rate);
      (*bank)[3].set(Biquad::Type::LowPass, 5000.0, 1.3066, 0.0, rate);
    }
    double ll = 0.0, rr = 0.0, lr = 0.0;
    for (size_t i = 0; i < left.size(); ++i) {
      float l = left[i], r = right[i];
      for (auto& b : fl) l = b.process(l);
      for (auto& b : fr) r = b.process(r);
      ll += static_cast<double>(l) * l; rr += static_cast<double>(r) * r; lr += static_cast<double>(l) * r;
    }
    f.stereo_corr = ll * rr > 0.0 ? lr / std::sqrt(ll * rr) : 1.0;
  }
  return f;
}

}  // namespace tone
