#!/bin/bash
# Actualiza el repositorio (git pull) y compila build/ALNS_VRPTW en el cluster. Ejecutar en el nodo de login:
#
#     bash compilar.sh
#
# Se puede correr desde cualquier carpeta. Si el pull falla (p. ej. cambios locales), no compila.

set -e
cd "$(dirname "$0")/.."

git pull
echo "Codigo en: $(git log -1 --format='%h %s')"

module purge
module load gnu12/12.4.0

mkdir -p build
g++ -O3 -std=c++17 -o build/ALNS_VRPTW \
    main.cpp "VRPTW Environment"/*.cpp Operators/*.cpp ALNS/*.cpp Utils/*.cpp

echo "Compilado: $(pwd)/build/ALNS_VRPTW"
