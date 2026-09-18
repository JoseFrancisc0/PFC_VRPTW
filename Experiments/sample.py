import random
import argparse
import os
import csv

CLASSES = ["C1", "C2", "R1", "R2", "RC1", "RC2"]
SIZE_FACTOR = {200: 2, 400: 4, 600: 6, 800: 8, 1000: 10}
IDX_MIN, IDX_MAX = 1, 10


def sample_indices(k, seed):
    rng = random.Random(seed)
    chosen = {}
    for cls in CLASSES:
        chosen[cls] = sorted(rng.sample(range(IDX_MIN, IDX_MAX + 1), k))
    return chosen


def build_manifest(k, seed, sizes):
    chosen = sample_indices(k, seed)
    rows = []
    for cls in CLASSES:
        for size in sizes:
            factor = SIZE_FACTOR[size]
            for idx in chosen[cls]:
                fname = f"{cls}_{factor}_{idx}.TXT"
                rows.append({
                    "class": cls, "size": size, "index": idx,
                    "filename": fname,
                    "relpath": os.path.join(cls, fname),  # subcarpeta por clase
                })
    return chosen, rows


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Muestreo estratificado reproducible de GH")
    ap.add_argument("--k", type=int, default=5, help="instancias por clase (default 5)")
    ap.add_argument("--seed", type=int, default=20260901, help="semilla (DOCUMENTAR)")
    ap.add_argument("--sizes", type=int, nargs="+", default=[200, 400, 600, 800, 1000])
    ap.add_argument("--outdir", default=".", help="carpeta de salida (el nombre se autogenera con k y seed)")
    args = ap.parse_args()

    chosen, rows = build_manifest(args.k, args.seed, args.sizes)

    print(f"=== Muestreo GH | k={args.k} por clase | seed={args.seed} ===")
    print("Indices sorteados por clase (mismos en todos los tamanos):")
    for cls in CLASSES:
        print(f"  {cls:4s}: {chosen[cls]}")
    print(f"\nTotal instancias = 6 clases x {args.k} x {len(args.sizes)} tamanos = {len(rows)}")
    print(f"Por tamano = 6 x {args.k} = {6*args.k} instancias")

    out_name = f"gh_sample_manifest_k{args.k}_seed{args.seed}.csv"
    os.makedirs(args.outdir, exist_ok=True)
    out_path = os.path.join(args.outdir, out_name)

    with open(out_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["class", "size", "index", "filename", "relpath"])
        w.writeheader()
        w.writerows(rows)
    print(f"\nManifiesto escrito en: {out_path}")
    print("PRE-REGISTRA este archivo (semilla + lista) ANTES de ejecutar los experimentos.")