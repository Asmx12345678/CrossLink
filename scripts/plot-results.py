#!/usr/bin/env python3
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"


def plot_link() -> None:
    rows = list(csv.DictReader((RESULTS / "link-sweep.csv").open()))
    grouped = defaultdict(list)
    for row in rows:
        key = (row["channel"], row["modulation"], row["coding"])
        grouped[key].append((float(row["snr_db"]), max(float(row["ber"]), 1e-6)))

    for channel in ("AWGN", "RAYLEIGH"):
        fig, ax = plt.subplots(figsize=(9, 5.5))
        for key, values in sorted(grouped.items()):
            if key[0] != channel:
                continue
            values.sort()
            ax.semilogy([x for x, _ in values], [y for _, y in values], marker="o",
                        label=f"{key[1]} / {key[2]}")
        ax.set(title=f"CrossLink BER performance — {channel}", xlabel="SNR (dB)", ylabel="BER")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(ncol=2)
        fig.tight_layout()
        fig.savefig(RESULTS / f"ber-{channel.lower()}.png", dpi=160)
        plt.close(fig)


def plot_manet() -> None:
    rows = list(csv.DictReader((RESULTS / "manet-sweep.csv").open()))
    metrics = defaultdict(list)
    for row in rows:
        metrics[row["protocol"]].append({
            "pdr": float(row["pdr_percent"]),
            "throughput": float(row["throughput_mbps"]),
            "delay": float(row["mean_delay_ms"]),
        })
    protocols = sorted(metrics)
    mean = lambda name, protocol: sum(x[name] for x in metrics[protocol]) / len(metrics[protocol])
    pdr = [mean("pdr", p) for p in protocols]
    throughput = [mean("throughput", p) for p in protocols]
    delay = [mean("delay", p) for p in protocols]

    fig, axes = plt.subplots(1, 3, figsize=(13, 4.3))
    for ax, values, title, unit in zip(
        axes, (pdr, throughput, delay),
        ("Packet delivery ratio", "Throughput", "Mean end-to-end delay"),
        ("%", "Mbit/s", "ms"),
    ):
        ax.bar(protocols, values, color=["#5577aa", "#66aa77", "#ddaa55", "#cc6677"])
        ax.set_title(title)
        ax.set_ylabel(unit)
        ax.grid(axis="y", alpha=0.25)
    fig.suptitle("CrossLink MANET routing comparison (3 deterministic seeds)")
    fig.tight_layout()
    fig.savefig(RESULTS / "routing-comparison.png", dpi=160)
    plt.close(fig)


def plot_ofdm() -> None:
    rows = list(csv.DictReader((RESULTS / "ofdm-sweep.csv").open()))
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["equalizer"]].append(
            (float(row["snr_db"]), max(float(row["ber"]), 1e-6), float(row["channel_nmse"]))
        )

    fig, axes = plt.subplots(1, 2, figsize=(11, 4.5))
    for equalizer, values in sorted(grouped.items()):
        values.sort()
        axes[0].semilogy([x[0] for x in values], [x[1] for x in values], marker="o", label=equalizer)
        axes[1].semilogy([x[0] for x in values], [max(x[2], 1e-8) for x in values], marker="o",
                        label=equalizer)
    axes[0].set(title="OFDM bit error rate", xlabel="SNR (dB)", ylabel="BER")
    axes[1].set(title="Pilot-aided channel estimation", xlabel="SNR (dB)", ylabel="Channel NMSE")
    for ax in axes:
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()
    fig.suptitle("CrossLink OFDM receiver comparison")
    fig.tight_layout()
    fig.savefig(RESULTS / "ofdm-equalizers.png", dpi=160)
    plt.close(fig)


def plot_amc() -> None:
    rows = list(csv.DictReader((RESULTS / "adaptive-mcs.csv").open()))
    snr = [float(row["snr_db"]) for row in rows]
    efficiency = [float(row["spectral_efficiency"]) for row in rows]
    labels = [row["selected_mcs"] for row in rows]

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.step(snr, efficiency, where="post", color="#3a78b8", linewidth=2)
    ax.scatter(snr, efficiency, color="#3a78b8")
    last = None
    for x, y, label in zip(snr, efficiency, labels):
        if label != last:
            ax.annotate(label, (x, y), xytext=(5, 8), textcoords="offset points", fontsize=8)
            last = label
    ax.set(title="Target-BER adaptive modulation and coding", xlabel="SNR (dB)",
           ylabel="Selected spectral efficiency (bit/s/Hz)")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(RESULTS / "adaptive-mcs.png", dpi=160)
    plt.close(fig)


if __name__ == "__main__":
    plot_link()
    plot_ofdm()
    plot_amc()
    plot_manet()
    print(f"plots written to {RESULTS}")
