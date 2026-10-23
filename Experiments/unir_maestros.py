"""Reemplaza en results/master_gh<size>.csv las filas de un algoritmo por las de otro CSV.

Uso tipico (despues de correr solo QLEARNING con --master results/master_gh600_ql.csv):
    python unir_maestros.py --size 600
    -> master_gh600.csv queda con CLASSIC tal como estaba + QLEARNING de master_gh600_ql.csv

Asi los notebooks (que leen master_gh<size>.csv) funcionan sin cambios. El maestro anterior
queda en el historial de git si hiciera falta recuperarlo.
"""
import os
import csv
import argparse

from automate import BASE_DIR, MASTER_FIELDS

ap = argparse.ArgumentParser(description="Une un CSV de un solo algoritmo con el maestro del tamano")
ap.add_argument("--size", type=int, required=True)
ap.add_argument("--nuevo", default=None, help="default: results/master_gh<size>_ql.csv")
ap.add_argument("--algo", default="QLEARNING", help="algoritmo que se reemplaza")
args = ap.parse_args()

master = os.path.join(BASE_DIR, "results", f"master_gh{args.size}.csv")
nuevo = args.nuevo or os.path.join(BASE_DIR, "results", f"master_gh{args.size}_ql.csv")

with open(master, newline="") as f:
    base = list(csv.DictReader(f))
with open(nuevo, newline="") as f:
    filas_nuevas = list(csv.DictReader(f))

otros = [r for r in filas_nuevas if r["algorithm"] != args.algo]
if otros:
    raise SystemExit(f"[ERROR] {nuevo} tiene filas de otro algoritmo ({otros[0]['algorithm']}); no se une nada")

conservadas = [r for r in base if r["algorithm"] != args.algo]
quitadas = len(base) - len(conservadas)
salida = conservadas + filas_nuevas

with open(master, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=MASTER_FIELDS, extrasaction="ignore")
    w.writeheader()
    w.writerows(salida)

print(f"{os.path.basename(master)}: {quitadas} filas {args.algo} reemplazadas por {len(filas_nuevas)} "
      f"de {os.path.basename(nuevo)}; total {len(salida)} filas")
