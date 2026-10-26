"""Prueba rapida Homberger: CLASSIC vs QLEARNING en un subconjunto de instancias y pocas semillas.

Sirve para decidir si vale la pena lanzar la campana completa (automate.py, 60 instancias x 10 corridas).
Por defecto: 4 instancias por clase (indices 1, 4, 7, 10) x 3 semillas x 2 algoritmos = 144 corridas.
Las semillas son las mismas para ambos algoritmos (corrida k = semilla k), como en automate.py.

Escribe en results/rapida_gh<size>[_<tag>].csv (NO toca master_gh<size>.csv) y al final imprime:
  - gap medio vs BKS (SINTEF) por clase y algoritmo
  - marcador pareado por instancia (flota y luego distancia, sobre las medias de las semillas),
    con el mismo criterio que analyze_results_solomon.py

Uso (desde Experiments/):
    python prueba_rapida_gh.py                         # 200 clientes, configuracion por defecto
    python prueba_rapida_gh.py --dry-run               # muestra corridas y tiempo estimado, sin ejecutar
    python prueba_rapida_gh.py --resume                # completa una prueba cortada
    python prueba_rapida_gh.py --runs 5 --indices 1 3 5 7 9
    python prueba_rapida_gh.py --classes R1 RC1 R2 RC2 # solo las clases con margen sobre el BKS
    python prueba_rapida_gh.py --tag a02 --params alpha=0.2   # variante, en otro CSV
    python prueba_rapida_gh.py --solo-analisis         # re-imprime el resumen de un CSV ya hecho

    # Variante del Q-learning reutilizando el CLASSIC ya corrido (la mitad del costo):
    python prueba_rapida_gh.py --size 400 --tag e005 --params epsilon_min=0.05 --ref results/rapida_gh400.csv
"""
import os
import csv
import math
import time
import argparse
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

from automate import (BASE_DIR, EXEC_PATH, CLASES, MASTER_FIELDS, MAX_WORKERS,
                      ITERACIONES, ALGORITMOS, ejecutar_corrida, cargar_hechas, abrir_csv)

try:
    from scipy.stats import wilcoxon
except ImportError:
    wilcoxon = None

INDICES_DEFECTO = [1, 4, 7, 10]
RUNS_DEFECTO = 3
EPS = 1e-9


# ---------------------------------------------------------------- ejecucion

def tiempo_estimado(size, jobs, workers):
    """Estimacion con los cpu_time de la campana completa previa (si existe)."""
    master = os.path.join(BASE_DIR, "results", f"master_gh{size}.csv")
    if not os.path.exists(master):
        return None
    tiempos = defaultdict(list)
    with open(master, newline="") as f:
        for row in csv.DictReader(f):
            try:
                tiempos[row["instance"]].append(float(row["cpu_time"]))
            except (KeyError, ValueError):
                pass
    if not tiempos:
        return None
    media_global = sum(sum(v) for v in tiempos.values()) / sum(len(v) for v in tiempos.values())
    total = sum((sum(tiempos[j[1]]) / len(tiempos[j[1]])) if tiempos.get(j[1]) else media_global
                for j in jobs)
    return total, total / max(1, min(workers, len(jobs)))


def ejecutar(args, csv_path):
    benchmark = args.benchmark or os.path.join(BASE_DIR, "..", f"homberger-{args.size}")
    insts = [(cls, f"{cls}_{args.size // 100}_{i}") for cls in args.classes for i in args.indices]
    hechas = cargar_hechas(csv_path) if args.resume else set()

    jobs = []
    for cls, name in insts:
        path = os.path.normpath(os.path.join(benchmark, cls, name + ".TXT"))
        for algo in args.algos:
            for run in range(1, args.runs + 1):
                if (name.lower(), algo, str(run)) not in hechas:
                    jobs.append((path, name, args.size, cls, algo, run))

    total = len(insts) * len(args.algos) * args.runs
    print(f"=== Prueba rapida GH{args.size} | {len(insts)} instancias x {len(args.algos)} algoritmos "
          f"x {args.runs} semillas = {total} corridas | pendientes: {len(jobs)} | hilos: {args.workers}")
    print(f"=== iters={args.iters} | params: {' '.join(args.params) or '(por defecto)'} | CSV: {csv_path}")
    est = tiempo_estimado(args.size, jobs, args.workers)
    if est:
        cpu, wall = est
        print(f"=== Tiempo estimado (segun master_gh{args.size}.csv): {cpu / 3600:.1f} h de CPU, "
              f"~{wall / 60:.0f} min de reloj con {args.workers} hilos")

    if args.dry_run:
        for j in jobs[:8]:
            print(f"   {j[4]:9s} {j[1]} run{j[5]} -> {j[0]}")
        print(f"   ... ({len(jobs)} en total)")
        return False

    if not os.path.exists(EXEC_PATH):
        print(f"[ERROR] No existe el ejecutable {EXEC_PATH}. Compila primero (ver README).")
        return False
    faltan = sorted({j[0] for j in jobs if not os.path.exists(j[0])})
    if faltan:
        print(f"[ERROR] {len(faltan)} archivos de instancia no existen, p.ej. {faltan[0]}")
        return False

    f, writer = abrir_csv(csv_path, MASTER_FIELDS, args.resume)
    hecho, errores, t0 = len(hechas), 0, time.time()
    executor = ThreadPoolExecutor(max_workers=args.workers)
    try:
        futuros = [executor.submit(ejecutar_corrida, j, args.iters, args.params) for j in jobs]
        for fut in as_completed(futuros):
            base, fila, error = fut.result()
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
        print("\n[INTERRUPCION] Lo ya escrito se conserva; completa con --resume.")
        executor.shutdown(wait=False, cancel_futures=True)
        raise
    finally:
        executor.shutdown(wait=True)
        f.close()

    print(f"=== Fin: errores={errores} | reloj: {(time.time() - t0) / 60:.1f} min")
    return True


# ---------------------------------------------------------------- analisis

def binom_dos_colas(k, n):
    """Prueba de signos exacta (respaldo si no hay scipy)."""
    if n == 0:
        return float("nan")
    k = min(k, n - k)
    p = sum(math.comb(n, i) for i in range(k + 1)) / 2 ** n
    return min(1.0, 2 * p)


def cargar_bks(size):
    path = os.path.join(BASE_DIR, "sintef", f"sintef_{size}.csv")
    bks = {}
    if os.path.exists(path):
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                bks[row["instance"].strip().lower()] = (float(row["BKS_Veh"]), float(row["BKS_Dist"]))
    return bks


def wilcoxon_aprox(difs):
    """Wilcoxon de rangos con signo, aproximacion normal (respaldo si no hay scipy)."""
    difs = [d for d in difs if abs(d) > EPS]
    n = len(difs)
    if n == 0:
        return float("nan")
    orden = sorted(range(n), key=lambda i: abs(difs[i]))
    rangos = [0.0] * n
    i = 0
    while i < n:  # rangos promedio en empates
        j = i
        while j + 1 < n and abs(abs(difs[orden[j + 1]]) - abs(difs[orden[i]])) <= EPS:
            j += 1
        for k in range(i, j + 1):
            rangos[orden[k]] = (i + j) / 2 + 1
        i = j + 1
    w = sum(r for r, d in zip(rangos, difs) if d > 0)
    z = (w - n * (n + 1) / 4) / math.sqrt(n * (n + 1) * (2 * n + 1) / 24)
    return math.erfc(abs(z) / math.sqrt(2))


def leer_filas(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def analizar(csv_path, size, ref_path=None):
    if not os.path.exists(csv_path):
        print(f"[ERROR] No existe {csv_path}")
        return

    filas = leer_filas(csv_path)
    if ref_path:
        # CLASSIC del CSV de referencia, solo en las instancias y semillas de esta prueba
        hechas = {(r["instance"].strip().lower(), r["run"]) for r in filas}
        ref = [r for r in leer_filas(ref_path) if r["algorithm"] == "CLASSIC"
               and (r["instance"].strip().lower(), r["run"]) in hechas]
        filas = [r for r in filas if r["algorithm"] != "CLASSIC"] + ref
        print(f"[REF] CLASSIC tomado de {os.path.basename(ref_path)}: {len(ref)} corridas")

    # medias por (instancia, algoritmo) de las corridas validas
    acc = defaultdict(list)
    clase = {}
    invalidas = 0
    for row in filas:
        if str(row.get("valid", "1")).strip() != "1":
            invalidas += 1
            continue
        key = (row["instance"].strip().lower(), row["algorithm"])
        acc[key].append((float(row["best_veh"]), float(row["best_dist"])))
        clase[key[0]] = row["class"]
    if invalidas:
        print(f"[AVISO] {invalidas} corridas con valid=0 excluidas del analisis")

    medias = {k: (sum(v for v, _ in xs) / len(xs), sum(d for _, d in xs) / len(xs), len(xs))
              for k, xs in acc.items()}
    algos = sorted({a for _, a in medias})
    bks = cargar_bks(size)

    print(f"\n--- GAP MEDIO vs BKS por clase (medias de las semillas; NV % / TD %) | {os.path.basename(csv_path)} ---")
    clases = [c for c in CLASES if c in set(clase.values())]
    print(f"{'Clase':<7}" + "".join(f"| {a:<22}" for a in algos))
    filas_total = defaultdict(list)
    for c in clases + ["Todas"]:
        linea = f"{c:<7}"
        for a in algos:
            gaps = [(100 * (m[0] - bks[i][0]) / bks[i][0], 100 * (m[1] - bks[i][1]) / bks[i][1])
                    for (i, al), m in medias.items()
                    if al == a and i in bks and (c == "Todas" or clase[i] == c)]
            if gaps:
                gv = sum(g[0] for g in gaps) / len(gaps)
                gd = sum(g[1] for g in gaps) / len(gaps)
                linea += f"| {gv:+6.2f}% / {gd:+6.2f}%     "
            else:
                linea += f"| {'(sin BKS)':<22}"
        print(linea)

    if "QLEARNING" not in algos or "CLASSIC" not in algos:
        return

    insts = sorted(i for i in clase if (i, "QLEARNING") in medias and (i, "CLASSIC") in medias)
    gana = pierde = mejor_flota = peor_flota = 0
    misma_q, misma_c, por_clase = [], [], defaultdict(lambda: [0, 0, 0])
    for i in insts:
        q, c = medias[(i, "QLEARNING")], medias[(i, "CLASSIC")]
        dv, dd = q[0] - c[0], q[1] - c[1]
        g = dv < -EPS or (abs(dv) <= EPS and dd < -EPS)
        p = dv > EPS or (abs(dv) <= EPS and dd > EPS)
        gana += g
        pierde += p
        mejor_flota += dv < -EPS
        peor_flota += dv > EPS
        por_clase[clase[i]][0 if g else (2 if p else 1)] += 1
        if abs(dv) <= EPS:
            misma_q.append(q[1])
            misma_c.append(c[1])

    print("\n--- MARCADOR PAREADO QLEARNING vs CLASSIC (por instancia; flota y luego distancia) ---")
    print(f"Instancias: {len(insts)} | Gana: {gana} | Empata: {len(insts) - gana - pierde} | Pierde: {pierde} "
          f"| Mejor flota: {mejor_flota} | Peor flota: {peor_flota}")
    print("Por clase (gana/empata/pierde): " +
          ", ".join(f"{c} {v[0]}/{v[1]}/{v[2]}" for c, v in sorted(por_clase.items())))

    if misma_q:
        mejora = [100 * (c - q) / c for q, c in zip(misma_q, misma_c)]
        print(f"Instancias con la misma flota media: {len(misma_q)} | mejora media de distancia de "
              f"QLEARNING: {sum(mejora) / len(mejora):+.3f}% (positivo = QLEARNING recorre menos)")
        difs = [q - c for q, c in zip(misma_q, misma_c) if abs(q - c) > EPS]
        if difs:
            p_w = (wilcoxon(misma_q, misma_c).pvalue if wilcoxon is not None
                   else wilcoxon_aprox(mejora))
            k = sum(d < 0 for d in difs)
            print(f"Wilcoxon pareado (dist., misma flota): p = {p_w:.4f}"
                  f"{'' if wilcoxon is not None else ' (aprox. normal, scipy no instalado)'}")
            print(f"Prueba de signos: QLEARNING menor en {k}/{len(difs)}, p = {binom_dos_colas(k, len(difs)):.4f}")

    n_sem = sorted({m[2] for m in medias.values()})
    print(f"\nNota: {len(insts)} instancias, {n_sem} semillas por instancia.", end=" ")
    if len(insts) < 60:
        print("Es una muestra: un p > 0.05 no prueba empate y un p < 0.05 conviene confirmarlo "
              "con la campana completa (automate.py).")
    else:
        print()


# ---------------------------------------------------------------- main

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Prueba rapida GH: CLASSIC vs QLEARNING en un subconjunto")
    ap.add_argument("--size", type=int, default=200, choices=[200, 400, 600, 800, 1000])
    ap.add_argument("--classes", nargs="+", default=CLASES, choices=CLASES)
    ap.add_argument("--indices", nargs="+", type=int, default=INDICES_DEFECTO,
                    help="indices 1..10 de cada clase (default: 1 4 7 10)")
    ap.add_argument("--runs", type=int, default=RUNS_DEFECTO, help="semillas por instancia y algoritmo")
    ap.add_argument("--algos", nargs="+", default=ALGORITMOS)
    ap.add_argument("--iters", type=int, default=ITERACIONES)
    ap.add_argument("--workers", type=int, default=MAX_WORKERS)
    ap.add_argument("--params", nargs="*", default=[], help="clave=valor, ver Utils/params.h")
    ap.add_argument("--tag", default="", help="sufijo del CSV, para comparar variantes sin pisarlas")
    ap.add_argument("--benchmark", default=None, help="raiz del benchmark (default: ../homberger-<size>)")
    ap.add_argument("--resume", action="store_true", help="agrega solo las corridas que faltan en el CSV")
    ap.add_argument("--dry-run", action="store_true", help="muestra que se correria y el tiempo estimado")
    ap.add_argument("--solo-analisis", action="store_true", help="no ejecuta; solo resume el CSV existente")
    ap.add_argument("--ref", default=None,
                    help="CSV del que se toma CLASSIC (p.ej. results/rapida_gh400.csv); entonces solo se corre "
                         "QLEARNING. Valido porque CLASSIC no cambia y con la misma semilla da el mismo resultado")
    args = ap.parse_args()

    nombre = f"rapida_gh{args.size}" + (f"_{args.tag}" if args.tag else "") + ".csv"
    csv_path = os.path.join(BASE_DIR, "results", nombre)

    ref_path = None
    if args.ref:
        ref_path = args.ref if os.path.isabs(args.ref) else os.path.join(BASE_DIR, args.ref)
        if not os.path.exists(ref_path):
            raise SystemExit(f"[ERROR] No existe el CSV de referencia {ref_path}")
        if os.path.abspath(ref_path) == os.path.abspath(csv_path):
            raise SystemExit("[ERROR] --ref no puede ser el mismo CSV de salida; usa --tag")
        args.algos = [a for a in args.algos if a != "CLASSIC"]

    if args.solo_analisis or ejecutar(args, csv_path):
        analizar(csv_path, args.size, ref_path)
