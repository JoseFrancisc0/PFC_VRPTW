#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include "ALNS/alns.h"
#include "ALNS/alns_qlearning.h"
#include "Utils/utils.h"

unsigned seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
std::mt19937 rng(seed);

// Tiempo de CPU del proceso (user + kernel). A diferencia del wall-clock, no se
// infla cuando varias corridas comparten la maquina en paralelo.
#ifdef _WIN32
#include <windows.h>
static double get_cpu_time() {
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        ULARGE_INTEGER k, u;
        k.LowPart = ftKernel.dwLowDateTime;     k.HighPart = ftKernel.dwHighDateTime;
        u.LowPart = ftUser.dwLowDateTime;       u.HighPart = ftUser.dwHighDateTime;
        return static_cast<double>(k.QuadPart + u.QuadPart) / 1e7;
    }
    return 0.0;
}
#else
#include <time.h>
static double get_cpu_time() {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}
#endif

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

            double start_cpu = get_cpu_time();

            Solution best_solution(inst);
            if (algorithm == "CLASSIC")
                best_solution = solve_with_classic(inst, initial_sol, max_iters, params);
            else if (algorithm == "QLEARNING")
                best_solution = solve_with_qlearning(inst, initial_sol, max_iters, params);
            else {
                std::cerr << "Algoritmo desconocido: " << algorithm << "\n";
                return 1;
            }
            double cpu_time_used = get_cpu_time() - start_cpu;
            bool is_valid = verifySolution(inst, best_solution);

            // Linea leida por Experiments/automate0.py (Solomon)
            std::cout << "[FINAL_RESULT] Veh: " << best_solution.used_vehicles << ", Dist: " << best_solution.total_distance << "\n";

            // Linea leida por Experiments/automate.py (Homberger)
            std::string inst_name = instance_file.substr(instance_file.find_last_of("/\\") + 1);
            inst_name = inst_name.substr(0, inst_name.find_last_of('.'));
            std::cout << "RESULT"
                      << ";instance=" << inst_name
                      << ";algorithm=" << algorithm
                      << ";seed=" << (argc >= 5 ? argv[4] : "clock")
                      << ";best_veh=" << best_solution.used_vehicles
                      << ";best_dist=" << best_solution.total_distance
                      << ";cpu_time=" << cpu_time_used
                      << ";valid=" << (is_valid ? 1 : 0)
                      << "\n";

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