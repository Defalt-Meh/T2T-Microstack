#!/usr/bin/env python3
import argparse

def diff(a,b):
    with open(a,'rb') as fa, open(b,'rb') as fb:
        ba = fa.read()
        bb = fb.read()
    if ba == bb:
        print("IDENTICAL")
        return 0
    # find first diff
    n = min(len(ba), len(bb))
    for i in range(n):
        if ba[i] != bb[i]:
            print(f"DIFF at byte {i}: {ba[i]} != {bb[i]}")
            break
    if len(ba) != len(bb):
        print(f"DIFF length: {len(ba)} != {len(bb)}")
    return 1

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    args = ap.parse_args()
    raise SystemExit(diff(args.a, args.b))
