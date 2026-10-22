#!/bin/bash
# Compila build/ALNS_VRPTW en el cluster. Ejecutar en el nodo de login despues de cada git pull:
#
#     bash compilar.sh
#
# Se puede correr desde cualquier carpeta. Carga el modulo de gcc, compila y avisa si algo falla.

set -e
cd "$(dirname "$0")/.."

module purge
module load gnu12/12.4.0

mkdir -p build
g++ -O3 -std=c++17 -o build/ALNS_VRPTW \
    main.cpp "VRPTW Environment"/*.cpp Operators/*.cpp ALNS/*.cpp Utils/*.cpp

echo "Compilado: $(pwd)/build/ALNS_VRPTW"
