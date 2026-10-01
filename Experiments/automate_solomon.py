"""Campana Solomon-100: CLASSIC vs QLEARNING sobre las 56 instancias, en paralelo.

Uso (desde Experiments/, sin parametros):
    python automate_solomon.py            # SOBRESCRIBE results/solomon/master_solomon.csv
    python automate_solomon.py --resume   # retoma una campana cortada

Escribe una fila por corrida, apenas termina (y la convergencia en
master_solomon_checkpoints.csv). El resumen lo arma analyze_results_solomon.py.
"""
import os
import re
import glob
import time
import zlib
import argparse
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

from automate import (EXEC_PATH, MASTER_FIELDS, CHECKPOINT_FIELDS, CHECKPOINT_RE,
                      parse_result_line, cargar_hechas, abrir_csv)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
BENCHMARK_DIR = os.path.join(BASE_DIR, "..", "solomon-100")
MASTER = os.path.join(BASE_DIR, "results", "solomon", "master_solomon.csv")

ALGORITMOS = ["CLASSIC", "QLEARNING"]
ITERACIONES = 25000
RUNS = 10
MAX_WORKERS = os.cpu_count() or 4
CHECKPOINT_EVERY = 5000
BASE_SEED = 12345


def semilla(inst_name, run, base_seed=BASE_SEED):
    """Semilla deterministica por (instancia, corrida), IGUAL para ambos
    algoritmos: la corrida k de CLASSIC y la de QLEARNING parten del mismo
    stream aleatorio (comparacion pareada) y todo el experimento es
    reproducible."""
    return (base_seed + zlib.crc32(inst_name.encode()) + 7919 * run) % (2**31 - 1)


def obtener_instancias(clases=None):
    patron = os.path.join(BENCHMARK_DIR, "**", "*.txt")
    instancias = sorted(glob.glob(patron, recursive=True))
    if clases:
        clases = {c.upper() for c in clases}
        instancias = [i for i in instancias if os.path.basename(os.path.dirname(i)).upper() in clases]
    return instancias


def ejecutar_corrida(job, iters, params, checkpoint_every, base_seed):
    inst_path, algo, run = job
    inst_name = os.path.splitext(os.path.basename(inst_path))[0].lower()
    cls = os.path.basename(os.path.dirname(inst_path)).upper()
    seed = semilla(inst_name, run, base_seed)
    extra = list(params)
    if checkpoint_every > 0:
        extra.append(f"checkpoint_every={checkpoint_every}")
    cmd = [EXEC_PATH, inst_path, algo, str(iters), str(seed)] + extra

    base = {"instance": inst_name, "size": 100, "class": cls,
            "algorithm": algo, "seed": seed, "run": run}
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        return base, None, [], f"exit {e.returncode}: {e.stderr.strip()[-500:]}"

    res = parse_result_line(r.stdout)
    if res is None:
        return base, None, [], "sin linea RESULT en la salida"

    fila = dict(base)
    for k in ("best_veh", "best_dist", "cpu_time", "valid"):
        fila[k] = res.get(k, "")
    checkpoints = [dict(base, iter=int(i), veh=int(v), dist=float(d))
                   for i, v, d in CHECKPOINT_RE.findall(r.stdout)]
    return base, fila, checkpoints, None


def main():
    ap = argparse.ArgumentParser(description="Corre CLASSIC y QLEARNING sobre Solomon-100 con semillas pareadas.")
    ap.add_argument("--master", default=MASTER, help=f"CSV de salida (default: {os.path.relpath(MASTER, BASE_DIR)})")
    ap.add_argument("--resume", action="store_true",
                    help="no sobrescribe: agrega solo las corridas que faltan en el maestro")
    ap.add_argument("--runs", type=int, default=RUNS)
    ap.add_argument("--iters", type=int, default=ITERACIONES)
    ap.add_argument("--clases", nargs="*", help="p.ej. R1 R2 RC1 RC2 (por defecto todas)")
    ap.add_argument("--algos", nargs="*", default=ALGORITMOS)
    ap.add_argument("--params", nargs="*", default=[], help="clave=valor, ver Utils/params.h")
    ap.add_argument("--checkpoint-every", type=int, default=CHECKPOINT_EVERY)
    ap.add_argument("--workers", type=int, default=MAX_WORKERS)
    ap.add_argument("--seed", type=int, default=BASE_SEED)
    args = ap.parse_args()

    instancias = obtener_instancias(args.clases)
    if not instancias:
        print(f"[ERROR] No se encontraron instancias en {BENCHMARK_DIR}")
        return
    if not os.path.exists(EXEC_PATH):
        print(f"[ERROR] No existe el ejecutable {EXEC_PATH}. Compila primero (ver README).")
        return

    hechas = cargar_hechas(args.master) if args.resume else set()
    jobs = [(i, a, r) for i in instancias for a in args.algos for r in range(1, args.runs + 1)
            if (os.path.splitext(os.path.basename(i))[0].lower(), a, str(r)) not in hechas]
    total = len(instancias) * len(args.algos) * args.runs

    print(f"\n=== Solomon PARALELO: {len(instancias)} instancias | {total} ejecuciones | "
          f"ya hechas: {len(hechas)} | pendientes: {len(jobs)} | Hilos: {args.workers} | "
          f"params: {' '.join(args.params) or '(por defecto)'} ===")
    modo = "RESUME: agrega a lo existente" if args.resume else "SOBRESCRIBE"
    print(f"=== Maestro: {args.master} ({modo}, se guarda corrida a corrida) ===")

    # Solo el hilo principal escribe: cada corrida se guarda al terminar.
    f, writer = abrir_csv(args.master, MASTER_FIELDS, args.resume)
    fc, writer_c = (None, None)
    ckpt_path = args.master.replace(".csv", "_checkpoints.csv")
    if args.checkpoint_every > 0:
        fc, writer_c = abrir_csv(ckpt_path, CHECKPOINT_FIELDS, args.resume)
    elif not args.resume and os.path.exists(ckpt_path):
        os.remove(ckpt_path)  # no dejar convergencias de una campana anterior
    hecho = base_hechas = len(hechas)
    errores = 0
    inicio = time.time()
    executor = ThreadPoolExecutor(max_workers=args.workers)
    try:
        futuros = [executor.submit(ejecutar_corrida, j, args.iters, args.params,
                                   args.checkpoint_every, args.seed) for j in jobs]
        for futuro in as_completed(futuros):
            base, fila, checkpoints, error = futuro.result()
            tag = f"{base['algorithm']} | {base['instance'].upper()} | Run {base['run']}"
            if error:
                errores += 1
                print(f"[ERROR] {tag}: {error}")
                continue
            writer.writerow(fila)
            f.flush()
            if writer_c:
                writer_c.writerows(checkpoints)
                fc.flush()
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
        if fc:
            fc.close()

    horas, rem = divmod(time.time() - inicio, 3600)
    minutos, segundos = divmod(rem, 60)
    print(f"\n=== Terminado en {int(horas)}h {int(minutos)}m {segundos:.2f}s | "
          f"nuevas: {hecho - base_hechas} | errores: {errores} ===")
    print(f"-> Resultados en: {args.master}")
    print("-> Siguiente paso: python analyze_results_solomon.py")


if __name__ == '__main__':
    main()
