#include "crosslink/ofdm.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <random>
#include <stdexcept>
#include <vector>

namespace crosslink {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kInvSqrt2 = 0.70710678118654752440;

std::vector<std::complex<double>> transform(
    const std::vector<std::complex<double>>& input,
    bool inverse) {
    const std::size_t n = input.size();
    std::vector<std::complex<double>> output(n);
    const double sign = inverse ? 1.0 : -1.0;
    for (std::size_t k = 0; k < n; ++k) {
        std::complex<double> sum{0.0, 0.0};
        for (std::size_t t = 0; t < n; ++t) {
            const double angle = sign * 2.0 * kPi * static_cast<double>(k * t) /
                                 static_cast<double>(n);
            sum += input[t] * std::complex<double>{std::cos(angle), std::sin(angle)};
        }
        output[k] = inverse ? sum / static_cast<double>(n) : sum;
    }
    return output;
}

std::vector<std::size_t> active_bins(std::size_t fftSize) {
    if (fftSize != 64U) throw std::invalid_argument("current OFDM profile requires fft_size=64");
    std::vector<std::size_t> bins;
    for (std::size_t k = 1; k <= 26U; ++k) bins.push_back(k);
    for (std::size_t k = 38U; k < 64U; ++k) bins.push_back(k);
    return bins;
}

bool is_pilot(std::size_t bin) {
    return bin == 7U || bin == 21U || bin == 43U || bin == 57U;
}

std::complex<double> qpsk(std::uint8_t a, std::uint8_t b) {
    return {(a ? 1.0 : -1.0) * kInvSqrt2,
            (b ? 1.0 : -1.0) * kInvSqrt2};
}

std::complex<double> channel_response(
    const std::vector<std::complex<double>>& taps,
    std::size_t bin,
    std::size_t fftSize) {
    std::complex<double> response{0.0, 0.0};
    for (std::size_t tap = 0; tap < taps.size(); ++tap) {
        const double angle = -2.0 * kPi * static_cast<double>(bin * tap) /
                             static_cast<double>(fftSize);
        response += taps[tap] * std::complex<double>{std::cos(angle), std::sin(angle)};
    }
    return response;
}

std::complex<double> interpolate(
    std::size_t bin,
    const std::vector<std::size_t>& pilotBins,
    const std::vector<std::complex<double>>& estimates) {
    if (bin <= pilotBins.front()) return estimates.front();
    if (bin >= pilotBins.back()) return estimates.back();
    for (std::size_t i = 1; i < pilotBins.size(); ++i) {
        if (bin <= pilotBins[i]) {
            const double alpha = static_cast<double>(bin - pilotBins[i - 1U]) /
                                 static_cast<double>(pilotBins[i] - pilotBins[i - 1U]);
            return estimates[i - 1U] * (1.0 - alpha) + estimates[i] * alpha;
        }
    }
    return estimates.back();
}

}  // namespace

OfdmMetrics simulate_ofdm(const OfdmConfig& config) {
    if (config.frames == 0U || config.cyclic_prefix < 3U) {
        throw std::invalid_argument("frames must be positive and cyclic prefix must cover the channel");
    }
    const auto active = active_bins(config.fft_size);
    std::vector<std::size_t> dataBins;
    std::vector<std::size_t> pilotBins;
    for (const auto bin : active) {
        (is_pilot(bin) ? pilotBins : dataBins).push_back(bin);
    }

    const std::vector<std::complex<double>> taps{
        {0.92, 0.0}, {0.28, 0.18}, {0.12, -0.10}};
    std::mt19937_64 rng(config.seed);
    std::bernoulli_distribution bit(0.5);
    std::normal_distribution<double> normal(0.0, 1.0);
    const double snr = std::pow(10.0, config.snr_db / 10.0);
    const double sigma = std::sqrt(1.0 / (2.0 * snr * config.fft_size));

    std::size_t errors = 0U;
    std::size_t totalBits = 0U;
    double evmPower = 0.0;
    double nmsePower = 0.0;
    double channelPower = 0.0;
    double paprSum = 0.0;

    for (std::size_t frame = 0; frame < config.frames; ++frame) {
        std::vector<std::complex<double>> frequency(config.fft_size, {0.0, 0.0});
        std::vector<std::array<std::uint8_t, 2>> frameBits;
        frameBits.reserve(dataBins.size());
        for (const auto bin : pilotBins) frequency[bin] = {1.0, 0.0};
        for (const auto bin : dataBins) {
            const std::array<std::uint8_t, 2> pair{
                static_cast<std::uint8_t>(bit(rng)), static_cast<std::uint8_t>(bit(rng))};
            frameBits.push_back(pair);
            frequency[bin] = qpsk(pair[0], pair[1]);
        }

        const auto time = transform(frequency, true);
        double peak = 0.0;
        double average = 0.0;
        for (const auto& sample : time) {
            peak = std::max(peak, std::norm(sample));
            average += std::norm(sample);
        }
        average /= static_cast<double>(time.size());
        paprSum += 10.0 * std::log10(peak / average);

        std::vector<std::complex<double>> withPrefix;
        withPrefix.insert(withPrefix.end(), time.end() - config.cyclic_prefix, time.end());
        withPrefix.insert(withPrefix.end(), time.begin(), time.end());
        std::vector<std::complex<double>> received(withPrefix.size(), {0.0, 0.0});
        for (std::size_t n = 0; n < withPrefix.size(); ++n) {
            for (std::size_t tap = 0; tap < taps.size(); ++tap) {
                if (n >= tap) received[n] += taps[tap] * withPrefix[n - tap];
            }
            received[n] += std::complex<double>{
                normal(rng) * sigma, normal(rng) * sigma};
        }
        std::vector<std::complex<double>> noPrefix(
            received.begin() + config.cyclic_prefix,
            received.begin() + config.cyclic_prefix + config.fft_size);
        const auto observed = transform(noPrefix, false);

        std::vector<std::complex<double>> pilotEstimates;
        for (const auto bin : pilotBins) pilotEstimates.push_back(observed[bin]);

        std::size_t symbolIndex = 0U;
        for (const auto bin : dataBins) {
            auto hEstimate = interpolate(bin, pilotBins, pilotEstimates);
            if (config.equalizer == Equalizer::Mmse) hEstimate *= snr / (snr + 1.0);
            const double denominator = std::norm(hEstimate) +
                (config.equalizer == Equalizer::Mmse ? 1.0 / snr : 1e-12);
            const auto equalized = observed[bin] * std::conj(hEstimate) / denominator;
            const auto reference = frequency[bin];
            evmPower += std::norm(equalized - reference);
            errors += static_cast<std::uint8_t>(equalized.real() >= 0.0) != frameBits[symbolIndex][0];
            errors += static_cast<std::uint8_t>(equalized.imag() >= 0.0) != frameBits[symbolIndex][1];
            totalBits += 2U;

            const auto actual = channel_response(taps, bin, config.fft_size);
            nmsePower += std::norm(hEstimate - actual);
            channelPower += std::norm(actual);
            ++symbolIndex;
        }
    }

    OfdmMetrics metrics;
    metrics.information_bits = totalBits;
    metrics.bit_errors = errors;
    metrics.ber = static_cast<double>(errors) / totalBits;
    metrics.evm_rms = std::sqrt(evmPower / (totalBits / 2U));
    metrics.papr_db = paprSum / config.frames;
    metrics.channel_nmse = nmsePower / channelPower;
    return metrics;
}

}  // namespace crosslink
