#include "crosslink/ofdm.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    crosslink::OfdmConfig config;
    bool csv = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (++i >= argc) throw std::invalid_argument("missing option value");
                return argv[i];
            };
            if (arg == "--frames") config.frames = std::stoull(value());
            else if (arg == "--snr-db") config.snr_db = std::stod(value());
            else if (arg == "--seed") config.seed = std::stoull(value());
            else if (arg == "--equalizer") {
                const auto mode = value();
                if (mode == "ZF") config.equalizer = crosslink::Equalizer::Zf;
                else if (mode == "MMSE") config.equalizer = crosslink::Equalizer::Mmse;
                else throw std::invalid_argument("equalizer must be ZF or MMSE");
            } else if (arg == "--csv") csv = true;
            else throw std::invalid_argument("unknown option: " + arg);
        }
        const auto result = crosslink::simulate_ofdm(config);
        const char* equalizer = config.equalizer == crosslink::Equalizer::Zf ? "ZF" : "MMSE";
        std::cout << std::fixed << std::setprecision(8);
        if (csv) {
            std::cout << config.snr_db << ',' << equalizer << ',' << result.information_bits << ','
                      << result.bit_errors << ',' << result.ber << ',' << result.evm_rms << ','
                      << result.papr_db << ',' << result.channel_nmse << '\n';
        } else {
            std::cout << "{\n  \"snr_db\": " << config.snr_db
                      << ",\n  \"equalizer\": \"" << equalizer
                      << "\",\n  \"ber\": " << result.ber
                      << ",\n  \"evm_rms\": " << result.evm_rms
                      << ",\n  \"papr_db\": " << result.papr_db
                      << ",\n  \"channel_nmse\": " << result.channel_nmse << "\n}\n";
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
