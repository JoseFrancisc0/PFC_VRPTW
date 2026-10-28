# PFC2: ALNS vs ALNS-Q para VRPTW

### Codigo del solver (C++)
* `VRPTW Environment/`: instancia, rutas y clase `Solution`
* `Operators/`: operadores de destruccion y reparacion; `operators.cpp` define el pool compartido y el grado de destruccion
* `ALNS/`: `alns.cpp` (bucle comun en dos fases: minimizacion de rutas y luego de distancia, con SA y reinicio por estancamiento, que recalienta solo en la fase de rutas), `alns_classic.cpp` (seleccion por ruleta) y `alns_qlearning.cpp` (Q-learning: una tabla para destruccion y otra para reparacion, con el mismo ritmo que la ruleta: eligen en cada iteracion y aprenden al cerrar cada segmento de 100)
* `Utils/`: verificador de soluciones y `params.h` (parametros `clave=valor` en tiempo de ejecucion)
* `main.cpp`: `ALNS_VRPTW.exe <instancia> <CLASSIC|QLEARNING> <iters> [semilla] [clave=valor ...]`.
  `<iters>` son las iteraciones de la fase 2 (distancia); la fase 1 (rutas) usa a lo sumo otras tantas.
  Imprime `[FINAL_RESULT] ...` y `RESULT;...;cpu_time=...;valid=...` (lo lee `automate.py`).

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

Cada campana tiene un nombre: `solomon_100` (Solomon, 56 instancias) o `gh<N>` (Gehring & Homberger, 60 instancias).

| Carpeta | Contenido | Quien lo genera |
|---|---|---|
| `results/` | `master_<campana>.csv` (CLASSIC y QLEARNING) y `master_<campana>_ql.csv` (solo QLEARNING): una fila por corrida | `automate.py` (corrida a corrida) |
| `sintef/` | BKS de referencia (`sintef_<N>.csv`, `sintef_solomon_100.csv`) | a mano (entrada) |
| `summary/` | `resumen_<campana>.csv`: promedio de los runs por instancia + DIF f1 / GAP f2 vs BKS | notebooks |
| `figs/` | `<campana>/*.png` | notebooks |

Cada ejecucion **sobrescribe** sus archivos; las versiones anteriores quedan en el historial de git.

## 3. Correr una campana (desde `Experiments/`)

```
python automate.py --size 100             # Solomon      -> results/master_solomon_100.csv (1120 corridas)
python automate.py --size 200             # Homberger    -> results/master_gh200.csv       (1200 corridas)
python automate.py --size 200 --solo-ql   # solo QLEARNING -> results/master_gh200_ql.csv; al terminar
                                          # reemplaza sus filas en master_gh200.csv (CLASSIC no cambia)
```

* Si una campana se corta, lo hecho ya esta guardado: `--resume` completa lo pendiente
  (sin `--resume` se empieza de cero y se sobrescribe).
* Semilla = run, identica para ambos algoritmos (comparacion pareada).
* Instancias: todas las de `solomon-100/` o `homberger-<N>/`.
* Paralelo con `--workers` (default: todos los hilos). Vehiculos y distancia no dependen de `--workers`, pero el
  tiempo de CPU se infla con mas hilos que nucleos fisicos (10): para reportar tiempos usar `--workers 10` o menos.
* Opcionales: `--dry-run`, `--iters`, `--runs`, `--params clave=valor ...`.
* En el cluster: `bash compilar.sh` y luego `sbatch VRPTW_homberger.sh <N> [--solo-ql]`.

## 4. Analisis

Ejecutar (*Run All*) `notebooks/analisis_single_campaign_<N>.ipynb` o `notebooks/analisis_single_campaign_solomon_100.ipynb`:
regenera `summary/resumen_<campana>.csv` y `figs/<campana>/*.png`. Requiere scipy.
