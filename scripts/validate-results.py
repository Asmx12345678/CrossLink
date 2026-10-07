#!/usr/bin/env python3
import csv
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"


def read(name):
    with (RESULTS / name).open(newline="") as stream:
        return list(csv.DictReader(stream))


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def finite(row, fields):
    return all(math.isfinite(float(row[field])) for field in fields)


def main():
    link = read("link-sweep.csv")
    require(len(link) == 108, "link sweep must contain 108 cases")
    require(all(finite(row, ("ber", "evm_rms", "spectral_efficiency")) for row in link),
            "link sweep contains a non-finite metric")
    for channel in ("AWGN", "RAYLEIGH"):
        for modulation in ("BPSK", "QPSK", "16QAM"):
            values = [row for row in link if row["channel"] == channel and
                      row["modulation"] == modulation and row["coding"] == "none"]
            values.sort(key=lambda row: float(row["snr_db"]))
            require(float(values[-1]["ber"]) < float(values[0]["ber"]),
                    f"BER trend failed for {channel}/{modulation}")

    ofdm = read("ofdm-sweep.csv")
    require(len(ofdm) == 14, "OFDM sweep must contain 14 cases")
    for equalizer in ("ZF", "MMSE"):
        values = [row for row in ofdm if row["equalizer"] == equalizer]
        values.sort(key=lambda row: float(row["snr_db"]))
        require(float(values[-1]["ber"]) < float(values[0]["ber"]),
                f"OFDM BER trend failed for {equalizer}")

    amc = read("adaptive-mcs.csv")
    require(len(amc) == 12, "AMC sweep must contain 12 cases")
    require(all(finite(row, ("ber", "spectral_efficiency")) for row in amc),
            "AMC sweep contains a non-finite metric")

    manet = read("manet-sweep.csv")
    require(len(manet) == 12, "MANET sweep must contain four protocols x three seeds")
    require({row["protocol"] for row in manet} == {"AODV", "OLSR", "DSDV", "LQSR"},
            "MANET protocol set is incomplete")
    require(all(float(row["tx_packets"]) > 0 and finite(
        row, ("pdr_percent", "throughput_mbps", "mean_delay_ms")) for row in manet),
        "MANET sweep contains an invalid flow metric")
    require(any(float(row["route_updates"]) > 0 for row in manet if row["protocol"] == "LQSR"),
            "LQSR route controller did not run")

    print("validated 108 link, 14 OFDM, 12 AMC, and 12 MANET cases")


if __name__ == "__main__":
    main()
