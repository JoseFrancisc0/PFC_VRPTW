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

# Pasos (desde Experiments/, en el nodo de login):
#   1. git pull
#   2. bash compilar.sh
#   3. sbatch VRPTW_homberger.sh 600          (el numero es el tamaño; sin numero usa 800)
# Lo que va despues del tamaño se pasa a automate.py, p.ej.:
#   sbatch VRPTW_homberger.sh 600 --algos QLEARNING --master results/master_gh600_ql.csv

SIZE="${1:-800}"
shift

# 1. Asegurar posición en la carpeta donde reside el script (Experiments/)
cd "$SLURM_SUBMIT_DIR"

# 2. Si el ejecutable es mas viejo que el codigo (faltó bash compilar.sh), no correr nada
if [ ! -f ../build/ALNS_VRPTW ] || [ -n "$(find .. -path ../build -prune -o -name '*.cpp' -newer ../build/ALNS_VRPTW -print -o -name '*.h' -newer ../build/ALNS_VRPTW -print)" ]; then
    echo "[ERROR] El ejecutable no existe o es mas viejo que el codigo. Corre primero: bash compilar.sh"
    exit 1
fi

# 3. Cargar módulos necesarios
module purge
module load gnu12/12.4.0
module load python3/3.11.11

echo "============================================================"
echo "Job ID:           $SLURM_JOB_ID"
echo "Nodo asignado:    $SLURMD_NODENAME"
echo "Workers asignados: $SLURM_CPUS_PER_TASK"
echo "Tamaño:           $SIZE"
echo "Commit:           $(git log -1 --format='%h %s' 2>/dev/null)"
echo "============================================================"

# 4. Lanzar la campaña
python3 automate.py \
    --size "$SIZE" \
    --workers $SLURM_CPUS_PER_TASK \
    --runs 10 \
    "$@"

echo "Campaña para size=$SIZE finalizada."
