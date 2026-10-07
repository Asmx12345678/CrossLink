#include "crosslink/phy.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>

namespace crosslink {
namespace {

constexpr double kInvSqrt2 = 0.70710678118654752440;
constexpr double kInvSqrt10 = 0.31622776601683793320;

std::uint8_t parity(unsigned value) {
    value ^= value >> 2U;
    value ^= value >> 1U;
    return static_cast<std::uint8_t>(value & 1U);
}

std::size_t bits_per_symbol(Modulation modulation) {
    switch (modulation) {
        case Modulation::Bpsk: return 1;
        case Modulation::Qpsk: return 2;
        case Modulation::Qam16: return 4;
    }
    throw std::invalid_argument("unsupported modulation");
}

double pam4(std::uint8_t msb, std::uint8_t lsb) {
    if (msb == 0 && lsb == 0) return -3.0;
    if (msb == 0 && lsb == 1) return -1.0;
    if (msb == 1 && lsb == 1) return 1.0;
    return 3.0;
}

std::array<std::uint8_t, 2> pam4_bits(double value) {
    if (value < -2.0 * kInvSqrt10) return {0, 0};
    if (value < 0.0) return {0, 1};
    if (value < 2.0 * kInvSqrt10) return {1, 1};
    return {1, 0};
}

}  // namespace

std::vector<std::uint8_t> convolutional_encode(
    const std::vector<std::uint8_t>& bits) {
    std::vector<std::uint8_t> output;
    output.reserve((bits.size() + 2U) * 2U);
    unsigned state = 0U;

    for (std::size_t i = 0; i < bits.size() + 2U; ++i) {
        const unsigned input = i < bits.size() ? bits[i] & 1U : 0U;
        const unsigned reg = (input << 2U) | state;
        output.push_back(parity(reg & 0b111U));
        output.push_back(parity(reg & 0b101U));
        state = (input << 1U) | ((state >> 1U) & 1U);
    }
    return output;
}

std::vector<std::uint8_t> viterbi_decode_hard(
    const std::vector<std::uint8_t>& encoded,
    std::size_t information_length) {
    if (encoded.size() % 2U != 0U) {
        throw std::invalid_argument("encoded bit count must be even");
    }
    const std::size_t steps = encoded.size() / 2U;
    constexpr unsigned kStates = 4U;
    constexpr unsigned kInf = std::numeric_limits<unsigned>::max() / 4U;

    std::array<unsigned, kStates> metric{kInf, kInf, kInf, kInf};
    metric[0] = 0U;
    std::vector<std::array<std::uint8_t, kStates>> previous_state(steps);
    std::vector<std::array<std::uint8_t, kStates>> previous_bit(steps);

    for (std::size_t t = 0; t < steps; ++t) {
        std::array<unsigned, kStates> next{kInf, kInf, kInf, kInf};
        for (unsigned state = 0; state < kStates; ++state) {
            if (metric[state] == kInf) continue;
            for (unsigned input = 0; input <= 1U; ++input) {
                const unsigned reg = (input << 2U) | state;
                const unsigned o0 = parity(reg & 0b111U);
                const unsigned o1 = parity(reg & 0b101U);
                const unsigned branch =
                    (o0 != encoded[2U * t]) + (o1 != encoded[2U * t + 1U]);
                const unsigned candidate = metric[state] + branch;
                const unsigned destination =
                    (input << 1U) | ((state >> 1U) & 1U);
                if (candidate < next[destination]) {
                    next[destination] = candidate;
                    previous_state[t][destination] = static_cast<std::uint8_t>(state);
                    previous_bit[t][destination] = static_cast<std::uint8_t>(input);
                }
            }
        }
        metric = next;
    }

    unsigned state = 0U;
    std::vector<std::uint8_t> decoded(steps, 0U);
    for (std::size_t t = steps; t-- > 0U;) {
        decoded[t] = previous_bit[t][state];
        state = previous_state[t][state];
    }
    decoded.resize(std::min(information_length, decoded.size()));
    return decoded;
}

std::vector<std::complex<double>> modulate(
    const std::vector<std::uint8_t>& bits,
    Modulation modulation) {
    const std::size_t width = bits_per_symbol(modulation);
    const std::size_t count = (bits.size() + width - 1U) / width;
    std::vector<std::complex<double>> symbols;
    symbols.reserve(count);

    auto bit = [&](std::size_t index) {
        return index < bits.size() ? static_cast<std::uint8_t>(bits[index] & 1U) : 0U;
    };

    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t offset = i * width;
        if (modulation == Modulation::Bpsk) {
            symbols.emplace_back(bit(offset) ? 1.0 : -1.0, 0.0);
        } else if (modulation == Modulation::Qpsk) {
            symbols.emplace_back(
                (bit(offset) ? 1.0 : -1.0) * kInvSqrt2,
                (bit(offset + 1U) ? 1.0 : -1.0) * kInvSqrt2);
        } else {
            symbols.emplace_back(
                pam4(bit(offset), bit(offset + 1U)) * kInvSqrt10,
                pam4(bit(offset + 2U), bit(offset + 3U)) * kInvSqrt10);
        }
    }
    return symbols;
}

std::vector<std::uint8_t> demodulate(
    const std::vector<std::complex<double>>& symbols,
    Modulation modulation) {
    std::vector<std::uint8_t> bits;
    bits.reserve(symbols.size() * bits_per_symbol(modulation));
    for (const auto& symbol : symbols) {
        if (modulation == Modulation::Bpsk) {
            bits.push_back(symbol.real() >= 0.0);
        } else if (modulation == Modulation::Qpsk) {
            bits.push_back(symbol.real() >= 0.0);
            bits.push_back(symbol.imag() >= 0.0);
        } else {
            const auto i = pam4_bits(symbol.real());
            const auto q = pam4_bits(symbol.imag());
            bits.insert(bits.end(), {i[0], i[1], q[0], q[1]});
        }
    }
    return bits;
}

LinkMetrics simulate_link(const LinkConfig& config) {
    if (config.information_bits == 0U) {
        throw std::invalid_argument("information_bits must be positive");
    }
    std::mt19937_64 rng(config.seed);
    std::bernoulli_distribution random_bit(0.5);
    std::normal_distribution<double> normal(0.0, 1.0);

    std::vector<std::uint8_t> information(config.information_bits);
    for (auto& value : information) value = static_cast<std::uint8_t>(random_bit(rng));

    std::vector<std::uint8_t> transmitted_bits = config.convolutional_coding
        ? convolutional_encode(information)
        : information;
    const auto transmitted = modulate(transmitted_bits, config.modulation);

    const double snr_linear = std::pow(10.0, config.snr_db / 10.0);
    const double noise_sigma = std::sqrt(1.0 / (2.0 * snr_linear));
    std::vector<std::complex<double>> equalized;
    equalized.reserve(transmitted.size());
    double error_power = 0.0;
    double channel_power = 0.0;

    for (const auto& symbol : transmitted) {
        std::complex<double> h{1.0, 0.0};
        if (config.channel == ChannelModel::Rayleigh) {
            h = {normal(rng) * kInvSqrt2, normal(rng) * kInvSqrt2};
        }
        const std::complex<double> noise{
            normal(rng) * noise_sigma, normal(rng) * noise_sigma};
        const auto received = h * symbol + noise;
        const double h2 = std::norm(h);
        const auto estimate = h2 > 1e-12
            ? received * std::conj(h) / h2
            : std::complex<double>{0.0, 0.0};
        equalized.push_back(estimate);
        error_power += std::norm(estimate - symbol);
        channel_power += h2;
    }

    auto received_bits = demodulate(equalized, config.modulation);
    received_bits.resize(transmitted_bits.size());
    std::vector<std::uint8_t> recovered = config.convolutional_coding
        ? viterbi_decode_hard(received_bits, information.size())
        : received_bits;

    const std::size_t compared = std::min(information.size(), recovered.size());
    std::size_t errors = information.size() - compared;
    for (std::size_t i = 0; i < compared; ++i) {
        errors += information[i] != recovered[i];
    }

    const double coding_rate = static_cast<double>(information.size()) /
                               static_cast<double>(transmitted_bits.size());
    LinkMetrics metrics;
    metrics.information_bits = information.size();
    metrics.bit_errors = errors;
    metrics.transmitted_symbols = transmitted.size();
    metrics.ber = static_cast<double>(errors) / static_cast<double>(information.size());
    metrics.evm_rms = std::sqrt(error_power / static_cast<double>(transmitted.size()));
    metrics.spectral_efficiency =
        static_cast<double>(bits_per_symbol(config.modulation)) * coding_rate;
    metrics.mean_channel_power = channel_power / static_cast<double>(transmitted.size());
    return metrics;
}

std::string to_string(Modulation modulation) {
    switch (modulation) {
        case Modulation::Bpsk: return "BPSK";
        case Modulation::Qpsk: return "QPSK";
        case Modulation::Qam16: return "16QAM";
    }
    return "unknown";
}

std::string to_string(ChannelModel channel) {
    return channel == ChannelModel::Awgn ? "AWGN" : "RAYLEIGH";
}

Modulation parse_modulation(const std::string& value) {
    if (value == "BPSK" || value == "bpsk") return Modulation::Bpsk;
    if (value == "QPSK" || value == "qpsk") return Modulation::Qpsk;
    if (value == "16QAM" || value == "qam16" || value == "16qam") {
        return Modulation::Qam16;
    }
    throw std::invalid_argument("modulation must be BPSK, QPSK, or 16QAM");
}

ChannelModel parse_channel(const std::string& value) {
    if (value == "AWGN" || value == "awgn") return ChannelModel::Awgn;
    if (value == "RAYLEIGH" || value == "rayleigh") return ChannelModel::Rayleigh;
    throw std::invalid_argument("channel must be AWGN or RAYLEIGH");
}

}  // namespace crosslink
