#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="${root_dir}/results/ofdm-sweep.csv"
printf '%s\n' 'snr_db,equalizer,information_bits,bit_errors,ber,evm_rms,papr_db,channel_nmse' > "${output}"
for equalizer in ZF MMSE; do
  for snr in 0 4 8 12 16 20 24; do
    "${root_dir}/build/crosslink-ofdm" --frames 250 --snr-db "${snr}" \
      --equalizer "${equalizer}" --seed 20261007 --csv >> "${output}"
  done
done
"${root_dir}/build/crosslink-amc" > "${root_dir}/results/adaptive-mcs.csv"
echo "wrote ${output} and adaptive-mcs.csv"
