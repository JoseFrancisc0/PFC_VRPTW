import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

try:
    from scipy.stats import wilcoxon
except ImportError:
    wilcoxon = None

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
# Misma organizacion que Homberger: results/ = crudo por corrida (automate),
# summary/ = tablas agregadas, figs/ = graficos (ambos generados aca).
TABLAS_DIR = os.path.join(BASE_DIR, "summary", "solomon")
GRAFICOS_DIR = os.path.join(BASE_DIR, "figs", "solomon")
os.makedirs(TABLAS_DIR, exist_ok=True)
os.makedirs(GRAFICOS_DIR, exist_ok=True)

# Maestro por corrida de automate_solomon.py. Opcional: python analyze_results_solomon.py <otro_master.csv>
MASTER_CSV = sys.argv[1] if len(sys.argv) > 1 else os.path.join(BASE_DIR, "results", "solomon", "master_solomon.csv")

# Orden y nombre de cada algoritmo en tablas y graficos; solo se usan los presentes en el maestro
ETIQUETAS = {"CLASSIC": "Classical ALNS", "QLEARNING": "Proposed RL-ALNS"}
ORDEN_CLASES = ["C1", "C2", "R1", "R2", "RC1", "RC2"]


def resumir_master(master_csv):
    """Maestro por corrida -> una fila por (instancia, algoritmo), en el formato
    ancho que usa el resto del analisis (Avg_*, Best_*, Veh_RunK, Dist_RunK)."""
    m = pd.read_csv(master_csv)
    m = m[(m["valid"] == 1) & m["algorithm"].isin(ETIQUETAS)]
    filas = []
    for (inst, algo), g in m.groupby(["instance", "algorithm"]):
        best = g.sort_values(["best_veh", "best_dist"]).iloc[0]
        row = {"Instancia": inst, "Algoritmo": algo,
               "Avg_Vehiculos": round(g["best_veh"].mean(), 2),
               "Avg_Distancia": round(g["best_dist"].mean(), 2),
               "Best_Vehiculos": int(best["best_veh"]), "Best_Distancia": round(best["best_dist"], 2),
               "Runs_OK": len(g),
               "Avg_Tiempo": round(g["cpu_time"].mean(), 2)}
        for _, r in g.sort_values("run").iterrows():
            row[f"Veh_Run{int(r['run'])}"] = int(r["best_veh"])
            row[f"Dist_Run{int(r['run'])}"] = round(r["best_dist"], 2)
        filas.append(row)
    return pd.DataFrame(filas)

class Logger:
    def __init__(self, filename):
        self.terminal = sys.stdout
        self.log = open(filename, "w", encoding='utf-8')
    def write(self, message):
        self.terminal.write(message)
        self.log.write(message)
    def flush(self):
        self.terminal.flush()
        self.log.flush()

sys.stdout = Logger(os.path.join(TABLAS_DIR, "conclusiones_comparativas.txt"))

print("=== INICIO DEL ANALISIS DE RESULTADOS ===")
df_res = resumir_master(MASTER_CSV)
df_res.to_csv(os.path.join(TABLAS_DIR, "resumen_solomon.csv"), index=False)
print(f"-> Leido {MASTER_CSV}; generada tabla: resumen_solomon.csv con {len(df_res)} registros.")
df_sintef = pd.read_csv(os.path.join(BASE_DIR, "sintef", "sintef_solomon.csv"), sep=";", encoding="latin1")

df_sintef.columns = ["Instancia", "BKS_Vehiculos", "BKS_Distancia"]
df_sintef["Instancia"] = df_sintef["Instancia"].str.lower()
df_res["Instancia"] = df_res["Instancia"].str.lower()

df = pd.merge(df_res, df_sintef, on="Instancia", how="inner")

if len(df) == 0:
    print("Error: No se pudieron cruzar las instancias.")
    sys.exit(1)

ALGOS = [a for a in ETIQUETAS if a in set(df["Algoritmo"])]
df = df[df["Algoritmo"].isin(ALGOS)].copy()
print(f"-> Algoritmos en el maestro: {', '.join(ALGOS)}")

def get_class(inst):
    if inst.startswith('rc'): return inst[:3].upper()
    return inst[:2].upper()

df["Clase"] = df["Instancia"].apply(get_class)

df["GAP_Vehiculos(%)"] = ((df["Avg_Vehiculos"] - df["BKS_Vehiculos"]) / df["BKS_Vehiculos"]) * 100
df["GAP_Distancia(%)"] = ((df["Avg_Distancia"] - df["BKS_Distancia"]) / df["BKS_Distancia"]) * 100

cols_instancia = ["Instancia", "Clase", "Algoritmo", "Avg_Vehiculos", "Avg_Distancia",
                  "BKS_Vehiculos", "BKS_Distancia", "GAP_Vehiculos(%)", "GAP_Distancia(%)"]
df_instancia = df[cols_instancia].copy()
df_instancia.to_csv(os.path.join(TABLAS_DIR, "gap_por_instancia.csv"), index=False)
print(f"-> Generada tabla: gap_por_instancia.csv con {len(df_instancia)} registros.")

# --- TABLA FORMATO PAPER ---
df_paper = df.groupby(["Clase", "Algoritmo"]).agg({
    "Avg_Vehiculos": "mean",
    "Avg_Distancia": "mean"
}).reset_index()

df_paper_pivot = df_paper.pivot(index="Clase", columns="Algoritmo", values=["Avg_Vehiculos", "Avg_Distancia"])
df_paper_pivot.columns = [f"{col[0]}_{col[1]}" for col in df_paper_pivot.columns]
df_paper_pivot.reset_index(inplace=True)

bks_df = df.groupby("Clase").agg({"BKS_Vehiculos": "mean", "BKS_Distancia": "mean"}).reset_index()
df_paper_final = pd.merge(bks_df, df_paper_pivot, on="Clase")

cat_type = pd.CategoricalDtype(categories=ORDEN_CLASES, ordered=True)
df_paper_final["Clase"] = df_paper_final["Clase"].astype(cat_type)
df_paper_final = df_paper_final.sort_values("Clase")

def fmt_nv_td(nv, td):
    return f"{nv:.2f} / {td:.2f}"

def fmt_pct(nv, td):
    return f"{nv:+.2f}% / {td:+.2f}%"

def mejora(ref, new):
    """Mejora relativa (%) de new sobre ref; positiva si new es menor."""
    return ((ref - new) / ref) * 100 if ref > 0 else 0

def fila_paper(clase, nv_td):
    """nv_td: algoritmo -> (NV, TD), con 'BKS' incluido."""
    bks_nv, bks_td = nv_td["BKS"]
    rec = {"Benchmark / Clase": clase, "BKS (NV/TD)": fmt_nv_td(bks_nv, bks_td)}
    for a in ALGOS:
        rec[f"{ETIQUETAS[a]} (NV/TD)"] = fmt_nv_td(*nv_td[a])
    if "QLEARNING" in ALGOS:
        ql_nv, ql_td = nv_td["QLEARNING"]
        rec["Gap vs. BKS (NV/TD %)"] = fmt_pct((ql_nv - bks_nv) / bks_nv * 100, (ql_td - bks_td) / bks_td * 100)
        if "CLASSIC" in ALGOS:
            cl_nv, cl_td = nv_td["CLASSIC"]
            rec["Imp. vs. ALNS (NV/TD %)"] = fmt_pct(mejora(cl_nv, ql_nv), mejora(cl_td, ql_td))
    return rec

paper_records = []
for _, row in df_paper_final.iterrows():
    nv_td = {"BKS": (row["BKS_Vehiculos"], row["BKS_Distancia"])}
    for a in ALGOS:
        nv_td[a] = (row[f"Avg_Vehiculos_{a}"], row[f"Avg_Distancia_{a}"])
    paper_records.append(fila_paper(row["Clase"], nv_td))

nv_td_global = {"BKS": (df_paper_final["BKS_Vehiculos"].mean(), df_paper_final["BKS_Distancia"].mean())}
for a in ALGOS:
    nv_td_global[a] = (df_paper_final[f"Avg_Vehiculos_{a}"].mean(), df_paper_final[f"Avg_Distancia_{a}"].mean())
paper_records.append(fila_paper("Promedio Global", nv_td_global))

df_paper_out = pd.DataFrame(paper_records)
df_paper_out.to_csv(os.path.join(TABLAS_DIR, "tabla_paper_format.csv"), index=False)
print(f"-> Generada tabla: tabla_paper_format.csv (Formato RL-ALNS).")

def imprimir_tabla(tabla):
    headers = tabla.columns.tolist()
    # Ancho por columna calculado a partir del contenido real (encabezado o mayor
    # valor), en vez de un formato fijo, que se desalinea apenas cambia un valor.
    col_widths = {h: max(len(h), tabla[h].astype(str).map(len).max()) for h in headers}
    header_format = " | ".join(f"{{:<{col_widths[h]}}}" for h in headers)
    sep_len = sum(col_widths.values()) + 3 * (len(headers) - 1)

    print(header_format.format(*headers))
    print("-" * sep_len)
    for _, row in tabla.iterrows():
        print(header_format.format(*[str(row[h]) for h in headers]))
    print("-" * sep_len)

print("\n--- TABLA PRINCIPAL DE RESULTADOS (FORMATO PAPER) ---")
imprimir_tabla(df_paper_out)

# --- MARCADOR PAREADO ENTRE ALGORITMOS ---
# Por instancia, sobre las medias de las corridas: primero flota, luego distancia
PARES = [("QLEARNING", "CLASSIC")] if set(ETIQUETAS) <= set(ALGOS) else []

def marcador(a, b):
    p = df.pivot(index="Instancia", columns="Algoritmo", values=["Avg_Vehiculos", "Avg_Distancia"]).dropna()
    dv = p[("Avg_Vehiculos", a)] - p[("Avg_Vehiculos", b)]
    dd = p[("Avg_Distancia", a)] - p[("Avg_Distancia", b)]
    gana = (dv < -1e-9) | ((dv.abs() <= 1e-9) & (dd < -1e-9))
    pierde = (dv > 1e-9) | ((dv.abs() <= 1e-9) & (dd > 1e-9))
    misma = dv.abs() <= 1e-9
    rec = {"Par": f"{a} vs {b}", "Instancias": len(p),
           "Gana": int(gana.sum()), "Empata": int((~gana & ~pierde).sum()), "Pierde": int(pierde.sum()),
           "Mejor flota": int((dv < -1e-9).sum()), "Peor flota": int((dv > 1e-9).sum()),
           "Mejora dist. misma flota (%)": round(float((-dd[misma] / p[("Avg_Distancia", b)][misma]).mean() * 100), 3)
                                          if misma.any() else float("nan")}
    # Wilcoxon pareado de la distancia media en las instancias con la misma flota media
    if wilcoxon is not None and misma.sum() > 0 and (dd[misma].abs() > 1e-9).any():
        rec["Wilcoxon p (dist.)"] = round(float(wilcoxon(p[("Avg_Distancia", a)][misma],
                                                         p[("Avg_Distancia", b)][misma]).pvalue), 4)
    else:
        rec["Wilcoxon p (dist.)"] = "n/a"
    return rec

if PARES:
    df_marcador = pd.DataFrame([marcador(a, b) for a, b in PARES])
    df_marcador.to_csv(os.path.join(TABLAS_DIR, "marcador_pareado.csv"), index=False)
    print("\n--- MARCADOR PAREADO POR INSTANCIA (medias; flota y luego distancia) ---")
    imprimir_tabla(df_marcador)
    if wilcoxon is None:
        print("(scipy no instalado: se omite el test de Wilcoxon)")

# --- GRAFICOS ---
sns.set_theme(style="whitegrid")

# Corridas individuales (el numero de runs se detecta de las columnas)
run_ids = sorted(int(c[len("Veh_Run"):]) for c in df.columns if c.startswith("Veh_Run"))
runs_data = []
for _, row in df.iterrows():
    inst = row["Instancia"]
    algo = row["Algoritmo"]
    clase = row["Clase"]
    bks_veh = row["BKS_Vehiculos"]
    bks_dist = row["BKS_Distancia"]

    for i in run_ids:
        col_veh = f"Veh_Run{i}"
        col_dist = f"Dist_Run{i}"
        if col_veh in row and pd.notna(row[col_veh]) and row[col_veh] != "":
            veh = float(row[col_veh])
            dist = float(row[col_dist])
            gap_veh = ((veh - bks_veh) / bks_veh) * 100 if bks_veh > 0 else 0
            gap_dist = ((dist - bks_dist) / bks_dist) * 100 if bks_dist > 0 else 0

            runs_data.append({
                "Instancia": inst,
                "Algoritmo": algo,
                "Clase": clase,
                "Run": i,
                "Veh": veh,
                "Dist": dist,
                "GAP_Vehiculos(%)": gap_veh,
                "GAP_Distancia(%)": gap_dist
            })

df_melted = pd.DataFrame(runs_data)

def generate_boxplot(y_col, title, filename):
    plt.figure(figsize=(10, 6))
    plot_df = df_melted if not df_melted.empty else df
    sns.boxplot(data=plot_df, x="Clase", y=y_col, hue="Algoritmo", order=ORDEN_CLASES, hue_order=ALGOS)
    plt.title(title)
    plt.ylabel(y_col)
    plt.xlabel("Clase de Instancia")
    plt.tight_layout()
    plt.savefig(os.path.join(GRAFICOS_DIR, filename), dpi=300)
    plt.close()


def generate_scatter(val_col, x_algo, y_algo, title, filename):
    df_scatter = df.pivot(index="Instancia", columns="Algoritmo", values=val_col).dropna()
    if df_scatter.empty or x_algo not in df_scatter.columns or y_algo not in df_scatter.columns:
        return
    plt.figure(figsize=(8, 8))
    sns.scatterplot(data=df_scatter, x=x_algo, y=y_algo, alpha=0.7)

    min_val = min(df_scatter[x_algo].min(), df_scatter[y_algo].min()) - 1
    max_val = max(df_scatter[x_algo].max(), df_scatter[y_algo].max()) + 1
    plt.plot([min_val, max_val], [min_val, max_val], 'r--', label="Linea y=x (Empate)")

    plt.title(title)
    plt.xlabel(f"{val_col} - {x_algo}")
    plt.ylabel(f"{val_col} - {y_algo}")
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(GRAFICOS_DIR, filename), dpi=300)
    plt.close()

# Barras -> Boxplots
generate_boxplot("GAP_Vehiculos(%)", "Comparacion de GAP Vehiculos Promedio por Clase de Instancia (Boxplot)", "boxplot_gap_vehiculos.png")
generate_boxplot("GAP_Distancia(%)", "Comparacion de GAP Distancia Promedio por Clase de Instancia (Boxplot)", "boxplot_gap_distancia.png")

# Dispersiones
generate_scatter("GAP_Vehiculos(%)", "CLASSIC", "QLEARNING", "Dispersion de GAP Vehiculos: ALNS vs Q-Learning", "scatter_gap_vehiculos.png")
generate_scatter("GAP_Distancia(%)", "CLASSIC", "QLEARNING", "Dispersion de GAP Distancia: ALNS vs Q-Learning", "scatter_gap_distancia.png")

print(f"\nGraficos generados exitosamente en {GRAFICOS_DIR}.")
print("=== FIN DEL ANALISIS ===")
