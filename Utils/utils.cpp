#include "utils.h"

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
            
            // 1. Auditoría de Capacidad (Solo suma demanda si el siguiente es un cliente)
            if (next != 0) {
                current_load += inst.clients[next].demand;
                if (current_load > inst.capacity) {
                    std::cerr << "[!] Violación de capacidad en la Ruta " << r 
                              << ". Al llegar al cliente " << next 
                              << ", la carga " << current_load << " excedió el límite de " << inst.capacity << "\n";
                    is_valid = false;
                }
            }

            // 2. Cálculo de Llegada y Auditoría Temporal
            route_distance += inst.dist_mat[curr][next];
            double arrival_time = current_time + inst.clients[curr].service_time + inst.dist_mat[curr][next];

            if (arrival_time > inst.clients[next].due_date + 1e-6) {
                std::cerr << "[!] Infracción temporal en la Ruta " << r 
                          << ". Llegada al nodo " << next << " en t=" << arrival_time 
                          << ", pero su ventana cerró en t=" << inst.clients[next].due_date << "\n";
                is_valid = false;
            }

            // 3. Ajuste de tiempo por espera temprana (si llega antes del ready_time, espera)
            current_time = std::max(arrival_time, inst.clients[next].ready_time);
        }

        calculated_distance += route_distance;
    }

    // Validando cobertura
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

    /// Validando f_1
    if (calculated_vehicles != sol.used_vehicles) {
        std::cerr << "[!] Inconsistencia en f_1 (Vehículos). Calculado: " << calculated_vehicles 
                  << " vs Reportado por Solution: " << sol.used_vehicles << "\n";
        is_valid = false;
    }

    /// Validando f_2
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

void solveExact(Solution& current_sol, std::vector<bool>& unassigned, int unassigned_count, double& best_cost, Solution& best_sol) {
    const Instance& inst = current_sol.inst;

    current_sol.updateMetrics();
    double current_cost = (current_sol.used_vehicles * 10000.0) + current_sol.total_distance;

    if (current_cost >= best_cost) return; // Podando
    
    // CASO BASE: Éxito al asignar a todos los clientes
    if (unassigned_count == 0) {
        best_cost = current_cost;
        best_sol = current_sol;
        std::cout << ">>> Nuevo Optimo Global Encontrado! Costo: " << best_cost 
                  << " (Vehiculos: " << current_sol.used_vehicles << ")\n";
        return;
    }

    // SELECCIÓN DE CLIENTE
    int client_id = -1;
    for (size_t i = 1; i < inst.clients.size(); ++i) {
        if (unassigned[i]) {
            client_id = i;
            break;
        }
    }
    if (client_id == -1) return;

    const Client& u_client = inst.clients[client_id]; // Explorando

    // Opción A: Intentar insertar en TODAS las rutas activas, en TODAS las posiciones
    for (size_t r = 0; r < current_sol.routes.size(); ++r) {
        Route& route = current_sol.routes[r];
        if (route.load + u_client.demand > inst.capacity) continue;

        for (size_t i = 0; i < route.path.size() - 1; ++i) {
            int prev = route.path[i];
            int next = route.path[i + 1];

            if (!inst.is_reachable[prev][client_id]) continue;
            if (!inst.is_reachable[client_id][next]) continue;

            double start_prev = std::max(route.arrival_times[i], inst.clients[prev].ready_time);
            double arrival_u = start_prev + inst.clients[prev].service_time + inst.dist_mat[prev][client_id];
            if (arrival_u > u_client.due_date) continue;

            double start_u = std::max(arrival_u, u_client.ready_time);
            double arrival_j_new = start_u + u_client.service_time + inst.dist_mat[client_id][next];
            double delay = std::max(0.0, arrival_j_new - route.arrival_times[i + 1]);
            
            if (delay > route.wait_times[i+1] + route.time_slacks[i+1]) continue;

            // Hacer
            route.path.insert(route.path.begin() + i + 1, client_id);
            route.recalculate(inst);
            unassigned[client_id] = false;

            // Llamada recursiva
            solveExact(current_sol, unassigned, unassigned_count - 1, best_cost, best_sol);

            // Deshacer
            route.path.erase(route.path.begin() + i + 1);
            route.recalculate(inst);
            unassigned[client_id] = true;
        }
    }

    // Opción B: Forzar la creación de un vehículo nuevo para este cliente
    Route new_route;
    new_route.path.insert(new_route.path.begin() + 1, client_id);
    new_route.recalculate(inst);
    
    // Hacer
    current_sol.routes.push_back(new_route);
    unassigned[client_id] = false;

    // Llamada Recursiva
    solveExact(current_sol, unassigned, unassigned_count - 1, best_cost, best_sol);

    // Deshacer
    current_sol.routes.pop_back();
    unassigned[client_id] = true;
}

Solution solve_with_classic(const Instance& inst, const Solution& sol, int max_iters) {
    std::cout << "[3] Iniciando ALNS por " << max_iters << " iteraciones...\n";
    ALNS solver(inst, sol);
    
    Solution best_solution = solver.solve(max_iters);
    
    verifySolution(inst, best_solution);
    return best_solution;
}

Solution solve_with_qlearning(const Instance& inst, const Solution& sol, int max_iters) {
    std::cout << "[3] Iniciando ALNS con Q-Learning por " << max_iters << " iteraciones...\n";
    ALNS_QLearning solver(inst, sol);

    Solution best_solution = solver.solve(max_iters);

    verifySolution(inst, best_solution);
    return best_solution;
}