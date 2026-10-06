#include "utils.h"
#include <cmath>

bool verifySolution(const Instance& inst, const Solution& sol) {
    int N = inst.clients.size();
    bool is_valid = true;

    std::cout << "\n--- INICIANDO VERIFICACION DE SOLUCION ---\n";
    std::vector<int> visit_count(N, 0);

    int calculated_vehicles = 0;
    double calculated_distance = 0.0;

    for (size_t r = 0; r < sol.routes.size(); ++r) {
        const Route& route = sol.routes[r];
        if (route.path.size() <= 2) continue;

        calculated_vehicles++;
        double current_time = 0.0;
        double current_load = 0.0;
        double route_distance = 0.0;

        for (size_t i = 0; i < route.path.size() - 1; ++i) {
            int curr = route.path[i];
            int next = route.path[i+1];

            if (curr != 0) visit_count[curr]++;

            if (next != 0) {
                current_load += inst.clients[next].demand;
                if (current_load > inst.capacity) {
                    std::cerr << "[!] Violación de capacidad en la Ruta " << r 
                              << ". Al llegar al cliente " << next 
                              << ", la carga " << current_load << " excedió el límite de " << inst.capacity << "\n";
                    is_valid = false;
                }
            }

            route_distance += inst.dist_mat[curr][next];
            double arrival_time = current_time + inst.clients[curr].service_time + inst.dist_mat[curr][next];

            if (arrival_time > inst.clients[next].due_date + 1e-6) {
                std::cerr << "[!] Infracción temporal en la Ruta " << r 
                          << ". Llegada al nodo " << next << " en t=" << arrival_time 
                          << ", pero su ventana cerró en t=" << inst.clients[next].due_date << "\n";
                is_valid = false;
            }

            current_time = std::max(arrival_time, inst.clients[next].ready_time);
        }

        calculated_distance += route_distance;
    }

    for (int i = 1; i < N; ++i) {
        if (visit_count[i] == 0) {
            std::cerr << "[!] Cliente omitido: El cliente " << i << " no fue visitado en ninguna ruta.\n";
            is_valid = false;
        } 
        else if (visit_count[i] > 1) {
            std::cerr << "[!] Cliente duplicado: El cliente " << i << " fue visitado " << visit_count[i] << " veces.\n";
            is_valid = false;
        }
    }

    if (calculated_vehicles != sol.used_vehicles) {
        std::cerr << "[!] Inconsistencia en f_1 (Vehículos). Calculado: " << calculated_vehicles 
                  << " vs Reportado por Solution: " << sol.used_vehicles << "\n";
        is_valid = false;
    }

    if (std::abs(calculated_distance - sol.total_distance) > 1e-4) {
        std::cerr << "[!] Inconsistencia en f_2 (Distancia). Calculado: " << calculated_distance 
                  << " vs Reportado por Solution: " << sol.total_distance << "\n";
        is_valid = false;
    }

    if (is_valid)
        std::cout << "[OK] La solucion es matematicamente 100% FACTIBLE.\n";
    else
        std::cout << "[X] La solucion es INFACTIBLE o tiene inconsistencias.\n";
    
    std::cout << "------------------------------------------\n";

    return is_valid;
}
