#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <filesystem>
#include <time.h>
#include <memory>
#include "ALNS/alns_classic.h"
#include "ALNS/alns_qlearning.h"
#include "Utils/utils.h"

unsigned seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
std::mt19937 rng(seed);

static const std::string DEFAULT_INSTANCE  = "solomon-100/RC1/rc101.txt";
static const std::string DEFAULT_ALGORITHM = "QLEARNING";  
static const int         DEFAULT_ITERS     = 25000;
static const long        DEFAULT_SEED      = 1;      

static double get_cpu_time() {
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

static std::string resolveInstancePath(const std::string& path, const char* argv0) {
    namespace fs = std::filesystem;
    fs::path p(path);
    if (p.is_absolute() || fs::exists(p)) return path;
    fs::path from_exe = fs::absolute(fs::path(argv0)).parent_path().parent_path() / p;
    if (fs::exists(from_exe)) return from_exe.string();
    return path; 
}

int main(int argc, char** argv) {
    try {
        std::string instance_file = resolveInstancePath(argc >= 2 ? argv[1] : DEFAULT_INSTANCE, argv[0]);
        std::string algorithm = argc >= 3 ? argv[2] : DEFAULT_ALGORITHM; // "CLASSIC" / "QLEARNING"
        int max_iters = argc >= 4 ? std::stoi(argv[3]) : DEFAULT_ITERS;

        long seed_arg = argc >= 5 ? std::stol(argv[4]) : DEFAULT_SEED;
        if (seed_arg >= 0) rng.seed(static_cast<unsigned>(seed_arg));
        std::string seed_str = seed_arg >= 0 ? std::to_string(seed_arg) : "clock";

        std::cout << "[CONFIG] instancia=" << instance_file << " algoritmo=" << algorithm
                  << " iters=" << max_iters << " semilla=" << seed_str << "\n";

        SolverParams params;
        for (int i = 5; i < argc; ++i) params.set(argv[i]);

        Instance inst(instance_file);
        std::cout << "[1] Instancia cargada: " << inst.clients.size() - 1 << " clientes\n";

        Solution initial_sol(inst);
        std::cout << "[2] Solucion inicial -> Veh: " << initial_sol.used_vehicles
                  << ", Dist: " << initial_sol.total_distance << "\n";

        double start_cpu = get_cpu_time();

        std::unique_ptr<ALNS> solver;
        if (algorithm == "CLASSIC") {
            std::cout << "[3] Iniciando ALNS por " << max_iters << " iteraciones...\n";
            solver = std::make_unique<ALNS_Classic>(initial_sol, params);
        }
        else if (algorithm == "QLEARNING") {
            std::cout << "[3] Iniciando ALNS con Q-Learning por " << max_iters << " iteraciones...\n";
            solver = std::make_unique<ALNS_QLearning>(initial_sol, params);
        }
        else {
            std::cerr << "Algoritmo desconocido: " << algorithm << "\n";
            return 1;
        }

        Solution best_solution = solver->solve(max_iters);
        double cpu_time_used = get_cpu_time() - start_cpu;
        bool is_valid = verifySolution(inst, best_solution);

        std::cout << "[FINAL_RESULT] Veh: " << best_solution.used_vehicles << ", Dist: " << best_solution.total_distance << "\n";

        std::string inst_name = instance_file.substr(instance_file.find_last_of("/\\") + 1);
        inst_name = inst_name.substr(0, inst_name.find_last_of('.'));
        std::cout << "RESULT"
                  << ";instance=" << inst_name
                  << ";algorithm=" << algorithm
                  << ";seed=" << seed_str
                  << ";best_veh=" << best_solution.used_vehicles
                  << ";best_dist=" << best_solution.total_distance
                  << ";cpu_time=" << cpu_time_used
                  << ";valid=" << (is_valid ? 1 : 0)
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "ERROR FATAL: " << e.what() << "\n";
        return 1;
    }

    return 0;
}