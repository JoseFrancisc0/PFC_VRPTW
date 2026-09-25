#include "operators.h"

bool evalInsertion(const Solution& sol, int client_id, const Route& route, size_t i, double& delta_cost) {
    int prev = route.path[i];
    int next = route.path[i + 1];
    const Client& u_client = sol.inst.clients[client_id];

    // Existe la ruta?
    if (!sol.inst.is_reachable[prev][client_id]) return false;
    if (!sol.inst.is_reachable[client_id][next]) return false;

    // Se llega a tiempo? (time windows)
    double start_prev = std::max(route.arrival_times[i], sol.inst.clients[prev].ready_time);
    double arrival_u = start_prev + sol.inst.clients[prev].service_time + sol.inst.dist_mat[prev][client_id];

    if (arrival_u > u_client.due_date) return false;

    // Se llega a tiempo? (slack times)
    double start_u = std::max(arrival_u, u_client.ready_time);
    double arrival_j_new = start_u + u_client.service_time + sol.inst.dist_mat[client_id][next];
    double delay = std::max(0.0, arrival_j_new - route.arrival_times[i + 1]);
    
    if (delay > route.wait_times[i+1] + route.time_slacks[i+1]) return false;

    // Desviacion del costo
    delta_cost = sol.inst.dist_mat[prev][client_id] + 
                 sol.inst.dist_mat[client_id][next] - 
                 sol.inst.dist_mat[prev][next];
                 
    return true;
}

// Insertamos lo que minimiza el aumento inmediato de costo
void greedyInsertion(Solution& sol, bool allow_new_routes){
    while (!sol.unassigned.empty()) {
        double best_cost = std::numeric_limits<double>::max();
        int best_client_idx_in_unassigned = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                // Una ruta vacia es, en los hechos, una ruta nueva: si el
                // repair tiene prohibido abrirlas tampoco puede rellenar los
                // cascarones que hayan quedado de iteraciones anteriores.
                if (!allow_new_routes && route.path.size() <= 2) continue;
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertion(sol, client_id, route, i, delta_cost)) continue;

                    if (delta_cost < best_cost) {
                        best_cost = delta_cost;
                        best_client_idx_in_unassigned = u_idx;
                        best_route_idx = r;
                        best_insert_pos = i + 1;
                    }
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[best_route_idx];
            
            target_route.path.insert(target_route.path.begin() + best_insert_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            // Sin permiso para abrir rutas nuevas, lo que no entra queda sin
            // asignar y el solver descartara el candidato. Es el desenlace
            // esperado de un intento fallido de eliminar una ruta, no una
            // anomalia, asi que no se reporta por stderr.
            if (!allow_new_routes) break;

            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                std::cerr << "[!] " << sol.unassigned.size()
                          << " clientes no pudieron ser insertados de forma factible. \n";
                break;
            }
            else
                sol.routes.push_back(Route());
        }
    }

    sol.updateMetrics();
}

// Greedy anticipando la segunda mejor posicion del cliente
void regret2Insertion(Solution& sol, bool allow_new_routes){
    while (!sol.unassigned.empty()) {
        double max_regret = -1.0;
        int best_client_idx_in_unassigned = -1;
        int global_best_route = -1;
        int global_best_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            double best_cost = std::numeric_limits<double>::max();
            double second_best_cost = std::numeric_limits<double>::max();
            int local_best_route = -1;
            int local_best_pos = -1;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                // Una ruta vacia es, en los hechos, una ruta nueva: si el
                // repair tiene prohibido abrirlas tampoco puede rellenar los
                // cascarones que hayan quedado de iteraciones anteriores.
                if (!allow_new_routes && route.path.size() <= 2) continue;
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertion(sol, client_id, route, i, delta_cost)) continue;

                    if (delta_cost < best_cost) {
                        second_best_cost = best_cost;
                        best_cost = delta_cost;
                        local_best_route = r;
                        local_best_pos = i+1;
                    }
                    else if (delta_cost < second_best_cost)
                        second_best_cost = delta_cost;
                }
            }

            if (best_cost != std::numeric_limits<double>::max()) {
                double regret = (second_best_cost == std::numeric_limits<double>::max())
                                ? 1e9 : (second_best_cost - best_cost);

                if (regret > max_regret) {
                    max_regret = regret;
                    best_client_idx_in_unassigned = u_idx;
                    global_best_route = local_best_route;
                    global_best_pos = local_best_pos;
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[global_best_route];

            target_route.path.insert(target_route.path.begin() + global_best_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            // Sin permiso para abrir rutas nuevas, lo que no entra queda sin
            // asignar y el solver descartara el candidato. Es el desenlace
            // esperado de un intento fallido de eliminar una ruta, no una
            // anomalia, asi que no se reporta por stderr.
            if (!allow_new_routes) break;

            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                std::cerr << "[!] " << sol.unassigned.size()
                          << " clientes no pudieron ser insertados de forma factible. \n";
                break;
            }
            else
                sol.routes.push_back(Route());
        }
    }

    sol.updateMetrics();
}

// Regret-2 pero para las m=3 mejores rutas
void regret3Insertion(Solution& sol, bool allow_new_routes){
    while (!sol.unassigned.empty()) {
        double max_regret = -1.0;
        int best_client_idx_in_unassigned = -1;
        int global_best_route = -1;
        int global_best_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            std::vector<RouteInsertion> best_per_route;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                // Una ruta vacia es, en los hechos, una ruta nueva: si el
                // repair tiene prohibido abrirlas tampoco puede rellenar los
                // cascarones que hayan quedado de iteraciones anteriores.
                if (!allow_new_routes && route.path.size() <= 2) continue;
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                double best_cost_in_r = std::numeric_limits<double>::max();
                int best_pos_in_r = -1;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertion(sol, client_id, route, i, delta_cost)) continue;

                    if (delta_cost < best_cost_in_r) {
                        best_cost_in_r = delta_cost;
                        best_pos_in_r = i + 1;
                    }
                }

                if (best_cost_in_r != std::numeric_limits<double>::max())
                    best_per_route.push_back({static_cast<int>(r), best_pos_in_r, best_cost_in_r});
            }

            if (!best_per_route.empty()){
                std::sort(best_per_route.begin(), best_per_route.end(),
                    [](const RouteInsertion& a, const RouteInsertion& b) {
                        return a.cost < b.cost;
                    });

                double best_cost = best_per_route[0].cost;
                double regret = 0.0;

                int m = std::min(3, static_cast<int>(best_per_route.size()));
                for (int k = 1; k < m; ++k)
                    regret += (best_per_route[k].cost - best_cost);

                int missing_routes = 3 - m;
                regret += missing_routes * 10000.0;

                if (regret > max_regret) {
                    max_regret = regret;
                    best_client_idx_in_unassigned = u_idx;
                    global_best_route = best_per_route[0].route_idx;
                    global_best_pos = best_per_route[0].insert_pos; 
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[global_best_route];

            target_route.path.insert(target_route.path.begin() + global_best_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            // Sin permiso para abrir rutas nuevas, lo que no entra queda sin
            // asignar y el solver descartara el candidato. Es el desenlace
            // esperado de un intento fallido de eliminar una ruta, no una
            // anomalia, asi que no se reporta por stderr.
            if (!allow_new_routes) break;

            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                std::cerr << "[!] " << sol.unassigned.size()
                          << " clientes no pudieron ser insertados de forma factible. \n";
                break;
            }
            else
                sol.routes.push_back(Route());
        }
    }

    sol.updateMetrics();
}

// Greedy con ruido
void pGreedyInsertion(Solution& sol, bool allow_new_routes, double eta){
    std::uniform_real_distribution<double> noise_distr(1.0 - eta, 1.0 + eta);

    while (!sol.unassigned.empty()) {
        double best_cost = std::numeric_limits<double>::max();
        int best_client_idx_in_unassigned = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                // Una ruta vacia es, en los hechos, una ruta nueva: si el
                // repair tiene prohibido abrirlas tampoco puede rellenar los
                // cascarones que hayan quedado de iteraciones anteriores.
                if (!allow_new_routes && route.path.size() <= 2) continue;
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertion(sol, client_id, route, i, delta_cost)) continue;

                    double perturbed_cost = delta_cost * noise_distr(rng);
                    if (perturbed_cost < best_cost) {
                        best_cost = perturbed_cost;
                        best_client_idx_in_unassigned = u_idx;
                        best_route_idx = r;
                        best_insert_pos = i + 1;
                    }
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[best_route_idx];

            target_route.path.insert(target_route.path.begin() + best_insert_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            // Sin permiso para abrir rutas nuevas, lo que no entra queda sin
            // asignar y el solver descartara el candidato. Es el desenlace
            // esperado de un intento fallido de eliminar una ruta, no una
            // anomalia, asi que no se reporta por stderr.
            if (!allow_new_routes) break;

            if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
                std::cerr << "[!] " << sol.unassigned.size()
                          << " clientes no pudieron ser insertados de forma factible. \n";
                break;
            }
            else
                sol.routes.push_back(Route());
        }
    }

    sol.updateMetrics();
}
// ======= REPARACION FASE 1 (RELAJADA) =======

bool evalInsertionRelaxed(const Solution& sol, int client_id, const Route& route, size_t i, double& delta_cost) {
    int prev = route.path[i];
    int next = route.path[i + 1];
    const Client& u_client = sol.inst.clients[client_id];

    // En Fase 1 NO usamos sol.inst.is_reachable porque is_reachable valida TW estrictamente!
    // Solo validamos capacidad en los bucles for externos (load + demand <= capacity).

    double current_dist_cost = sol.inst.dist_mat[prev][client_id] + sol.inst.dist_mat[client_id][next] - sol.inst.dist_mat[prev][next];
    
    // Calculamos el lateness aproximado de insertar aqui
    double start_prev = std::max(route.arrival_times[i], sol.inst.clients[prev].ready_time);
    double arrival_u = start_prev + sol.inst.clients[prev].service_time + sol.inst.dist_mat[prev][client_id];
    
    double added_lateness = 0.0;
    if (arrival_u > u_client.due_date) {
        added_lateness += (arrival_u - u_client.due_date);
    }
    
    double start_u = std::max(arrival_u, u_client.ready_time);
    double arrival_j_new = start_u + u_client.service_time + sol.inst.dist_mat[client_id][next];
    
    double delay = arrival_j_new - route.arrival_times[i + 1];
    
    // Si delay > 0, empuja el resto de la ruta
    if (delay > 0) {
        // Cuanto empuja sin ser absorbido por slacks locales
        double push_forward = std::max(0.0, delay - (route.wait_times[i+1] + route.time_slacks[i+1]));
        added_lateness += push_forward; 
    }

    // Cost_phase1 penaliza lateness con 100
    delta_cost = current_dist_cost + (added_lateness * 100.0);
    return true;
}

void greedyInsertionRelaxed(Solution& sol){
    while (!sol.unassigned.empty()) {
        double best_cost = std::numeric_limits<double>::max();
        int best_client_idx_in_unassigned = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertionRelaxed(sol, client_id, route, i, delta_cost)) continue;

                    if (delta_cost < best_cost) {
                        best_cost = delta_cost;
                        best_client_idx_in_unassigned = u_idx;
                        best_route_idx = r;
                        best_insert_pos = i + 1;
                    }
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[best_route_idx];
            
            target_route.path.insert(target_route.path.begin() + best_insert_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            // Si incluso relajado no pudo (posiblemente por capacidad estricta), crea nueva
            sol.routes.push_back(Route());
        }
    }
    sol.updateMetrics();
}

void regret2InsertionRelaxed(Solution& sol){
    while (!sol.unassigned.empty()) {
        double max_regret = -1.0;
        int best_client_idx_in_unassigned = -1;
        int global_best_route = -1;
        int global_best_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            double best_cost = std::numeric_limits<double>::max();
            double second_best_cost = std::numeric_limits<double>::max();
            int local_best_route = -1;
            int local_best_pos = -1;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertionRelaxed(sol, client_id, route, i, delta_cost)) continue;

                    if (delta_cost < best_cost) {
                        second_best_cost = best_cost;
                        best_cost = delta_cost;
                        local_best_route = r;
                        local_best_pos = i+1;
                    }
                    else if (delta_cost < second_best_cost)
                        second_best_cost = delta_cost;
                }
            }

            if (best_cost != std::numeric_limits<double>::max()) {
                double regret = (second_best_cost == std::numeric_limits<double>::max())
                                ? 1e9 : (second_best_cost - best_cost);

                if (regret > max_regret) {
                    max_regret = regret;
                    best_client_idx_in_unassigned = u_idx;
                    global_best_route = local_best_route;
                    global_best_pos = local_best_pos;
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[global_best_route];

            target_route.path.insert(target_route.path.begin() + global_best_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            sol.routes.push_back(Route());
        }
    }
    sol.updateMetrics();
}

void regret3InsertionRelaxed(Solution& sol){
    while (!sol.unassigned.empty()) {
        double max_regret = -1.0;
        int best_client_idx_in_unassigned = -1;
        int global_best_route = -1;
        int global_best_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            std::vector<RouteInsertion> best_per_route;

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                double best_cost_in_r = std::numeric_limits<double>::max();
                int best_pos_in_r = -1;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertionRelaxed(sol, client_id, route, i, delta_cost)) continue;

                    if (delta_cost < best_cost_in_r) {
                        best_cost_in_r = delta_cost;
                        best_pos_in_r = i + 1;
                    }
                }

                if (best_cost_in_r != std::numeric_limits<double>::max())
                    best_per_route.push_back({static_cast<int>(r), best_pos_in_r, best_cost_in_r});
            }

            if (!best_per_route.empty()){
                std::sort(best_per_route.begin(), best_per_route.end(),
                    [](const RouteInsertion& a, const RouteInsertion& b) {
                        return a.cost < b.cost;
                    });

                double best_cost = best_per_route[0].cost;
                double regret = 0.0;

                int m = std::min(3, static_cast<int>(best_per_route.size()));
                for (int k = 1; k < m; ++k)
                    regret += (best_per_route[k].cost - best_cost);

                int missing_routes = 3 - m;
                regret += missing_routes * 10000.0;

                if (regret > max_regret) {
                    max_regret = regret;
                    best_client_idx_in_unassigned = u_idx;
                    global_best_route = best_per_route[0].route_idx;
                    global_best_pos = best_per_route[0].insert_pos; 
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[global_best_route];

            target_route.path.insert(target_route.path.begin() + global_best_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            sol.routes.push_back(Route());
        }
    }
    sol.updateMetrics();
}

void pGreedyInsertionRelaxed(Solution& sol, double eta){
    std::uniform_real_distribution<double> noise_distr(1.0 - eta, 1.0 + eta);

    while (!sol.unassigned.empty()) {
        double best_cost = std::numeric_limits<double>::max();
        int best_client_idx_in_unassigned = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;

        for (size_t u_idx = 0; u_idx < sol.unassigned.size(); ++u_idx) {
            int client_id = sol.unassigned[u_idx];
            const Client& u_client = sol.inst.clients[client_id];

            for (size_t r = 0; r < sol.routes.size(); ++r) {
                Route& route = sol.routes[r];
                if (route.load + u_client.demand > sol.inst.capacity) continue;

                for (size_t i = 0; i < route.path.size() - 1; ++i) {
                    double delta_cost = 0.0;
                    if (!evalInsertionRelaxed(sol, client_id, route, i, delta_cost)) continue;

                    double perturbed_cost = delta_cost * noise_distr(rng);
                    if (perturbed_cost < best_cost) {
                        best_cost = perturbed_cost;
                        best_client_idx_in_unassigned = u_idx;
                        best_route_idx = r;
                        best_insert_pos = i + 1;
                    }
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            int client_to_insert = sol.unassigned[best_client_idx_in_unassigned];
            Route& target_route = sol.routes[best_route_idx];

            target_route.path.insert(target_route.path.begin() + best_insert_pos, client_to_insert);
            target_route.recalculate(sol.inst);
            sol.unassigned[best_client_idx_in_unassigned] = sol.unassigned.back();
            sol.unassigned.pop_back();
        }
        else {
            sol.routes.push_back(Route());
        }
    }
    sol.updateMetrics();
}
