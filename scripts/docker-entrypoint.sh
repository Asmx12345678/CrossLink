#!/usr/bin/env bash
set -euo pipefail

case "${1:-demo}" in
  test)
    ctest --test-dir /workspace/build --output-on-failure
    ;;
  link-sweep)
    exec /workspace/scripts/run-link-sweep.sh
    ;;
  ofdm-sweep)
    exec /workspace/scripts/run-ofdm-sweep.sh
    ;;
  manet-sweep)
    exec /workspace/scripts/run-manet-sweep.sh
    ;;
  demo)
    /workspace/scripts/run-link-sweep.sh
    /workspace/scripts/run-ofdm-sweep.sh
    /workspace/scripts/run-manet-sweep.sh
    python3 /workspace/scripts/validate-results.py
    python3 /workspace/scripts/plot-results.py
    python3 /workspace/scripts/build-report.py
    ;;
  shell)
    exec bash
    ;;
  *)
    exec "$@"
    ;;
esac
