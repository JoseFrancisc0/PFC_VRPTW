# PFC2: ALNS vs ALNS-Q para VRPTW

### Codigo del solver (C++)
* `VRPTW Environment/`: instancia, rutas y clase `Solution`
* `Operators/`: operadores de destruccion y reparacion; `operators.cpp` define el pool compartido y el grado de destruccion
* `ALNS/`: `alns.cpp` (bucle comun en dos fases: minimizacion de rutas y luego de distancia, con SA y reinicio por estancamiento), `alns_classic.cpp` (seleccion por ruleta) y `alns_qlearning.cpp` (seleccion por Q-learning)
* `Utils/`: verificador de soluciones y `params.h` (parametros `clave=valor` en tiempo de ejecucion)
* `main.cpp`: `ALNS_VRPTW.exe <instancia> <CLASSIC|QLEARNING> <iters> [semilla] [clave=valor ...]`.
  `<iters>` son las iteraciones de la fase 2 (distancia); la fase 1 (rutas) usa a lo sumo otras tantas.
  Imprime `[FINAL_RESULT] ...` (lo lee `automate0.py`) y `RESULT;...;cpu_time=...;valid=...` (lo lee `automate.py`).

### Benchmarks
* `homberger-{200,400,600,800,1000}/`: Gehring & Homberger, subcarpetas por clase
* `solomon-100/`: Solomon, 56 instancias

## 1. Compilar

Requiere MSYS2 UCRT64 (g++) y CMake. Desde la raiz:

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Genera `build/ALNS_VRPTW.exe` (enlazado estatico, no depende del PATH). Recompilar tras cualquier cambio en `.cpp`/`.h`.

## 2. Organizacion de `Experiments/`

| Carpeta | Contenido | Quien lo genera |
|---|---|---|
| `results/` | Crudo: una fila por corrida (instancia, algoritmo, run, veh, dist, cpu, valid) | `automate.py`, `automate_solomon.py` (corrida a corrida) |
| `sintef/` | BKS de referencia | a mano (entrada) |
| `summary/` | Agregado por instancia: promedio de los runs + DIF f1 / GAP f2 vs BKS | `notebooks/analisis_single_campaign_<N>.ipynb`, `analyze_results_solomon.py` |
| `figs/` | Graficos | `notebooks/analisis_single_campaign_<N>.ipynb`, `analyze_results_solomon.py` |

Cada ejecucion **sobrescribe** sus archivos en `results/`, `summary/` y `figs/`; las versiones
anteriores quedan en el historial de git.

## 3. Homberger (experimentos PFC2, desde `Experiments/`)

```
python automate.py --size 200      # -> results/master_gh200.csv   (1200 corridas)
python automate.py --size 400
python automate.py --size 600
python automate.py --size 800
```

* Si una campana se corta, lo hecho ya esta guardado: `python automate.py --size 200 --resume` completa lo pendiente
  (sin `--resume` se empieza de cero y se sobrescribe).
* Semilla = run, identica para ambos algoritmos (comparacion pareada).
* Instancias: las 60 del tamano (6 clases x 10), listadas en el propio `automate.py`.
* Paralelo con `--workers` (default: todos los hilos). Vehiculos y distancia no dependen de `--workers`, pero el
  tiempo de CPU se infla con mas hilos que nucleos fisicos (10): para reportar tiempos usar `--workers 10` o menos.
* Opcionales: `--dry-run`, `--iters`, `--runs`, `--algos`, `--params clave=valor ...`.
* Analisis: ejecutar `notebooks/analisis_single_campaign_<N>.ipynb` (*Run All*) regenera
  `summary/resumen_gh<N>.csv` y `figs/gh<N>/*.png` de ese tamano. Requiere scipy.

## 4. Solomon-100 (desde `Experiments/`, sin parametros)

```
python automate_solomon.py           # -> results/solomon/master_solomon.csv, 1120 corridas
python analyze_results_solomon.py    # -> summary/solomon/*.csv + conclusiones_comparativas.txt, figs/solomon/*.png
```
Igual que Homberger: sobrescribe por defecto y `--resume` completa una campana cortada.
