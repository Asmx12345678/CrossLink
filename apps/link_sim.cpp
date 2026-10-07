#include "crosslink/phy.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string value_after(int& index, int argc, char** argv) {
    if (index + 1 >= argc) throw std::invalid_argument("missing option value");
    return argv[++index];
}

void print_help() {
    std::cout
        << "CrossLink physical-layer Monte Carlo simulator\n\n"
        << "Options:\n"
        << "  --bits N                Information bits (default 100000)\n"
        << "  --snr-db DB             Symbol SNR in dB (default 8)\n"
        << "  --mod BPSK|QPSK|16QAM   Modulation (default QPSK)\n"
        << "  --channel AWGN|RAYLEIGH Channel model (default AWGN)\n"
        << "  --coding                Enable rate-1/2 convolutional code\n"
        << "  --seed N                Random seed\n"
        << "  --csv                    Print one CSV data row\n";
}

}  // namespace

int main(int argc, char** argv) {
    crosslink::LinkConfig config;
    bool csv = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--bits") {
                config.information_bits = std::stoull(value_after(i, argc, argv));
            } else if (arg == "--snr-db") {
                config.snr_db = std::stod(value_after(i, argc, argv));
            } else if (arg == "--mod") {
                config.modulation = crosslink::parse_modulation(value_after(i, argc, argv));
            } else if (arg == "--channel") {
                config.channel = crosslink::parse_channel(value_after(i, argc, argv));
            } else if (arg == "--seed") {
                config.seed = std::stoull(value_after(i, argc, argv));
            } else if (arg == "--coding") {
                config.convolutional_coding = true;
            } else if (arg == "--csv") {
                csv = true;
            } else if (arg == "--help" || arg == "-h") {
                print_help();
                return EXIT_SUCCESS;
            } else {
                throw std::invalid_argument("unknown option: " + arg);
            }
        }

        const auto metrics = crosslink::simulate_link(config);
        std::cout << std::fixed << std::setprecision(8);
        if (csv) {
            std::cout << config.snr_db << ','
                      << crosslink::to_string(config.modulation) << ','
                      << crosslink::to_string(config.channel) << ','
                      << (config.convolutional_coding ? "conv-1/2" : "none") << ','
                      << metrics.information_bits << ',' << metrics.bit_errors << ','
                      << metrics.ber << ',' << metrics.evm_rms << ','
                      << metrics.spectral_efficiency << ','
                      << metrics.mean_channel_power << '\n';
        } else {
            std::cout << "{\n"
                      << "  \"snr_db\": " << config.snr_db << ",\n"
                      << "  \"modulation\": \"" << crosslink::to_string(config.modulation) << "\",\n"
                      << "  \"channel\": \"" << crosslink::to_string(config.channel) << "\",\n"
                      << "  \"coding\": \"" << (config.convolutional_coding ? "conv-1/2" : "none") << "\",\n"
                      << "  \"information_bits\": " << metrics.information_bits << ",\n"
                      << "  \"bit_errors\": " << metrics.bit_errors << ",\n"
                      << "  \"ber\": " << metrics.ber << ",\n"
                      << "  \"evm_rms\": " << metrics.evm_rms << ",\n"
                      << "  \"spectral_efficiency_bits_per_symbol\": "
                      << metrics.spectral_efficiency << ",\n"
                      << "  \"mean_channel_power\": " << metrics.mean_channel_power << "\n"
                      << "}\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
