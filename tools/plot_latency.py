#!/usr/bin/env python3
import argparse, pandas as pd, matplotlib.pyplot as plt

def plot(lat_path, out_png_prefix="latency"):
    df = pd.read_csv(lat_path)
    stages = df["stage"].unique()
    for s in stages:
        d = df[df["stage"]==s]["ns"] / 1000.0  # to microseconds
        if d.empty: continue
        plt.figure()
        plt.hist(d, bins=100)
        plt.xlabel("Latency (µs)")
        plt.ylabel("Count")
        plt.title(f"{s} latency")
        png = f"{out_png_prefix}_{s}.png"
        plt.savefig(png, dpi=120, bbox_inches="tight")
        plt.close()
        print(f"wrote {png}")

    # summary
    def q(v, p): return v.quantile(p)
    summ = []
    for s in stages:
        d = (df[df["stage"]==s]["ns"] / 1000.0)
        if d.empty: continue
        summ.append([s, q(d,0.50), q(d,0.90), q(d,0.99), q(d,0.999)])
    sm = pd.DataFrame(summ, columns=["stage","p50_us","p90_us","p99_us","p999_us"])
    print(sm.to_string(index=False))

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--lat", required=True, help="latency.csv")
    ap.add_argument("--out-prefix", default="latency_hist")
    args = ap.parse_args()
    plot(args.lat, args.out_prefix)
