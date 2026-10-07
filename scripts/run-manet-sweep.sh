#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ns3_home="${NS3_HOME:-/opt/ns3}"
output="${root_dir}/results/manet-sweep.csv"
mkdir -p "${root_dir}/results"
rm -f "${output}"

for protocol in AODV OLSR DSDV LQSR; do
  for seed in 41 42 43; do
    mobility="${root_dir}/results/mobility-${protocol}-${seed}.csv"
    routes="${root_dir}/results/routes-${protocol}-${seed}.csv"
    cd "${ns3_home}"
    ./ns3 run "scratch/crosslink-manet --protocol=${protocol} --nodes=30 --duration=45 --speed=12 --seed=${seed} --output=${output} --mobilityOutput=${mobility} --routeOutput=${routes}"
  done
done

echo "wrote ${output}"
