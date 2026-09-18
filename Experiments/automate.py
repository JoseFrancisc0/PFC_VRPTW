import os
import re
import csv
import time
import argparse
import subprocess

EXEC_PATH = "../build/ALNS_VRPTW.exe"

MASTER_FIELDS = ["instance", "size", "class", "algorithm", "seed", "run",
                 "best_veh", "best_dist", "cpu_time", "valid"]
RESULT_RE = re.compile(r"^RESULT;(.*)$", re.MULTILINE)


def parse_result_line(stdout):
    matches = RESULT_RE.findall(stdout)
    if not matches:
        return None
    fields = {}
    for kv in matches[-1].split(";"):
        if "=" in kv:
            k, v = kv.split("=", 1)
            fields[k] = v
    return fields


def cargar_manifiesto(manifest_path, size_filter=None):
    rows = []
    with open(manifest_path, newline="") as f:
        for row in csv.DictReader(f):
            if size_filter is None or int(row["size"]) == size_filter:
                rows.append(row)
    return rows


def cargar_hechas(master_path):
    hechas = set()
    if not os.path.exists(master_path):
        return hechas
    with open(master_path, newline="") as f:
        for row in csv.DictReader(f):
            hechas.add((row["instance"].strip().lower(), row["algorithm"], str(row["run"])))
    return hechas


def abrir_maestro_append(master_path):
    nuevo = not os.path.exists(master_path) or os.path.getsize(master_path) == 0
    os.makedirs(os.path.dirname(master_path) or ".", exist_ok=True)
    f = open(master_path, "a", newline="")
    writer = csv.DictWriter(f, fieldnames=MASTER_FIELDS)
    if nuevo:
        writer.writeheader()
        f.flush()
    return f, writer


def run_campaign(args):
    manifest_rows = cargar_manifiesto(args.manifest, args.size)
    if not manifest_rows:
        print(f"[ERROR] El manifiesto no tiene instancias" +
              (f" para size={args.size}" if args.size else "") + ".")
        return

    hechas = cargar_hechas(args.master)

    jobs = []
    for row in manifest_rows:
        inst_name = os.path.splitext(row["filename"])[0]
        full_path = os.path.join(args.benchmark, row["relpath"])
        for algo in args.algos:
            for run in range(1, args.runs + 1):
                if (inst_name.lower(), algo, str(run)) not in hechas:
                    jobs.append((full_path, inst_name, row["size"], row["class"], algo, run))

    total = len(manifest_rows) * len(args.algos) * args.runs
    fase = f"FASE size={args.size}" if args.size else "TODAS las tallas"
    print(f"=== Campana SECUENCIAL | {fase} ===")
    print(f"=== Instancias en fase: {len(manifest_rows)} | corridas objetivo: {total} "
          f"| ya hechas: {len(hechas)} | pendientes: {len(jobs)} ===")
    print(f"=== Maestro: {args.master} (incremental + resume) ===")

    if args.dry_run:
        print("\n[DRY-RUN] No se ejecuta nada. Primeras corridas que se harian:")
        for j in jobs[:10]:
            print(f"   {j[4]:9s} {j[1]} run{j[5]}")
        print(f"   ... ({len(jobs)} en total)")
        return

    faltan = sorted({j[0] for j in jobs if not os.path.exists(j[0])})
    if faltan:
        print(f"[ERROR] {len(faltan)} archivos de instancia no existen. Ejemplos:")
        for p in faltan[:5]:
            print(f"   {p}")
        print("Revisa --benchmark y la estructura de carpetas. Abortando.")
        return

    f, writer = abrir_maestro_append(args.master)
    hecho = len(hechas)
    base = len(hechas)
    t0 = time.time()
    try:
        for (full_path, inst_name, size, cls, algo, run) in jobs:
            cmd = [EXEC_PATH, full_path, algo, str(args.iters), str(run)]
            try:
                r = subprocess.run(cmd, capture_output=True, text=True, check=True)
            except subprocess.CalledProcessError as e:
                print(f"[ERROR] {algo} {inst_name} run{run}\n{e.stderr}")
                continue
            res = parse_result_line(r.stdout)
            if res is None:
                print(f"[WARN] sin RESULT: {algo} {inst_name} run{run}")
                continue

            fila = {k: res.get(k, "") for k in MASTER_FIELDS}
            fila["instance"] = inst_name
            fila["size"] = size
            fila["class"] = cls
            writer.writerow(fila)
            f.flush()
            hecho += 1
            print(f"[{hecho}/{total}] {algo} {inst_name} run{run} "
                  f"-> veh={res.get('best_veh')} dist={res.get('best_dist')} "
                  f"cpu={res.get('cpu_time')}s valid={res.get('valid')}")
    finally:
        f.close()

    dt = time.time() - t0
    h, rem = divmod(dt, 3600); m, s = divmod(rem, 60)
    print(f"=== Fin {fase}. Nuevas: {hecho - base}. "
          f"Wall-clock: {int(h)}h {int(m)}m {s:.1f}s ===")


def run_trace(args):
    seed_arg = str(args.seed) if args.seed is not None else str(args.run)
    cmd = [EXEC_PATH, args.instance, args.algo, str(args.iters), str(args.run), seed_arg, "--trace"]
    print("Regenerando traza:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    print("Traza escrita en ../Results/<algo>/metrics/")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Campana secuencial por fases (ALNS / ALNS-Q)")
    sub = ap.add_subparsers(dest="modo", required=True)

    pc = sub.add_parser("campaign", help="Corre una FASE (por tamano) secuencialmente")
    pc.add_argument("--manifest", required=True, help="CSV de instancias muestreadas")
    pc.add_argument("--benchmark", required=True, help="raiz del benchmark (contiene subcarpetas de clase)")
    pc.add_argument("--master", required=True, help="CSV maestro de salida (sugerido: uno por fase)")
    pc.add_argument("--size", type=int, default=None, help="filtra la fase: 200/400/600/800/1000")
    pc.add_argument("--algos", nargs="+", default=["CLASSIC", "QLEARNING"])
    pc.add_argument("--iters", type=int, default=25000)
    pc.add_argument("--runs", type=int, default=10)
    pc.add_argument("--dry-run", action="store_true", help="muestra que se correria, sin ejecutar")
    pc.set_defaults(func=run_campaign)

    pr = sub.add_parser("trace", help="Regenera la traza de una corrida (visualizer)")
    pr.add_argument("--instance", required=True)
    pr.add_argument("--algo", required=True, choices=["CLASSIC", "QLEARNING"])
    pr.add_argument("--iters", type=int, default=25000)
    pr.add_argument("--run", type=int, required=True)
    pr.add_argument("--seed", type=int, default=None)
    pr.set_defaults(func=run_trace)

    args = ap.parse_args()
    args.func(args)