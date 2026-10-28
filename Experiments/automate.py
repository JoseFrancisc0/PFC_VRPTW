"""Campana por tamano: CLASSIC vs QLEARNING, en paralelo.

Tamano 100: Solomon (56 instancias, campana "solomon_100"). 200 a 1000: Gehring & Homberger
(60 instancias, 10 por clase, campana "gh<size>").

Flujo: una corrida del .exe por (instancia, algoritmo, run) -> una fila por corrida en
results/master_<campana>.csv -> notebooks/analisis_single_campaign_<...>.ipynb.

Uso (desde Experiments/):
    python automate.py --size 200             # CLASSIC y QLEARNING: SOBRESCRIBE results/master_gh200.csv
    python automate.py --size 200 --solo-ql   # solo QLEARNING en results/master_gh200_ql.csv; al terminar
                                              # reemplaza sus filas en master_gh200.csv (CLASSIC no cambia)
    python automate.py --size 200 --resume    # retoma una campana cortada
    python automate.py --size 100             # Solomon: results/master_solomon_100.csv

El CSV se escribe corrida a corrida (flush inmediato), asi que una campana
cortada conserva lo hecho y se completa con --resume.
"""
import os
import re
import csv
import glob
import time
import argparse
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
_build_dir = os.path.join(BASE_DIR, "..", "build")
EXEC_PATH = os.path.join(_build_dir, "ALNS_VRPTW" if os.name != "nt" else "ALNS_VRPTW.exe")

CLASES = ["C1", "C2", "R1", "R2", "RC1", "RC2"]
ALGORITMOS = ["CLASSIC", "QLEARNING"]
ITERACIONES = 25000
RUNS = 10
MAX_WORKERS = os.cpu_count() or 4

MASTER_FIELDS = ["instance", "size", "class", "algorithm", "seed", "run",
                 "best_veh", "best_dist", "cpu_time", "valid"]
RESULT_RE = re.compile(r"^RESULT;(.*)$", re.MULTILINE)


def semilla(run):
    """Semilla = run, igual para ambos algoritmos: la corrida k de CLASSIC y la
    de QLEARNING parten del mismo stream aleatorio (comparacion pareada)."""
    return run


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


def campana(size):
    return "solomon_100" if size == 100 else f"gh{size}"


def instancias(size):
    """(clase, nombre, ruta) de cada instancia del tamano, leidas de su carpeta de benchmark."""
    benchmark = os.path.join(BASE_DIR, "..", "solomon-100" if size == 100 else f"homberger-{size}")
    lista = []
    for cls in CLASES:
        for path in sorted(glob.glob(os.path.join(benchmark, cls, "*.[tT][xX][tT]"))):
            lista.append((cls, os.path.splitext(os.path.basename(path))[0], os.path.normpath(path)))
    return lista


def cargar_hechas(master_path):
    hechas = set()
    if not os.path.exists(master_path):
        return hechas
    with open(master_path, newline="") as f:
        for row in csv.DictReader(f):
            hechas.add((row["instance"].strip().lower(), row["algorithm"], str(row["run"])))
    return hechas


def abrir_csv(path, fields, resume):
    """resume=False: sobrescribe el archivo. resume=True: agrega al final."""
    nuevo = not resume or not os.path.exists(path) or os.path.getsize(path) == 0
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    f = open(path, "w" if nuevo else "a", newline="")
    writer = csv.DictWriter(f, fieldnames=fields)
    if nuevo:
        writer.writeheader()
        f.flush()
    return f, writer


def ejecutar_corrida(job, iters, params):
    full_path, inst_name, size, cls, algo, run = job
    seed = semilla(run)
    cmd = [EXEC_PATH, full_path, algo, str(iters), str(seed)] + list(params)

    base = {"instance": inst_name, "size": size, "class": cls,
            "algorithm": algo, "seed": seed, "run": run}
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        return base, None, f"exit {e.returncode}: {e.stderr.strip()[-500:]}"

    res = parse_result_line(r.stdout)
    if res is None:
        return base, None, "sin linea RESULT en la salida"

    fila = dict(base)
    for k in ("best_veh", "best_dist", "cpu_time", "valid"):
        fila[k] = res.get(k, "")
    return base, fila, None


def maestro(size, solo_ql=False):
    return os.path.join(BASE_DIR, "results", f"master_{campana(size)}{'_ql' if solo_ql else ''}.csv")


def actualizar_maestro(size, ql_path):
    """Reemplaza las filas QLEARNING del maestro del tamano por las de ql_path (CLASSIC se conserva)."""
    master = maestro(size)
    with open(master, newline="") as f:
        filas = [r for r in csv.DictReader(f) if r["algorithm"] != "QLEARNING"]
    with open(ql_path, newline="") as f:
        filas += list(csv.DictReader(f))
    with open(master, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=MASTER_FIELDS, extrasaction="ignore")
        w.writeheader()
        w.writerows(filas)
    print(f"=== {os.path.basename(master)} actualizado con QLEARNING de {os.path.basename(ql_path)} ===")


def run_campaign(args):
    args.master = maestro(args.size, args.solo_ql)
    args.algos = ["QLEARNING"] if args.solo_ql else ALGORITMOS
    insts = instancias(args.size)

    hechas = cargar_hechas(args.master) if args.resume else set()

    jobs = []
    for cls, inst_name, full_path in insts:
        for algo in args.algos:
            for run in range(1, args.runs + 1):
                if (inst_name.lower(), algo, str(run)) not in hechas:
                    jobs.append((full_path, inst_name, args.size, cls, algo, run))

    total = len(insts) * len(args.algos) * args.runs
    print(f"=== Campana PARALELA | size={args.size} | hilos: {args.workers} | "
          f"params: {' '.join(args.params) or '(por defecto)'} ===")
    print(f"=== Instancias: {len(insts)} | corridas objetivo: {total} "
          f"| ya hechas: {len(hechas)} | pendientes: {len(jobs)} ===")
    modo = "RESUME: agrega a lo existente" if args.resume else "SOBRESCRIBE"
    print(f"=== Maestro: {args.master} ({modo}, se guarda corrida a corrida) ===")

    if args.dry_run:
        print("\n[DRY-RUN] No se ejecuta nada. Primeras corridas que se harian:")
        for j in jobs[:10]:
            print(f"   {j[4]:9s} {j[1]} run{j[5]} -> {j[0]}")
        print(f"   ... ({len(jobs)} en total)")
        return

    if not os.path.exists(EXEC_PATH):
        print(f"[ERROR] No existe el ejecutable {EXEC_PATH}. Compila primero (ver README).")
        return
    if not insts:
        print(f"[ERROR] No hay instancias del tamano {args.size} en la carpeta del benchmark.")
        return

    f, writer = abrir_csv(args.master, MASTER_FIELDS, args.resume)
    hecho = base_hechas = len(hechas)
    errores = 0
    t0 = time.time()
    executor = ThreadPoolExecutor(max_workers=args.workers)
    try:
        futuros = [executor.submit(ejecutar_corrida, j, args.iters, args.params)
                   for j in jobs]
        for futuro in as_completed(futuros):
            base, fila, error = futuro.result()
            tag = f"{base['algorithm']} {base['instance']} run{base['run']}"
            if error:
                errores += 1
                print(f"[ERROR] {tag}: {error}")
                continue
            writer.writerow(fila)
            f.flush()
            hecho += 1
            print(f"[{hecho}/{total}] {tag} -> veh={fila['best_veh']} dist={fila['best_dist']} "
                  f"cpu={fila['cpu_time']}s valid={fila['valid']}")
    except KeyboardInterrupt:
        print("\n[INTERRUPCION] Cancelando corridas pendientes; lo ya escrito se conserva.")
        executor.shutdown(wait=False, cancel_futures=True)
        raise
    finally:
        executor.shutdown(wait=True)
        f.close()

    dt = time.time() - t0
    h, rem = divmod(dt, 3600); m, s = divmod(rem, 60)
    print(f"=== Fin size={args.size}. Nuevas: {hecho - base_hechas} | errores: {errores} | "
          f"Wall-clock: {int(h)}h {int(m)}m {s:.1f}s ===")

    if args.solo_ql and hecho == total:
        actualizar_maestro(args.size, args.master)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Campana GH por tamano (ALNS / ALNS-Q), en paralelo")
    ap.add_argument("--size", type=int, required=True, choices=[100, 200, 400, 600, 800, 1000])
    ap.add_argument("--solo-ql", action="store_true",
                    help="corre solo QLEARNING en master_gh<size>_ql.csv y luego actualiza master_gh<size>.csv")
    ap.add_argument("--resume", action="store_true",
                    help="no sobrescribe: agrega solo las corridas que faltan en el maestro")
    ap.add_argument("--iters", type=int, default=ITERACIONES)
    ap.add_argument("--runs", type=int, default=RUNS)
    ap.add_argument("--workers", type=int, default=MAX_WORKERS,
                    help="corridas simultaneas (default: todos los hilos logicos)")
    ap.add_argument("--params", nargs="*", default=[], help="clave=valor, ver Utils/params.h")
    ap.add_argument("--dry-run", action="store_true", help="muestra que se correria, sin ejecutar")
    run_campaign(ap.parse_args())
