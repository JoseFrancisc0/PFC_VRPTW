#!/bin/bash
#SBATCH --job-name=vrptw_gh
#SBATCH --partition=standard       # Partición de nodos CPU
#SBATCH --nodes=1                  # 1 nodo para memoria compartida en Python
#SBATCH --ntasks=1                 # 1 proceso principal
#SBATCH --cpus-per-task=32         # Máximo permitido por tu cuenta (cpu=32)
#SBATCH --mem=64G                  # Memoria holgada para 32 workers concurrentes
#SBATCH --time=08:00:00            # Límite máximo de tu cuenta (8 horas)
#SBATCH --output=vrptw_%j.out      # Salida estándar (prints de avance de automate.py)
#SBATCH --error=vrptw_%j.err       # Errores y tracebacks de Python / C++

# Uso (desde Experiments/):
#   sbatch VRPTW_homberger.sh                 # size=800, todo como antes
#   sbatch VRPTW_homberger.sh 600             # otro tamaño
#   sbatch VRPTW_homberger.sh 600 --algos QLEARNING --master results/master_gh600_ql.csv
# Todo lo que va despues del tamaño se pasa tal cual a automate.py.
#
# Antes de la campaña SIEMPRE recompila el ejecutable a partir del codigo actual del repositorio
# (build/ no esta en git: sin esto se usaba el binario de la ultima compilacion en este equipo).
# Si la compilacion falla, el job termina sin correr nada.

SIZE="${1:-800}"
shift || true

# 1. Asegurar posición en la carpeta donde reside el script (Experiments/)
cd "${SLURM_SUBMIT_DIR:-.}" || exit 1
[ -f automate.py ] || { echo "[ERROR] Lanza sbatch desde Experiments/ (no se encontro automate.py en $PWD)"; exit 1; }
REPO_DIR="$(cd .. && pwd)"
BUILD_DIR="$REPO_DIR/build"
EXE="$BUILD_DIR/ALNS_VRPTW"

# 2. Cargar módulos necesarios
module purge
module load gnu12/12.4.0
module load python3/3.11.11

# Desde aqui cualquier error termina el job (despues de module: Lmod no tolera set -e/-u)
set -eo pipefail

echo "============================================================"
echo "Job ID:            $SLURM_JOB_ID"
echo "Nodo asignado:     $SLURMD_NODENAME"
echo "Workers asignados: $SLURM_CPUS_PER_TASK"
echo "Tamaño:            $SIZE"
echo "Args extra:        ${*:-(ninguno)}"
if git -C "$REPO_DIR" rev-parse --git-dir > /dev/null 2>&1; then
    echo "Commit:            $(git -C "$REPO_DIR" log -1 --format='%h %s')"
    if [ -n "$(git -C "$REPO_DIR" status --porcelain -- '*.cpp' '*.h')" ]; then
        echo "AVISO: hay cambios sin commit en .cpp/.h; se compilan tal como estan en disco"
    fi
fi
echo "============================================================"

# 3. Recompilar. Mismas fuentes y -O3 que CMakeLists.txt; se compila con g++ directo para no
#    depender de un modulo de cmake ni de un CMakeCache viejo. Se escribe a un temporal y luego
#    se reemplaza, y flock evita que dos jobs lanzados a la vez compilen encima del otro.
compilar() {
    cd "$REPO_DIR"
    local tmp_exe="$EXE.tmp.${SLURM_JOB_ID:-$$}"
    echo "[BUILD] Compilando con $(g++ --version | head -1) ..."
    g++ -O3 -std=c++17 -o "$tmp_exe" \
        main.cpp "VRPTW Environment"/*.cpp Operators/*.cpp ALNS/*.cpp Utils/*.cpp
    mv -f "$tmp_exe" "$EXE"
}

mkdir -p "$BUILD_DIR"
if command -v flock > /dev/null 2>&1; then
    (
        flock -w 600 9 || { echo "[ERROR] No se obtuvo el lock de compilacion en 10 min"; exit 1; }
        compilar
    ) 9> "$BUILD_DIR/.build.lock"
else
    echo "[AVISO] flock no disponible: no lances dos jobs a la vez"
    ( compilar )
fi

# Comprobacion: el ejecutable debe ser mas nuevo que todas las fuentes
NEWER_SRC="$(find "$REPO_DIR" -path "$BUILD_DIR" -prune -o \( -name '*.cpp' -o -name '*.h' \) -newer "$EXE" -print | head -1)"
if [ -n "$NEWER_SRC" ]; then
    echo "[ERROR] $NEWER_SRC es mas nuevo que el ejecutable; abortando para no usar un binario viejo"
    exit 1
fi
echo "[BUILD] OK: $EXE ($(date -r "$EXE" '+%Y-%m-%d %H:%M:%S'))"

# 4. Lanzar la campaña
python3 automate.py \
    --size "$SIZE" \
    --workers "$SLURM_CPUS_PER_TASK" \
    --runs 10 \
    "$@"

echo "Campaña para size=$SIZE finalizada."
