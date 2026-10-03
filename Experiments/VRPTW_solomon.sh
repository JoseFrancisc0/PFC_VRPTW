#!/bin/bash
#SBATCH --job-name=vrptw_solomon_100
#SBATCH --partition=debug       # Partición de nodos CPU
#SBATCH --nodes=1                  # 1 nodo para memoria compartida en Python
#SBATCH --ntasks=1                 # 1 proceso principal
#SBATCH --cpus-per-task=32         # Máximo permitido por tu cuenta (cpu=32)
#SBATCH --mem=64G                  # Memoria holgada para 32 workers concurrentes
#SBATCH --output=vrptw_%j.out      # Salida estándar (prints de avance de automate.py)
#SBATCH --error=vrptw_%j.err       # Errores y tracebacks de Python / C++

# 1. Asegurar posición en la carpeta donde reside el script (Experiments/)
cd "$SLURM_SUBMIT_DIR"

# 2. Cargar módulos necesarios
module purge
module load gnu12/12.4.0
module load python3/3.11.11

echo "============================================================"
echo "Job ID:           $SLURM_JOB_ID"
echo "Nodo asignado:    $SLURMD_NODENAME"
echo "Workers asignados: $SLURM_CPUS_PER_TASK"
echo "============================================================"

# 3. Lanzar la campaña
python3 automate_solomon.py

echo "Campaña para size=100 finalizada."
