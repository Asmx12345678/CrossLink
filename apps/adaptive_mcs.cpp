#include "crosslink/phy.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct Mcs {
    const char* name;
    crosslink::Modulation modulation;
    bool coding;
};

int main(int argc, char** argv) {
    double targetBer = 0.01;
    if (argc == 3 && std::string(argv[1]) == "--target-ber") targetBer = std::stod(argv[2]);
    const std::vector<Mcs> candidates{
        {"BPSK-1/2", crosslink::Modulation::Bpsk, true},
        {"QPSK-1/2", crosslink::Modulation::Qpsk, true},
        {"QPSK", crosslink::Modulation::Qpsk, false},
        {"16QAM-1/2", crosslink::Modulation::Qam16, true},
        {"16QAM", crosslink::Modulation::Qam16, false},
    };

    std::cout << "snr_db,selected_mcs,ber,spectral_efficiency,target_met\n";
    std::cout << std::fixed << std::setprecision(8);
    for (int snr = -2; snr <= 20; snr += 2) {
        const Mcs* selected = &candidates.front();
        crosslink::LinkMetrics selectedMetrics{};
        bool targetMet = false;
        for (const auto& candidate : candidates) {
            crosslink::LinkConfig config;
            config.information_bits = 60000;
            config.snr_db = snr;
            config.seed = 9000U + static_cast<std::uint64_t>(snr + 2);
            config.channel = crosslink::ChannelModel::Rayleigh;
            config.modulation = candidate.modulation;
            config.convolutional_coding = candidate.coding;
            const auto metrics = crosslink::simulate_link(config);
            if (metrics.ber <= targetBer) {
                selected = &candidate;
                selectedMetrics = metrics;
                targetMet = true;
            }
        }
        if (!targetMet) {
            crosslink::LinkConfig fallback;
            fallback.information_bits = 60000;
            fallback.snr_db = snr;
            fallback.seed = 9000U + static_cast<std::uint64_t>(snr + 2);
            fallback.channel = crosslink::ChannelModel::Rayleigh;
            fallback.modulation = selected->modulation;
            fallback.convolutional_coding = selected->coding;
            selectedMetrics = crosslink::simulate_link(fallback);
        }
        std::cout << snr << ',' << selected->name << ',' << selectedMetrics.ber << ','
                  << selectedMetrics.spectral_efficiency << ','
                  << (targetMet ? "true" : "false") << '\n';
    }
    return EXIT_SUCCESS;
}
