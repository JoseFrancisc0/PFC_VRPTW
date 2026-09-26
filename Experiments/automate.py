import os
import re
import csv
import glob
import time
import argparse
import subprocess
import threading
import zlib
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
EXEC_PATH = os.path.join(BASE_DIR, "..", "build", "ALNS_VRPTW.exe")
BENCHMARK_DIR = os.path.join(BASE_DIR, "..", "solomon-100")

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


def ejecutar_corrida(inst_path, algo, run, iters, params, checkpoint_every, base_seed=BASE_SEED):
    inst_name = os.path.basename(inst_path).replace('.txt', '').lower()
    seed = semilla(inst_name, run, base_seed)
    extra = list(params)
    if checkpoint_every > 0:
        extra.append(f"checkpoint_every={checkpoint_every}")
    comando = [EXEC_PATH, inst_path, algo, str(iters), str(seed)] + extra

    res = {"Instancia": inst_name, "Algoritmo": algo, "Run": run, "Seed": seed,
           "Veh": 0, "Dist": 0.0, "Time": 0.0, "Checkpoints": [], "Status": "ERROR"}
    t0 = time.perf_counter()
    try:
        salida = subprocess.run(comando, capture_output=True, text=True, check=True).stdout
    except subprocess.CalledProcessError as e:
        res["ErrorMsg"] = e.stderr
        return res
    res["Time"] = time.perf_counter() - t0

    match = re.search(r'\[FINAL_RESULT\] Veh: (\d+), Dist: ([\d.]+)', salida)
    if match:
        res["Veh"] = int(match.group(1))
        res["Dist"] = float(match.group(2))
        res["Status"] = "OK"
    res["Checkpoints"] = [(int(i), int(v), float(d)) for i, v, d in
                          re.findall(r'\[CHECKPOINT\] (\d+) (\d+) ([\d.]+)', salida)]
    return res


def ejecutar_lote(instancias, algoritmos, runs, iters, params=(), checkpoint_every=0,
                  workers=MAX_WORKERS, verbose=True, base_seed=BASE_SEED):
    """Ejecuta todas las corridas en paralelo y devuelve la lista de resultados."""
    tareas = [(i, a, r) for i in instancias for a in algoritmos for r in range(1, runs + 1)]
    resultados = []
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futuros = [executor.submit(ejecutar_corrida, i, a, r, iters, params, checkpoint_every, base_seed)
                   for (i, a, r) in tareas]
        for n, futuro in enumerate(as_completed(futuros), 1):
            res = futuro.result()
            resultados.append(res)
            if verbose:
                estado = "OK" if res["Status"] == "OK" else "ERROR FATAL"
                print(f"[{n}/{len(tareas)}] {res['Algoritmo']} | {res['Instancia'].upper()} | Run {res['Run']} [{estado}]")
    return resultados


def guardar_resumen(resultados, runs, csv_path):
    """CSV agregado por (instancia, algoritmo), mismo formato que antes."""
    grupos = defaultdict(list)
    for r in resultados:
        grupos[(r["Instancia"], r["Algoritmo"])].append(r)

    fieldnames = ["Instancia", "Algoritmo", "Avg_Vehiculos", "Avg_Distancia",
                  "Best_Vehiculos", "Best_Distancia", "Runs_OK", "Avg_Tiempo"]
    for i in range(1, runs + 1):
        fieldnames += [f"Veh_Run{i}", f"Dist_Run{i}"]

    with open(csv_path, mode='w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for (inst, algo) in sorted(grupos):
            ok = [r for r in grupos[(inst, algo)] if r["Status"] == "OK"]
            if not ok:
                continue
            best = min(ok, key=lambda r: (r["Veh"], r["Dist"]))
            row = {
                "Instancia": inst, "Algoritmo": algo,
                "Avg_Vehiculos": round(sum(r["Veh"] for r in ok) / len(ok), 2),
                "Avg_Distancia": round(sum(r["Dist"] for r in ok) / len(ok), 2),
                "Best_Vehiculos": best["Veh"], "Best_Distancia": round(best["Dist"], 2),
                "Runs_OK": len(ok),
                "Avg_Tiempo": round(sum(r["Time"] for r in ok) / len(ok), 2),
            }
            por_run = {r["Run"]: r for r in ok}
            for i in range(1, runs + 1):
                row[f"Veh_Run{i}"] = por_run[i]["Veh"] if i in por_run else ""
                row[f"Dist_Run{i}"] = round(por_run[i]["Dist"], 2) if i in por_run else ""
            writer.writerow(row)


def guardar_checkpoints(resultados, csv_path):
    """Formato largo: una fila por (instancia, algoritmo, run, iteracion)."""
    with open(csv_path, mode='w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["Instancia", "Algoritmo", "Run", "Seed", "Iter", "Veh", "Dist"])
        for r in sorted(resultados, key=lambda r: (r["Instancia"], r["Algoritmo"], r["Run"])):
            for it, veh, dist in r["Checkpoints"]:
                writer.writerow([r["Instancia"], r["Algoritmo"], r["Run"], r["Seed"], it, veh, round(dist, 2)])


def main():
    ap = argparse.ArgumentParser(description="Corre CLASSIC y QLEARNING sobre Solomon-100 con semillas pareadas.")
    ap.add_argument("--runs", type=int, default=RUNS)
    ap.add_argument("--iters", type=int, default=ITERACIONES)
    ap.add_argument("--clases", nargs="*", help="p.ej. R1 R2 RC1 RC2 (por defecto todas)")
    ap.add_argument("--algos", nargs="*", default=ALGORITMOS)
    ap.add_argument("--params", nargs="*", default=[], help="clave=valor, ver Utils/params.h")
    ap.add_argument("--checkpoint-every", type=int, default=CHECKPOINT_EVERY)
    ap.add_argument("--workers", type=int, default=MAX_WORKERS)
    ap.add_argument("--seed", type=int, default=BASE_SEED)
    ap.add_argument("--out", default="resultados_ejecuciones_iterativas.csv")
    args = ap.parse_args()

    instancias = obtener_instancias(args.clases)
    if not instancias:
        print(f"[ERROR] No se encontraron instancias en {BENCHMARK_DIR}")
        return

    total = len(instancias) * len(args.algos) * args.runs
    print(f"\n=== Experimentos PARALELOS: {len(instancias)} instancias | {total} ejecuciones | "
          f"Hilos: {args.workers} | params: {' '.join(args.params) or '(por defecto)'} ===")
    inicio = time.time()

    resultados = ejecutar_lote(instancias, args.algos, args.runs, args.iters, args.params,
                               args.checkpoint_every, args.workers, base_seed=args.seed)

    csv_path = os.path.join(BASE_DIR, args.out)
    guardar_resumen(resultados, args.runs, csv_path)
    if args.checkpoint_every > 0:
        guardar_checkpoints(resultados, csv_path.replace(".csv", "_checkpoints.csv"))

    errores = sum(r["Status"] != "OK" for r in resultados)
    horas, rem = divmod(time.time() - inicio, 3600)
    minutos, segundos = divmod(rem, 60)
    print(f"\n=== Terminado en {int(horas)}h {int(minutos)}m {segundos:.2f}s | errores: {errores} ===")
    print(f"-> Resultados en: {csv_path}")


if __name__ == '__main__':
    main()
