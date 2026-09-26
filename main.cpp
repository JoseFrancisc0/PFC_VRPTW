#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include "ALNS/alns.h"
#include "ALNS/alns_qlearning.h"
#include "Utils/utils.h"

unsigned seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
std::mt19937 rng(seed);

int main(int argc, char** argv) {
    if (argc >= 4) {
        try {
            std::string instance_file = argv[1];
            std::string algorithm = argv[2]; // "CLASSIC" / "QLEARNING"
            int max_iters = std::stoi(argv[3]);

            // Semilla opcional: permite correr CLASSIC y QLEARNING sobre el
            // mismo stream aleatorio (comparacion pareada). Sin ella cada
            // proceso se siembra con el reloj y una diferencia real entre
            // ambos algoritmos queda enterrada en la varianza entre corridas.
            if (argc >= 5) rng.seed(static_cast<unsigned>(std::stoul(argv[4])));

            // Parametros opcionales 'clave=valor' (ver Utils/params.h).
            SolverParams params;
            for (int i = 5; i < argc; ++i) params.set(argv[i]);

            Instance inst(instance_file);
            Solution initial_sol(inst);

            Solution best_solution(inst);
            if (algorithm == "CLASSIC")
                best_solution = solve_with_classic(inst, initial_sol, max_iters, params);
            else if (algorithm == "QLEARNING")
                best_solution = solve_with_qlearning(inst, initial_sol, max_iters, params);
            else {
                std::cerr << "Algoritmo desconocido: " << algorithm << "\n";
                return 1;
            }
            std::cout << "[FINAL_RESULT] Veh: " << best_solution.used_vehicles << ", Dist: " << best_solution.total_distance << "\n";

        } catch (const std::exception& e) {
            std::cerr << "ERROR FATAL: " << e.what() << "\n";
            return 1;
        }
    }
    else {
        std::cerr << "Uso incorrecto. Argumentos esperados: <instancia> <CLASSIC|QLEARNING> <iteraciones> [semilla] [clave=valor ...]\n";
        return 1;
    }

    return 0;
}