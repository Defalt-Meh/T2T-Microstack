#!/usr/bin/env python3
import argparse, random, csv

def gen(n, seed, out, base_px=10_000, burst_p=0.05, cancel_p=0.15, exec_p=0.10):
    rng = random.Random(seed)
    ts = 0
    next_id = 1
    live = {}  # id -> (side, px, qty)

    with open(out, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["ts_ns","type","order_id","side","px","qty"])
        for i in range(n):
            # time bursts
            step = rng.randint(50, 500) if rng.random() < burst_p else rng.randint(1, 50)
            ts += step

            # drift-y price
            base_px += rng.choice([-1,0,1])
            side = 1 if rng.random() < 0.5 else 0
            px = base_px + (rng.randint(0, 5) * (1 if side else -1))
            qty = rng.randint(1, 5)

            # add
            oid = next_id; next_id += 1
            w.writerow([ts, "A", oid, side, px, qty])
            live[oid] = (side, px, qty)

            # maybe cancel
            if live and rng.random() < cancel_p:
                cid = rng.choice(list(live.keys()))
                w.writerow([ts + rng.randint(1,20), "C", cid, live[cid][0], 0, 0])
                live.pop(cid, None)

            # maybe exec (pick an existing order id)
            if live and rng.random() < exec_p:
                eid = rng.choice(list(live.keys()))
                s, p, q = live[eid]
                ex_qty = max(1, int(q * rng.uniform(0.2, 1.0)))
                w.writerow([ts + rng.randint(1,30), "E", eid, s, p, ex_qty])
                # keep order alive (idempotent cancel later will be fine)

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--n", type=int, default=100_000)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()
    gen(args.n, args.seed, args.out)
    print(f"wrote {args.out}")
