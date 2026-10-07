#pragma once

#include <cstddef>
#include <cstdint>

namespace crosslink {

enum class Equalizer { Zf, Mmse };

struct OfdmConfig {
    std::size_t frames{250};
    std::size_t fft_size{64};
    std::size_t cyclic_prefix{16};
    double snr_db{15.0};
    std::uint64_t seed{20261007};
    Equalizer equalizer{Equalizer::Mmse};
};

struct OfdmMetrics {
    std::size_t information_bits{};
    std::size_t bit_errors{};
    double ber{};
    double evm_rms{};
    double papr_db{};
    double channel_nmse{};
};

OfdmMetrics simulate_ofdm(const OfdmConfig& config);

}  // namespace crosslink
