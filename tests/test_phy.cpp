#include "crosslink/phy.hpp"
#include "crosslink/ofdm.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main() {
    const std::vector<std::uint8_t> bits{0, 1, 1, 0, 1, 0, 0, 1};
    for (const auto modulation : {
             crosslink::Modulation::Bpsk,
             crosslink::Modulation::Qpsk,
             crosslink::Modulation::Qam16}) {
        const auto symbols = crosslink::modulate(bits, modulation);
        auto recovered = crosslink::demodulate(symbols, modulation);
        recovered.resize(bits.size());
        require(recovered == bits, "noiseless modulation round-trip");
    }

    const auto encoded = crosslink::convolutional_encode(bits);
    const auto decoded = crosslink::viterbi_decode_hard(encoded, bits.size());
    require(decoded == bits, "convolutional codec round-trip");

    crosslink::LinkConfig low;
    low.information_bits = 50000;
    low.snr_db = 0.0;
    low.seed = 7;
    low.modulation = crosslink::Modulation::Bpsk;
    const auto low_metrics = crosslink::simulate_link(low);

    auto high = low;
    high.snr_db = 12.0;
    const auto high_metrics = crosslink::simulate_link(high);
    require(high_metrics.ber < low_metrics.ber, "BER decreases with SNR");
    require(high_metrics.ber < 1e-3, "BPSK high-SNR BER sanity check");
    require(std::abs(high_metrics.mean_channel_power - 1.0) < 1e-12,
            "AWGN channel power is unity");

    crosslink::OfdmConfig ofdmLow;
    ofdmLow.frames = 30;
    ofdmLow.snr_db = 4.0;
    ofdmLow.seed = 11;
    const auto ofdmLowMetrics = crosslink::simulate_ofdm(ofdmLow);
    auto ofdmHigh = ofdmLow;
    ofdmHigh.snr_db = 22.0;
    const auto ofdmHighMetrics = crosslink::simulate_ofdm(ofdmHigh);
    require(ofdmHighMetrics.ber < ofdmLowMetrics.ber, "OFDM BER decreases with SNR");
    require(ofdmHighMetrics.papr_db > 3.0, "OFDM PAPR sanity check");

    std::cout << "All CrossLink PHY tests passed\n";
    return EXIT_SUCCESS;
}
