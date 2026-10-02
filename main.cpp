#include <iostream>
#include <chrono>
#include <random>
#include <string>
#include <filesystem>
#include "ALNS/alns.h"
#include "ALNS/alns_qlearning.h"
#include "Utils/utils.h"

unsigned seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
std::mt19937 rng(seed);

// Valores por defecto cuando el .exe se ejecuta con menos argumentos (o
// ninguno). Cada argumento posicional que falte toma el valor de aqui.
// La ruta de la instancia es relativa a la raiz del repositorio; ver
// resolveInstancePath para desde donde se puede lanzar el .exe.
static const std::string DEFAULT_INSTANCE  = "homberger-400/RC1/RC1_4_1.TXT";
static const std::string DEFAULT_ALGORITHM = "QLEARNING";   // "CLASSIC" / "QLEARNING"
static const int         DEFAULT_ITERS     = 25000;
static const long        DEFAULT_SEED      = 1;           // < 0 -> semilla por reloj

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

// Una ruta relativa se busca primero desde el directorio actual y, si no
// existe ahi, desde la raiz del repo deducida de la ubicacion del .exe
// (build/ALNS_VRPTW.exe -> ..). Asi el .exe funciona lanzado tanto desde la
// raiz como desde build/ o Experiments/.
static std::string resolveInstancePath(const std::string& path, const char* argv0) {
    namespace fs = std::filesystem;
    fs::path p(path);
    if (p.is_absolute() || fs::exists(p)) return path;
    fs::path from_exe = fs::absolute(fs::path(argv0)).parent_path().parent_path() / p;
    if (fs::exists(from_exe)) return from_exe.string();
    return path; // que loadSolomon reporte la ruta tal como se pidio
}

int main(int argc, char** argv) {
    try {
        std::string instance_file = resolveInstancePath(argc >= 2 ? argv[1] : DEFAULT_INSTANCE, argv[0]);
        std::string algorithm = argc >= 3 ? argv[2] : DEFAULT_ALGORITHM; // "CLASSIC" / "QLEARNING"
        int max_iters = argc >= 4 ? std::stoi(argv[3]) : DEFAULT_ITERS;

        // Semilla: permite correr CLASSIC y QLEARNING sobre el mismo
        // stream aleatorio (comparacion pareada). Con semilla < 0 cada
        // proceso se siembra con el reloj y una diferencia real entre
        // ambos algoritmos queda enterrada en la varianza entre corridas.
        long seed_arg = argc >= 5 ? std::stol(argv[4]) : DEFAULT_SEED;
        if (seed_arg >= 0) rng.seed(static_cast<unsigned>(seed_arg));
        std::string seed_str = seed_arg >= 0 ? std::to_string(seed_arg) : "clock";

        std::cout << "[CONFIG] instancia=" << instance_file << " algoritmo=" << algorithm
                  << " iters=" << max_iters << " semilla=" << seed_str << "\n";

        // Parametros opcionales 'clave=valor' (ver Utils/params.h).
        SolverParams params;
        for (int i = 5; i < argc; ++i) params.set(argv[i]);

        Instance inst(instance_file);
        std::cout << "[1] Instancia cargada: " << inst.clients.size() - 1 << " clientes\n";

        Solution initial_sol(inst);
        std::cout << "[2] Solucion inicial -> Veh: " << initial_sol.used_vehicles
                  << ", Dist: " << initial_sol.total_distance << "\n";

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