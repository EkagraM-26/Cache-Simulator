#!/usr/bin/env python3
"""
Cache Simulator — Visualizer
Reads the CSV output from cache_sim --compare and produces publication-quality charts.

Usage:
    python3 scripts/visualize.py results/comparison.csv
    python3 scripts/visualize.py results/comparison.csv --output results/charts
"""

import argparse
import sys
import os
import csv

try:
    import matplotlib
    matplotlib.use("Agg")   # headless / file output
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import numpy as np
except ImportError:
    print("ERROR: matplotlib not installed. Run: pip install matplotlib numpy")
    sys.exit(1)


# ─── Colour palette ──────────────────────────────────────────────────────────
PALETTE = {
    "LRU":    "#4FC3F7",   # sky blue
    "FIFO":   "#FFB74D",   # amber
    "Random": "#81C784",   # sage green
}
BG   = "#1A1D23"
FG   = "#E8EAF0"
GRID = "#2E3240"

def style_ax(ax):
    ax.set_facecolor(BG)
    ax.tick_params(colors=FG, labelsize=10)
    ax.spines[:].set_color(GRID)
    ax.xaxis.label.set_color(FG)
    ax.yaxis.label.set_color(FG)
    ax.title.set_color(FG)
    ax.yaxis.grid(True, color=GRID, linewidth=0.8, linestyle="--")
    ax.set_axisbelow(True)


def load_csv(path: str) -> list[dict]:
    rows = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def plot_bar_group(ax, policies, values, ylabel, title, fmt="{:.2f}"):
    x    = np.arange(len(policies))
    bars = ax.bar(x, values, color=[PALETTE.get(p, "#AAAAAA") for p in policies],
                  width=0.55, zorder=3, edgecolor=BG, linewidth=1.5)
    ax.set_xticks(x)
    ax.set_xticklabels(policies, fontsize=11, color=FG)
    ax.set_ylabel(ylabel, color=FG, fontsize=11)
    ax.set_title(title, color=FG, fontsize=13, fontweight="bold", pad=10)
    for bar, v in zip(bars, values):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + max(values)*0.01,
                fmt.format(v), ha="center", va="bottom", color=FG, fontsize=10)
    style_ax(ax)


def make_charts(rows: list[dict], output_dir: str):
    os.makedirs(output_dir, exist_ok=True)

    policies  = [r["Policy"] for r in rows]
    l1_hit    = [float(r["L1_HitRate"]) * 100  for r in rows]
    l1_miss   = [float(r["L1_MissRate"]) * 100 for r in rows]
    l1_mpki   = [float(r["L1_MPKI"])            for r in rows]
    l2_hit    = [float(r["L2_HitRate"]) * 100   for r in rows]
    evictions = [int(r["L1_Evictions"])          for r in rows]

    plt.rcParams.update({
        "figure.facecolor": BG,
        "savefig.facecolor": BG,
        "font.family": "DejaVu Sans",
    })

    # ── Chart 1: L1 Hit Rate ──────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(8, 5))
    plot_bar_group(ax, policies, l1_hit, "L1 Hit Rate (%)", "L1 Cache Hit Rate by Policy",
                   "{:.2f}%")
    ax.set_ylim(0, max(l1_hit) * 1.15)
    fig.tight_layout()
    out = os.path.join(output_dir, "l1_hit_rate.png")
    fig.savefig(out, dpi=150)
    print(f"  Saved: {out}")
    plt.close(fig)

    # ── Chart 2: L1 Miss Rate ─────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(8, 5))
    plot_bar_group(ax, policies, l1_miss, "L1 Miss Rate (%)", "L1 Cache Miss Rate by Policy",
                   "{:.2f}%")
    ax.set_ylim(0, max(l1_miss) * 1.2)
    fig.tight_layout()
    out = os.path.join(output_dir, "l1_miss_rate.png")
    fig.savefig(out, dpi=150)
    print(f"  Saved: {out}")
    plt.close(fig)

    # ── Chart 3: MPKI ─────────────────────────────────────────────────────
    fig, ax = plt.subplots(figsize=(8, 5))
    plot_bar_group(ax, policies, l1_mpki, "Misses Per Kilo-Instruction (MPKI)",
                   "L1 MPKI — Lower is Better", "{:.2f}")
    ax.set_ylim(0, max(l1_mpki) * 1.2)
    fig.tight_layout()
    out = os.path.join(output_dir, "mpki.png")
    fig.savefig(out, dpi=150)
    print(f"  Saved: {out}")
    plt.close(fig)

    # ── Chart 4: L2 Hit Rate ──────────────────────────────────────────────
    if any(v > 0 for v in l2_hit):
        fig, ax = plt.subplots(figsize=(8, 5))
        plot_bar_group(ax, policies, l2_hit, "L2 Hit Rate (%)", "L2 Cache Hit Rate by Policy",
                       "{:.2f}%")
        ax.set_ylim(0, max(l2_hit) * 1.15)
        fig.tight_layout()
        out = os.path.join(output_dir, "l2_hit_rate.png")
        fig.savefig(out, dpi=150)
        print(f"  Saved: {out}")
        plt.close(fig)

    # ── Chart 5: Summary — 4-panel overview ──────────────────────────────
    fig, axes = plt.subplots(2, 2, figsize=(14, 9))
    fig.suptitle("Cache Replacement Policy Comparison", color=FG,
                 fontsize=16, fontweight="bold", y=1.01)

    data_panels = [
        (axes[0, 0], l1_hit,    "L1 Hit Rate (%)",         "Hit Rate",  "{:.2f}%"),
        (axes[0, 1], l1_mpki,   "MPKI (Misses/1K inst.)",  "MPKI",      "{:.2f}"),
        (axes[1, 0], l1_miss,   "L1 Miss Rate (%)",         "Miss Rate", "{:.2f}%"),
        (axes[1, 1], evictions, "L1 Evictions",             "Evictions", "{:,.0f}"),
    ]

    for ax, vals, ylabel, title, fmt in data_panels:
        x    = np.arange(len(policies))
        bars = ax.bar(x, vals, color=[PALETTE.get(p, "#AAA") for p in policies],
                      width=0.55, zorder=3, edgecolor=BG, linewidth=1.5)
        ax.set_xticks(x)
        ax.set_xticklabels(policies, fontsize=10, color=FG)
        ax.set_ylabel(ylabel, color=FG, fontsize=10)
        ax.set_title(title, color=FG, fontsize=11, fontweight="bold")
        for bar, v in zip(bars, vals):
            ax.text(bar.get_x() + bar.get_width()/2,
                    bar.get_height() + max(vals)*0.01 if max(vals) > 0 else 0.001,
                    fmt.format(v), ha="center", va="bottom", color=FG, fontsize=9)
        style_ax(ax)
        ax.set_ylim(0, max(vals)*1.2 if max(vals) > 0 else 1)

    # Legend
    patches = [mpatches.Patch(color=v, label=k) for k, v in PALETTE.items()]
    fig.legend(handles=patches, loc="lower center", ncol=3,
               facecolor=BG, edgecolor=GRID, labelcolor=FG, fontsize=11,
               bbox_to_anchor=(0.5, -0.04))

    fig.tight_layout()
    out = os.path.join(output_dir, "summary_overview.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"  Saved: {out}")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description="Cache simulator chart generator")
    parser.add_argument("csv", help="CSV file from cache_sim --compare")
    parser.add_argument("--output", default="results", help="Output directory")
    args = parser.parse_args()

    if not os.path.isfile(args.csv):
        print(f"ERROR: File not found: {args.csv}")
        sys.exit(1)

    print(f"\n  Cache Simulator — Chart Generator")
    print(f"  Input  : {args.csv}")
    print(f"  Output : {args.output}/\n")

    rows = load_csv(args.csv)
    make_charts(rows, args.output)
    print("\n  All charts generated successfully.")


if __name__ == "__main__":
    main()
