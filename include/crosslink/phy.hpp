#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace crosslink {

enum class Modulation { Bpsk, Qpsk, Qam16 };
enum class ChannelModel { Awgn, Rayleigh };

struct LinkConfig {
    std::size_t information_bits{100000};
    double snr_db{8.0};
    std::uint64_t seed{20261007};
    Modulation modulation{Modulation::Qpsk};
    ChannelModel channel{ChannelModel::Awgn};
    bool convolutional_coding{false};
};

struct LinkMetrics {
    std::size_t information_bits{};
    std::size_t bit_errors{};
    std::size_t transmitted_symbols{};
    double ber{};
    double evm_rms{};
    double spectral_efficiency{};
    double mean_channel_power{};
};

std::vector<std::uint8_t> convolutional_encode(
    const std::vector<std::uint8_t>& bits);

std::vector<std::uint8_t> viterbi_decode_hard(
    const std::vector<std::uint8_t>& encoded,
    std::size_t information_length);

std::vector<std::complex<double>> modulate(
    const std::vector<std::uint8_t>& bits,
    Modulation modulation);

std::vector<std::uint8_t> demodulate(
    const std::vector<std::complex<double>>& symbols,
    Modulation modulation);

LinkMetrics simulate_link(const LinkConfig& config);

std::string to_string(Modulation modulation);
std::string to_string(ChannelModel channel);
Modulation parse_modulation(const std::string& value);
ChannelModel parse_channel(const std::string& value);

}  // namespace crosslink
