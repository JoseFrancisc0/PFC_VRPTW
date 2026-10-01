"""Analisis de campana Gehring & Homberger (version script de
notebooks/analisis_single_campaign_{N}.ipynb, misma logica y mismas salidas).

Uso (desde Experiments/):
    python analyze_results.py              # todos los tamanos con results/master_gh<N>.csv
    python analyze_results.py --size 200   # solo uno

Lee   results/master_gh<N>.csv y sintef/sintef_<N>.csv
Crea  summary/resumen_gh<N>.csv y figs/gh<N>/*.png (sobrescribe)
"""
import os
import math
import argparse

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import seaborn as sns

try:
    from scipy.stats import wilcoxon as _scipy_wilcoxon
except ImportError:
    _scipy_wilcoxon = None
    from stats_utils import wilcoxon_signed_rank

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SIZES = [200, 400, 600, 800, 1000]

ORDEN_FAM = ['C1', 'C2', 'R1', 'R2', 'RC1', 'RC2']
C_ALNS, C_QL, C_BKS = '#e74c3c', '#2ecc71', '#7f8c8d'


def rutas(size):
    return {
        "master": os.path.join(BASE_DIR, "results", f"master_gh{size}.csv"),
        "bks": os.path.join(BASE_DIR, "sintef", f"sintef_{size}.csv"),
        "outdir": os.path.join(BASE_DIR, "figs", f"gh{size}"),
        "resumen": os.path.join(BASE_DIR, "summary", f"resumen_gh{size}.csv"),
    }


def configurar_estilo():
    sns.set_theme(style="darkgrid")
    plt.rcParams['figure.figsize'] = [12, 6]
    plt.rcParams['font.size'] = 12
    plt.rcParams['savefig.dpi'] = 300


def guardar(fig, path):
    fig.savefig(path, bbox_inches='tight')
    plt.close(fig)
    print(f"   -> {os.path.relpath(path, BASE_DIR)}")


# --- Carga y consolidacion -------------------------------------------------
def consolidar(master, size):
    df = pd.read_csv(master)
    df['instance'] = df['instance'].astype(str).str.lower().str.strip()

    n_total = len(df)
    df = df[df['valid'] == 1].copy()
    print(f"Corridas: {n_total} | validas: {len(df)} | descartadas: {n_total - len(df)}")

    df = df[df['size'] == size].copy()

    ren = {'best_veh': 'Vehiculos', 'best_dist': 'Distancia', 'cpu_time': 'Tiempo_s'}
    df_avg = (df.groupby(['instance', 'class', 'algorithm'])[['best_veh', 'best_dist', 'cpu_time']]
                .mean().reset_index())
    df_pivot = df_avg.pivot(index=['instance', 'class'], columns='algorithm',
                            values=['best_veh', 'best_dist', 'cpu_time'])
    df_pivot.columns = [f"{algo}_{ren[m]}" for m, algo in df_pivot.columns]
    df_pivot = df_pivot.reset_index().rename(columns={'class': 'Familia'})
    df_pivot['Familia'] = df_pivot['Familia'].str.upper()
    return df_pivot


# --- BKS, GAP f2 y regla de auditoria --------------------------------------
# El GAP solo es valido si el algoritmo igualo o mejoro la flota del BKS (si
# uso mas vehiculos, el GAP se anula con NaN).
def agregar_bks(df_pivot, bks_file, size):
    df_final = df_pivot.copy()
    tiene_bks = os.path.exists(bks_file)
    if tiene_bks:
        df_bks = pd.read_csv(bks_file)
        df_bks['instance'] = df_bks['instance'].astype(str).str.lower().str.strip()
        df_bks = df_bks[df_bks['size'] == size][['instance', 'BKS_Veh', 'BKS_Dist']]
        df_final = pd.merge(df_final, df_bks, on='instance', how='left')
        for A in ['CLASSIC', 'QLEARNING']:
            df_final[f'{A}_DIF_f1'] = df_final[f'{A}_Vehiculos'] - df_final['BKS_Veh']
            gap = ((df_final[f'{A}_Distancia'] - df_final['BKS_Dist']) / df_final['BKS_Dist']) * 100
            df_final[f'{A}_GAP_f2(%)'] = np.where(df_final[f'{A}_Vehiculos'] > df_final['BKS_Veh'],
                                                  np.nan, gap.clip(lower=0))
    else:
        print(f"[AVISO] Sin {bks_file}: se omiten GAP y graficos que dependen del BKS.")
    df_final['size'] = size
    return df_final, tiene_bks


# --- Graficos --------------------------------------------------------------
def exito_f1(df_final):
    df_final = df_final.copy()
    df_final['CLASSIC_Exito_f1'] = df_final['CLASSIC_Vehiculos'] <= df_final['BKS_Veh']
    df_final['QLEARNING_Exito_f1'] = df_final['QLEARNING_Vehiculos'] <= df_final['BKS_Veh']
    df_exito = df_final.groupby('Familia').agg(
        Total=('instance', 'count'),
        CLASSIC_Exitos=('CLASSIC_Exito_f1', 'sum'),
        QLEARNING_Exitos=('QLEARNING_Exito_f1', 'sum')).reset_index()
    df_exito['CLASSIC_Tasa(%)'] = (df_exito['CLASSIC_Exitos'] / df_exito['Total'] * 100).round(2)
    df_exito['QLEARNING_Tasa(%)'] = (df_exito['QLEARNING_Exitos'] / df_exito['Total'] * 100).round(2)
    return df_exito


def plot_f1_success(df_exito, size, outdir):
    fams = df_exito['Familia']; x = np.arange(len(fams)); w = 0.35
    fig, ax = plt.subplots(figsize=(12, 6))
    b1 = ax.bar(x - w/2, df_exito['CLASSIC_Tasa(%)'], w, label='ALNS', color=C_ALNS, edgecolor='black')
    b2 = ax.bar(x + w/2, df_exito['QLEARNING_Tasa(%)'], w, label='ALNS-Q', color=C_QL, edgecolor='black')
    ax.set_title(f'Fleet Minimization Success Rate ($f_1$) — GH {size}', fontweight='bold', pad=15)
    ax.set_ylabel('Success Rate (%)'); ax.set_xlabel('Instance Family')
    ax.set_xticks(x); ax.set_xticklabels(fams); ax.set_ylim(0, 125); ax.legend()
    for bars, ex, tot in [(b1, df_exito['CLASSIC_Exitos'], df_exito['Total']),
                          (b2, df_exito['QLEARNING_Exitos'], df_exito['Total'])]:
        for i, bar in enumerate(bars):
            h = bar.get_height()
            ax.annotate(f'{h:.0f}%\n({ex.iloc[i]}/{tot.iloc[i]})', (bar.get_x() + bar.get_width()/2, h),
                        xytext=(0, 4), textcoords='offset points', ha='center', fontsize=9, fontweight='bold')
    fig.tight_layout()
    guardar(fig, f"{outdir}/f1_success.png")


def plot_f1_panel(df_final, size, outdir):
    fams_var = [f for f in ORDEN_FAM if f in df_final['Familia'].unique()]
    ncols = 2; nrows = math.ceil(len(fams_var) / ncols)
    fig, axes = plt.subplots(nrows, ncols, figsize=(12, 4*nrows), constrained_layout=True)
    axes = np.array(axes).flatten()
    for ax, fam in zip(axes, fams_var):
        d = df_final[df_final['Familia'] == fam].sort_values('instance'); x = np.arange(len(d)); w = 0.25
        ax.bar(x - w, d['BKS_Veh'], w, label='BKS', color=C_BKS, edgecolor='black')
        ax.bar(x, d['CLASSIC_Vehiculos'], w, label='ALNS', color=C_ALNS, edgecolor='black')
        ax.bar(x + w, d['QLEARNING_Vehiculos'], w, label='ALNS-Q', color=C_QL, edgecolor='black')
        ax.set_title(f'Family {fam}', fontweight='bold'); ax.set_ylabel('Vehicles ($f_1$)')
        ax.set_xticks(x); ax.set_xticklabels(d['instance'].str.upper(), rotation=45, ha='right', fontsize=7)
    for ax in axes[len(fams_var):]:
        ax.set_visible(False)
    h, l = axes[0].get_legend_handles_labels()
    fig.legend(h, l, loc='lower center', ncol=3, bbox_to_anchor=(0.5, -0.03))
    fig.suptitle(f'Fleet assignment ($f_1$) per instance — GH {size}', fontweight='bold', y=1.03)
    guardar(fig, f"{outdir}/f1_panel.png")


def plot_f2_direct(df_final, size, outdir):
    """Comparativa directa, solo instancias donde ambos igualan en vehiculos."""
    dd = df_final[df_final['CLASSIC_Vehiculos'] == df_final['QLEARNING_Vehiculos']].copy()
    print(f"Instancias con flota empatada (ALNS = ALNS-Q): {len(dd)} de {len(df_final)}")
    if len(dd) == 0:
        print("Sin instancias de flota empatada para la comparativa directa.")
        return
    dd['dDist%'] = ((dd['CLASSIC_Distancia'] - dd['QLEARNING_Distancia']) / dd['CLASSIC_Distancia']) * 100
    dd = dd.sort_values('dDist%')
    y = np.arange(len(dd))
    colors = [C_QL if v > 0 else C_ALNS for v in dd['dDist%']]
    fig, ax = plt.subplots(figsize=(10, max(4, 0.32*len(dd))))
    ax.barh(y, dd['dDist%'], color=colors, edgecolor='black', linewidth=0.4)
    ax.axvline(0, color='black', linewidth=1)
    ax.set_yticks(y); ax.set_yticklabels(dd['instance'].str.upper(), fontsize=7)
    ax.set_xlabel('$\\Delta$ Distance $f_2$ (%)   ($>0$ = ALNS-Q shorter, $<0$ = ALNS shorter)')
    ax.set_title(f'Direct route-quality comparison (same fleet) — GH {size}', fontweight='bold')
    ax.legend(handles=[Patch(facecolor=C_QL, edgecolor='black', label='ALNS-Q shorter'),
                       Patch(facecolor=C_ALNS, edgecolor='black', label='ALNS shorter')],
              loc='lower right', framealpha=0.9)
    fig.tight_layout()
    guardar(fig, f"{outdir}/f2_direct.png")


def plot_f2_dumbbell(df_final, size, outdir):
    """Solo instancias donde ambos algoritmos igualaron la flota del BKS."""
    d = df_final.dropna(subset=['CLASSIC_GAP_f2(%)', 'QLEARNING_GAP_f2(%)']).copy()
    print(f"Instancias con GAP f2 valido: {len(d)} de {len(df_final)}")
    if len(d) == 0:
        print("Sin instancias validas para el dumbbell en este tamano.")
        return
    d['__fam'] = d['Familia'].apply(lambda f: ORDEN_FAM.index(f) if f in ORDEN_FAM else 99)
    d['__idx'] = d['instance'].apply(lambda s: int(s.split('_')[-1]))
    d = d.sort_values(['__fam', '__idx']).reset_index(drop=True)
    y = np.arange(len(d))
    fig, ax = plt.subplots(figsize=(10, max(4, 0.34*len(d))))
    ax.hlines(y, d['CLASSIC_GAP_f2(%)'], d['QLEARNING_GAP_f2(%)'], color='gray', alpha=0.6)
    ax.scatter(d['CLASSIC_GAP_f2(%)'], y, color=C_ALNS, label='ALNS', s=55, edgecolor='black', zorder=3)
    ax.scatter(d['QLEARNING_GAP_f2(%)'], y, color=C_QL, label='ALNS-Q', s=55, edgecolor='black', zorder=3)
    ax.set_yticks(y); ax.set_yticklabels(d['instance'].str.upper(), fontsize=7)
    for tick, (_, r) in zip(ax.get_yticklabels(), d.iterrows()):
        if r['QLEARNING_GAP_f2(%)'] < r['CLASSIC_GAP_f2(%)']:
            tick.set_color(C_QL)
        elif r['QLEARNING_GAP_f2(%)'] > r['CLASSIC_GAP_f2(%)']:
            tick.set_color(C_ALNS)
        else:
            tick.set_color('gray')
    ax.invert_yaxis()
    ax.set_xlabel('Distance GAP $f_2$ (%) vs BKS'); ax.set_title(f'Route quality GAP — GH {size}', fontweight='bold')
    ax.legend(); fig.tight_layout()
    guardar(fig, f"{outdir}/f2_dumbbell.png")


def plot_f2_parity(df_final, size, outdir):
    """Por debajo de la diagonal = ALNS-Q mejor."""
    d = df_final.dropna(subset=['CLASSIC_GAP_f2(%)', 'QLEARNING_GAP_f2(%)'])
    if len(d) == 0:
        print("Sin instancias validas para la paridad en este tamano.")
        return
    fig, ax = plt.subplots(figsize=(8, 8))
    lim = max(d['CLASSIC_GAP_f2(%)'].max(), d['QLEARNING_GAP_f2(%)'].max())*1.1 + 0.1
    ax.fill_between([0, lim], [0, 0], [0, lim], color=C_QL, alpha=0.08, zorder=0)
    ax.fill_between([0, lim], [0, lim], [lim, lim], color=C_ALNS, alpha=0.08, zorder=0)
    ax.plot([0, lim], [0, lim], '--', color='black', alpha=0.6, zorder=1)
    ax.text(lim*0.72, lim*0.18, 'ALNS-Q better\n(lower GAP)', color=C_QL,
            fontsize=11, fontweight='bold', ha='center', va='center')
    ax.text(lim*0.24, lim*0.85, 'ALNS better\n(lower GAP)', color=C_ALNS,
            fontsize=11, fontweight='bold', ha='center', va='center')
    sns.scatterplot(data=d, x='CLASSIC_GAP_f2(%)', y='QLEARNING_GAP_f2(%)', hue='Familia',
                    s=120, edgecolor='black', ax=ax, zorder=3)
    ax.set_xlim(0, lim); ax.set_ylim(0, lim)
    ax.set_xlabel('GAP ALNS (%)'); ax.set_ylabel('GAP ALNS-Q (%)')
    ax.set_title(f'GAP parity — GH {size}', fontweight='bold')
    fig.tight_layout()
    guardar(fig, f"{outdir}/f2_parity.png")


def marcador_wtl(df_final):
    """Marcador directo ALNS-Q vs ALNS (orden lexicografico)."""
    def enfrentamiento(r):
        if r['QLEARNING_Vehiculos'] < r['CLASSIC_Vehiculos']: return 'Win'
        if r['QLEARNING_Vehiculos'] > r['CLASSIC_Vehiculos']: return 'Loss'
        if r['QLEARNING_Distancia'] < r['CLASSIC_Distancia']: return 'Win'
        if r['QLEARNING_Distancia'] > r['CLASSIC_Distancia']: return 'Loss'
        return 'Tie'
    wtl = df_final.apply(enfrentamiento, axis=1).rename('WTL')
    marcador = pd.crosstab(df_final['Familia'], wtl)
    for c in ['Win', 'Tie', 'Loss']:
        if c not in marcador:
            marcador[c] = 0
    marcador = marcador[['Win', 'Tie', 'Loss']]
    marcador.loc['TOTAL'] = marcador.sum()
    return marcador


def plot_time_dist(df_final, size, outdir):
    dc = df_final[['Familia', 'CLASSIC_Tiempo_s']].rename(columns={'CLASSIC_Tiempo_s': 'Tiempo'}); dc['Algoritmo'] = 'ALNS'
    dq = df_final[['Familia', 'QLEARNING_Tiempo_s']].rename(columns={'QLEARNING_Tiempo_s': 'Tiempo'}); dq['Algoritmo'] = 'ALNS-Q'
    dbox = pd.concat([dc, dq], ignore_index=True)
    fig, ax = plt.subplots(figsize=(14, 7))
    sns.boxplot(data=dbox, x='Familia', y='Tiempo', hue='Algoritmo',
                order=[f for f in ORDEN_FAM if f in dbox['Familia'].unique()],
                palette=[C_ALNS, C_QL], ax=ax)
    ax.set_title(f'CPU time distribution per family — GH {size}', fontweight='bold')
    ax.set_ylabel('CPU time (s)'); ax.set_xlabel('Instance Family')
    fig.tight_layout()
    guardar(fig, f"{outdir}/time_dist.png")


def plot_tradeoff(df_final, size, outdir):
    """Trade-off computacional (instancias con misma flota)."""
    dt = df_final[df_final['CLASSIC_Vehiculos'] == df_final['QLEARNING_Vehiculos']].copy()
    if len(dt) == 0:
        print("Sin instancias de flota igual para el trade-off.")
        return
    dt['dTime%'] = ((dt['QLEARNING_Tiempo_s'] - dt['CLASSIC_Tiempo_s']) / dt['CLASSIC_Tiempo_s']) * 100
    dt['dDist%'] = ((dt['CLASSIC_Distancia'] - dt['QLEARNING_Distancia']) / dt['CLASSIC_Distancia']) * 100
    fig, ax = plt.subplots(figsize=(11, 9))
    xmax = max(abs(dt['dTime%']).max(), 1) * 1.15
    ymax = max(abs(dt['dDist%']).max(), 0.1) * 1.25
    ax.fill_between([-xmax, 0], [0, 0], [ymax, ymax], color=C_QL, alpha=0.10, zorder=0)
    ax.fill_between([0, xmax], [-ymax, -ymax], [0, 0], color=C_ALNS, alpha=0.10, zorder=0)
    ax.fill_between([0, xmax], [0, 0], [ymax, ymax], color='#f1c40f', alpha=0.07, zorder=0)
    ax.fill_between([-xmax, 0], [-ymax, -ymax], [0, 0], color='#f1c40f', alpha=0.07, zorder=0)
    ax.axhline(0, color='black', ls='--'); ax.axvline(0, color='black', ls='--')
    q = dict(fontsize=11, fontweight='bold', alpha=0.75, ha='center', va='center')
    ax.text(-xmax*0.5, ymax*0.88, 'DOMINANCE\n(faster, better route)', color='#1e8449', **q)
    ax.text(xmax*0.5, ymax*0.88, 'TRADE-OFF\n(slower, better route)', color='#b7950b', **q)
    ax.text(-xmax*0.5, -ymax*0.88, 'STAGNATION\n(faster, worse route)', color='#b7950b', **q)
    ax.text(xmax*0.5, -ymax*0.88, 'DEFEAT\n(slower, worse route)', color='#922b21', **q)
    sns.scatterplot(data=dt, x='dTime%', y='dDist%', hue='Familia', s=140, edgecolor='black', ax=ax, zorder=3)
    ax.set_xlim(-xmax, xmax); ax.set_ylim(-ymax, ymax)
    ax.set_xlabel('$\\Delta$ CPU Time (%)  ($<0$ = ALNS-Q faster)')
    ax.set_ylabel('$\\Delta$ Distance $f_2$ (%)  ($>0$ = ALNS-Q wins)')
    ax.set_title(f'Computational trade-off — GH {size}', fontweight='bold')
    fig.tight_layout()
    guardar(fig, f"{outdir}/tradeoff.png")


# --- Tests estadisticos ----------------------------------------------------
def wtest(a, b, label):
    try:
        if _scipy_wilcoxon is not None:
            stat, p = _scipy_wilcoxon(a, b, zero_method='wilcox')
        else:
            w_pos, w_neg, _, p = wilcoxon_signed_rank(list(a), list(b))
            stat = min(w_pos, w_neg)
        print(f"[Wilcoxon] {label}: stat={stat:.4f}, p={p:.6f} -> {'SIGNIFICATIVO' if p < 0.05 else 'no significativo'}")
    except Exception as e:
        print(f"[Wilcoxon] {label}: no aplicable ({e})")


def tests_estadisticos(df_final, tiene_bks, size):
    print(f"===== TESTS ESTADISTICOS — GH {size} =====")
    if _scipy_wilcoxon is None:
        print("[AVISO] scipy no instalado: Wilcoxon por aproximacion normal (stats_utils.py). "
              "Para los mismos p-valores que el notebook: pip install scipy")
    wtest(df_final['CLASSIC_Vehiculos'], df_final['QLEARNING_Vehiculos'], "f1 flota")
    if tiene_bks:
        dvalid = df_final.dropna(subset=['CLASSIC_GAP_f2(%)', 'QLEARNING_GAP_f2(%)'])
        if len(dvalid) > 0:
            wtest(dvalid['CLASSIC_GAP_f2(%)'], dvalid['QLEARNING_GAP_f2(%)'], f"f2 GAP (n={len(dvalid)})")
        else:
            print("[Wilcoxon] f2 GAP: sin instancias validas en este tamano")
    dsame = df_final[df_final['CLASSIC_Vehiculos'] == df_final['QLEARNING_Vehiculos']]
    if len(dsame) > 0:
        wtest(dsame['CLASSIC_Distancia'], dsame['QLEARNING_Distancia'],
              f"f2 directo (misma flota, n={len(dsame)})")
    else:
        print("[Wilcoxon] f2 directo: sin instancias de flota empatada")
    wtest(df_final['CLASSIC_Tiempo_s'], df_final['QLEARNING_Tiempo_s'], "tiempo CPU")


def analizar(size):
    p = rutas(size)
    print(f"\n{'=' * 20} GH {size} {'=' * 20}")
    os.makedirs(p["outdir"], exist_ok=True)
    os.makedirs(os.path.dirname(p["resumen"]), exist_ok=True)

    df_final, tiene_bks = agregar_bks(consolidar(p["master"], size), p["bks"], size)
    df_final.to_csv(p["resumen"], index=False)
    print(f"Resumen guardado en {os.path.relpath(p['resumen'], BASE_DIR)}  ({len(df_final)} instancias)")

    if tiene_bks:
        df_exito = exito_f1(df_final)
        print("\n--- Exito f1 por familia ---")
        print(df_exito.to_string(index=False))
        plot_f1_success(df_exito, size, p["outdir"])
        plot_f1_panel(df_final, size, p["outdir"])
    plot_f2_direct(df_final, size, p["outdir"])
    if tiene_bks:
        plot_f2_dumbbell(df_final, size, p["outdir"])
        plot_f2_parity(df_final, size, p["outdir"])
    print("\n--- Marcador ALNS-Q vs ALNS ---")
    print(marcador_wtl(df_final).to_string())
    plot_time_dist(df_final, size, p["outdir"])
    plot_tradeoff(df_final, size, p["outdir"])
    print()
    tests_estadisticos(df_final, tiene_bks, size)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Analisis GH: summary/resumen_gh<N>.csv + figs/gh<N>/")
    ap.add_argument("--size", type=int, choices=SIZES, default=None,
                    help="tamano a analizar (default: todos los que tengan results/master_gh<N>.csv)")
    args = ap.parse_args()

    sizes = [args.size] if args.size else [s for s in SIZES if os.path.exists(rutas(s)["master"])]
    if not sizes:
        print("[ERROR] No hay results/master_gh<N>.csv para analizar.")
    configurar_estilo()
    for s in sizes:
        if not os.path.exists(rutas(s)["master"]):
            print(f"[ERROR] No existe {rutas(s)['master']}")
            continue
        analizar(s)
