#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${root_dir}/build/crosslink-link"
output="${root_dir}/results/link-sweep.csv"
mkdir -p "${root_dir}/results"
printf '%s\n' 'snr_db,modulation,channel,coding,information_bits,bit_errors,ber,evm_rms,spectral_efficiency,mean_channel_power' > "${output}"

for channel in AWGN RAYLEIGH; do
  for modulation in BPSK QPSK 16QAM; do
    for coding in none conv; do
      for snr in 0 2 4 6 8 10 12 14 16; do
        args=(--bits 50000 --snr-db "${snr}" --mod "${modulation}" --channel "${channel}" --seed 20261007 --csv)
        if [[ "${coding}" == "conv" ]]; then args+=(--coding); fi
        "${binary}" "${args[@]}" >> "${output}"
      done
    done
  done
done

echo "wrote ${output}"
