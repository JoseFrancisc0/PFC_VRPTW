import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
# Misma organizacion que Homberger: results/ = crudo por corrida (automate),
# summary/ = tablas agregadas, figs/ = graficos (ambos generados aca).
TABLAS_DIR = os.path.join(BASE_DIR, "summary", "solomon")
GRAFICOS_DIR = os.path.join(BASE_DIR, "figs", "solomon")
os.makedirs(TABLAS_DIR, exist_ok=True)
os.makedirs(GRAFICOS_DIR, exist_ok=True)

# Maestro por corrida de automate_solomon.py. Opcional: python analyze_results_solomon.py <otro_master.csv>
MASTER_CSV = sys.argv[1] if len(sys.argv) > 1 else os.path.join(BASE_DIR, "results", "solomon", "master_solomon.csv")


def resumir_master(master_csv):
    """Maestro por corrida -> una fila por (instancia, algoritmo), en el formato
    ancho que usa el resto del analisis (Avg_*, Best_*, Veh_RunK, Dist_RunK)."""
    m = pd.read_csv(master_csv)
    m = m[m["valid"] == 1]
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

# --- NUEVA TABLA FORMATO PAPER ---
df_paper = df.groupby(["Clase", "Algoritmo"]).agg({
    "Avg_Vehiculos": "mean",
    "Avg_Distancia": "mean"
}).reset_index()

df_paper_pivot = df_paper.pivot(index="Clase", columns="Algoritmo", values=["Avg_Vehiculos", "Avg_Distancia"])
df_paper_pivot.columns = [f"{col[0]}_{col[1]}" for col in df_paper_pivot.columns]
df_paper_pivot.reset_index(inplace=True)

bks_df = df.groupby("Clase").agg({"BKS_Vehiculos": "mean", "BKS_Distancia": "mean"}).reset_index()
df_paper_final = pd.merge(bks_df, df_paper_pivot, on="Clase")

cat_type = pd.CategoricalDtype(categories=["C1", "C2", "R1", "R2", "RC1", "RC2"], ordered=True)
df_paper_final["Clase"] = df_paper_final["Clase"].astype(cat_type)
df_paper_final = df_paper_final.sort_values("Clase")

def fmt_nv_td(nv, td):
    return f"{nv:.2f} / {td:.2f}"

paper_records = []
for _, row in df_paper_final.iterrows():
    bks_str = fmt_nv_td(row["BKS_Vehiculos"], row["BKS_Distancia"])
    
    classic_nv = row.get("Avg_Vehiculos_CLASSIC", row["BKS_Vehiculos"])
    classic_td = row.get("Avg_Distancia_CLASSIC", row["BKS_Distancia"])
    ql_nv = row.get("Avg_Vehiculos_QLEARNING", row["BKS_Vehiculos"])
    ql_td = row.get("Avg_Distancia_QLEARNING", row["BKS_Distancia"])
    
    classic_str = fmt_nv_td(classic_nv, classic_td)
    ql_str = fmt_nv_td(ql_nv, ql_td)
    
    gap_bks_nv = ((ql_nv - row["BKS_Vehiculos"]) / row["BKS_Vehiculos"]) * 100 if row["BKS_Vehiculos"] > 0 else 0
    gap_bks_td = ((ql_td - row["BKS_Distancia"]) / row["BKS_Distancia"]) * 100 if row["BKS_Distancia"] > 0 else 0
    
    imp_alns_nv = ((classic_nv - ql_nv) / classic_nv) * 100 if classic_nv > 0 else 0
    imp_alns_td = ((classic_td - ql_td) / classic_td) * 100 if classic_td > 0 else 0
    
    gap_str = f"{gap_bks_nv:+.2f}% / {gap_bks_td:+.2f}%"
    imp_str = f"{imp_alns_nv:+.2f}% / {imp_alns_td:+.2f}%"

    paper_records.append({
        "Benchmark / Clase": row["Clase"],
        "BKS (NV/TD)": bks_str,
        "Classical ALNS (NV/TD)": classic_str,
        "Proposed RL-ALNS (NV/TD)": ql_str,
        "Gap vs. BKS (NV/TD %)": gap_str,
        "Imp. vs. ALNS (NV/TD %)": imp_str
    })

avg_bks_nv = df_paper_final["BKS_Vehiculos"].mean()
avg_bks_td = df_paper_final["BKS_Distancia"].mean()
avg_classic_nv = df_paper_final.get("Avg_Vehiculos_CLASSIC", df_paper_final["BKS_Vehiculos"]).mean()
avg_classic_td = df_paper_final.get("Avg_Distancia_CLASSIC", df_paper_final["BKS_Distancia"]).mean()
avg_ql_nv = df_paper_final.get("Avg_Vehiculos_QLEARNING", df_paper_final["BKS_Vehiculos"]).mean()
avg_ql_td = df_paper_final.get("Avg_Distancia_QLEARNING", df_paper_final["BKS_Distancia"]).mean()

gap_bks_nv_global = ((avg_ql_nv - avg_bks_nv) / avg_bks_nv) * 100 if avg_bks_nv > 0 else 0
gap_bks_td_global = ((avg_ql_td - avg_bks_td) / avg_bks_td) * 100 if avg_bks_td > 0 else 0
imp_alns_nv_global = ((avg_classic_nv - avg_ql_nv) / avg_classic_nv) * 100 if avg_classic_nv > 0 else 0
imp_alns_td_global = ((avg_classic_td - avg_ql_td) / avg_classic_td) * 100 if avg_classic_td > 0 else 0

gap_str_global = f"{gap_bks_nv_global:+.2f}% / {gap_bks_td_global:+.2f}%"
imp_str_global = f"{imp_alns_nv_global:+.2f}% / {imp_alns_td_global:+.2f}%"

paper_records.append({
    "Benchmark / Clase": "Promedio Global",
    "BKS (NV/TD)": fmt_nv_td(avg_bks_nv, avg_bks_td),
    "Classical ALNS (NV/TD)": fmt_nv_td(avg_classic_nv, avg_classic_td),
    "Proposed RL-ALNS (NV/TD)": fmt_nv_td(avg_ql_nv, avg_ql_td),
    "Gap vs. BKS (NV/TD %)": gap_str_global,
    "Imp. vs. ALNS (NV/TD %)": imp_str_global
})

df_paper_out = pd.DataFrame(paper_records)
df_paper_out.to_csv(os.path.join(TABLAS_DIR, "tabla_paper_format.csv"), index=False)
print(f"-> Generada tabla: tabla_paper_format.csv (Formato RL-ALNS).")

print("\n--- TABLA PRINCIPAL DE RESULTADOS (FORMATO PAPER) ---")
headers = df_paper_out.columns.tolist()
# Ancho por columna calculado a partir del contenido real (encabezado o mayor
# valor), en vez de un formato fijo, que se desalinea apenas cambia un valor.
col_widths = {h: max(len(h), df_paper_out[h].astype(str).map(len).max()) for h in headers}
header_format = " | ".join(f"{{:<{col_widths[h]}}}" for h in headers)
sep_len = sum(col_widths.values()) + 3 * (len(headers) - 1)

print(header_format.format(*headers))
print("-" * sep_len)
for _, row in df_paper_out.iterrows():
    print(header_format.format(*[str(row[h]) for h in headers]))
print("-" * sep_len)

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
    sns.boxplot(data=plot_df, x="Clase", y=y_col, hue="Algoritmo", order=["C1", "C2", "R1", "R2", "RC1", "RC2"])
    plt.title(title)
    plt.ylabel(y_col)
    plt.xlabel("Clase de Instancia")
    plt.tight_layout()
    plt.savefig(os.path.join(GRAFICOS_DIR, filename), dpi=300)
    plt.close()

    
def generate_scatter(val_col, title, filename):
    df_scatter = df.pivot(index="Instancia", columns="Algoritmo", values=val_col).dropna()
    if df_scatter.empty or "CLASSIC" not in df_scatter.columns or "QLEARNING" not in df_scatter.columns:
        return
    plt.figure(figsize=(8, 8))
    sns.scatterplot(data=df_scatter, x="CLASSIC", y="QLEARNING", alpha=0.7)

    min_val = min(df_scatter["CLASSIC"].min(), df_scatter["QLEARNING"].min()) - 1
    max_val = max(df_scatter["CLASSIC"].max(), df_scatter["QLEARNING"].max()) + 1
    plt.plot([min_val, max_val], [min_val, max_val], 'r--', label="Linea y=x (Empate)")

    plt.title(title)
    plt.xlabel(f"{val_col} - CLASSIC")
    plt.ylabel(f"{val_col} - Q-LEARNING")
    plt.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(GRAFICOS_DIR, filename), dpi=300)
    plt.close()

# Barras -> Boxplots
generate_boxplot("GAP_Vehiculos(%)", "Comparacion de GAP Vehiculos Promedio por Clase de Instancia (Boxplot)", "boxplot_gap_vehiculos.png")
generate_boxplot("GAP_Distancia(%)", "Comparacion de GAP Distancia Promedio por Clase de Instancia (Boxplot)", "boxplot_gap_distancia.png")

# Dispersiones
generate_scatter("GAP_Vehiculos(%)", "Dispersion de GAP Vehiculos: ALNS vs Q-Learning", "scatter_gap_vehiculos.png")
generate_scatter("GAP_Distancia(%)", "Dispersion de GAP Distancia: ALNS vs Q-Learning", "scatter_gap_distancia.png")

print(f"\nGraficos generados exitosamente en {GRAFICOS_DIR}.")
print("=== FIN DEL ANALISIS ===")
