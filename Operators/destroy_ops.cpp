#include "operators.h"

/// Quitamos clientes aleatorios de rutas
void randomRemoval(Solution& sol, int q){
    int N = sol.inst.clients.size();
    int removed = 0;

    while (removed < q && sol.unassigned.size() < N - 1){
        std::vector<int> active_routes;
        for (int i = 0; i < sol.routes.size(); ++i) {
            if (sol.routes[i].path.size() > 2) {
                active_routes.push_back(i);
            }
        }

        if (active_routes.empty()) break;

        std::uniform_int_distribution<int> route_distr(0, active_routes.size() - 1);
        int r_idx = active_routes[route_distr(rng)];
        Route& route = sol.routes[r_idx];

        std::uniform_int_distribution<int> client_distr(1, route.path.size() - 2);
        int c_idx = client_distr(rng);
        int client_id = route.path[c_idx];
        
        sol.unassigned.push_back(client_id);
        route.path.erase(route.path.begin() + c_idx);
        route.recalculate(sol.inst);
        removed++;
    }

    sol.updateMetrics();
}

// Eliminamos rutas aleatorias
void routeRemoval(Solution& sol, int q){
    int N = sol.inst.clients.size();
    int removed = 0;

    while (removed < q && sol.unassigned.size() < N - 1) {
        std::vector<int> active_routes;
        for (int i = 0; i < sol.routes.size(); ++i)
            if (sol.routes[i].path.size() > 2)
                active_routes.push_back(i);
        
        if (active_routes.empty()) break;

        std::uniform_int_distribution<int> distr(0, active_routes.size() - 1);
        int chosen_idx = active_routes[distr(rng)];
        Route& route = sol.routes[chosen_idx];
        
        for (size_t i = 1; i < route.path.size() - 1; ++i) {
            sol.unassigned.push_back(route.path[i]);
            removed++;
        }

        route.path.clear();
        route.path.push_back(0);
        route.path.push_back(0);

        route.recalculate(sol.inst);
    }

    sol.updateMetrics();
}

// Eliminamos clientes mas ineficientes
void worstRemoval(Solution& sol, int q, double p){
    int N = sol.inst.clients.size();
    int removed = 0;

    while (removed < q && sol.unassigned.size() < N - 1) {
        std::vector<RemovalCandidate> candidates;

        for (int r = 0; r < sol.routes.size(); ++r) {
            Route& route = sol.routes[r];
            if (route.path.size() <= 2) continue;

            for (int i = 1; i < route.path.size() - 1; ++i) {
                int prev = route.path[i-1];
                int curr = route.path[i];
                int next = route.path[i+1];

                double cost = sol.inst.dist_mat[prev][curr] + sol.inst.dist_mat[curr][next] - sol.inst.dist_mat[prev][next];
                candidates.push_back({r, i, cost});
            }
        }

        if (candidates.empty()) break;

        std::sort(candidates.begin(), candidates.end(),
            [](const RemovalCandidate& a, const RemovalCandidate& b) {
                return a.deviation_cost > b.deviation_cost;
            });
        
        std::uniform_real_distribution<double> distr(0.0, 1.0);
        double y = distr(rng);

        int chosen_idx = static_cast<int>(std::pow(y,p) * candidates.size());
        if (chosen_idx >= candidates.size())
            chosen_idx = candidates.size() - 1;
        
        RemovalCandidate chosen = candidates[chosen_idx];
        Route& target_route = sol.routes[chosen.route_idx];

        sol.unassigned.push_back(target_route.path[chosen.node_idx]);
        target_route.path.erase(target_route.path.begin() + chosen.node_idx);
        target_route.recalculate(sol.inst);
        removed++;
    }

    sol.updateMetrics();
}

// Eliminamos clientes mas parecidos
void shawRemoval(Solution& sol, int q, double p){
    const double w_dist = 9.0;
    const double w_time = 3.0;
    const double w_demand = 2.0;

    int N = sol.inst.clients.size();
    int removed = 0;
    std::vector<int> removed_clients;

    std::vector<int> active_routes;
    for (int i = 0; i < sol.routes.size(); ++i)
        if (sol.routes[i].path.size() > 2)
            active_routes.push_back(i);
    
    if (active_routes.empty()) return;

    std::uniform_int_distribution<int> r_distr(0, active_routes.size() - 1);
    int first_r_idx = active_routes[r_distr(rng)];
    Route& first_route = sol.routes[first_r_idx];

    std::uniform_int_distribution<int> c_distr(1, first_route.path.size() - 2);
    int first_c_idx = c_distr(rng);
    int first_client = first_route.path[first_c_idx];

    removed_clients.push_back(first_client);
    sol.unassigned.push_back(first_client);
    first_route.path.erase(first_route.path.begin() + first_c_idx);
    first_route.recalculate(sol.inst);
    removed++;

    while (removed < q && sol.unassigned.size() < N - 1) {
        std::uniform_int_distribution<int> base_distr(0, removed_clients.size() - 1);
        int base_client_id = removed_clients[base_distr(rng)];
        const Client& base_client = sol.inst.clients[base_client_id];

        std::vector<RelatednessCandidate> candidates;
        for (int r = 0; r < sol.routes.size(); ++r) {
            Route& route = sol.routes[r];
            if (route.path.size() <= 2) continue;

            for (int i = 1; i < route.path.size() - 1; ++i) {
                int target_id = route.path[i];
                const Client& target_client = sol.inst.clients[target_id];

                double r_score = 
                    (w_dist * sol.inst.dist_mat[base_client_id][target_id]) +
                    (w_time * std::abs(base_client.ready_time - target_client.ready_time)) +
                    (w_demand * std::abs(base_client.demand - target_client.demand));
                
                candidates.push_back({r, i, target_id, r_score});
            }
        }

        if (candidates.empty()) break;

        std::sort(candidates.begin(), candidates.end(),
            [](const RelatednessCandidate& a, const RelatednessCandidate& b) {
                return a.relatedness < b.relatedness;
            });
        
        std::uniform_real_distribution<double> y_distr(0.0, 1.0);
        double y = y_distr(rng);

        int chosen_idx = static_cast<int>(std::pow(y, p) * candidates.size());
        if (chosen_idx >= candidates.size())
            chosen_idx = candidates.size() - 1;
        
        RelatednessCandidate chosen = candidates[chosen_idx];
        Route& target_route = sol.routes[chosen.route_idx];

        removed_clients.push_back(chosen.client_id);
        sol.unassigned.push_back(chosen.client_id);
        target_route.path.erase(target_route.path.begin() + chosen.node_idx);
        target_route.recalculate(sol.inst);
        removed++;
    }

    sol.updateMetrics();
}// Operador destruir la ruta mas pequenia
void removeSmallestRoute(Solution& sol) {
    if (sol.routes.empty()) return;

    int smallest_idx = -1;
    size_t min_size = std::numeric_limits<size_t>::max();

    for (size_t r = 0; r < sol.routes.size(); ++r) {
        if (sol.routes[r].path.size() > 2 && sol.routes[r].path.size() < min_size) {
            min_size = sol.routes[r].path.size();
            smallest_idx = static_cast<int>(r);
        }
    }

    if (smallest_idx != -1) {
        Route& route = sol.routes[smallest_idx];
        for (size_t i = 1; i < route.path.size() - 1; ++i) {
            sol.unassigned.push_back(route.path[i]);
        }
        sol.routes.erase(sol.routes.begin() + smallest_idx);
    }
    sol.updateMetrics();
}

void timeWindowRemoval(Solution& sol, int q) {
    if (q <= 0 || sol.routes.empty()) return;
    
    struct TW_Candidate {
        int route_idx;
        int node_idx;
        int client_id;
        double tw_width;
    };
    
    std::vector<TW_Candidate> candidates;
    for (int r = 0; r < sol.routes.size(); ++r) {
        for (int i = 1; i < sol.routes[r].path.size() - 1; ++i) {
            int client = sol.routes[r].path[i];
            double width = sol.inst.clients[client].due_date - sol.inst.clients[client].ready_time;
            candidates.push_back({r, i, client, width});
        }
    }
    
    if (candidates.empty()) return;
    
    std::sort(candidates.begin(), candidates.end(), [](const TW_Candidate& a, const TW_Candidate& b) {
        return a.tw_width < b.tw_width;
    });
    
    int to_remove = std::min(q, static_cast<int>(candidates.size()));
    
    // Sort in reverse order of route and index to delete from end safely
    std::vector<TW_Candidate> to_delete(candidates.begin(), candidates.begin() + to_remove);
    std::sort(to_delete.begin(), to_delete.end(), [](const TW_Candidate& a, const TW_Candidate& b) {
        if (a.route_idx != b.route_idx) return a.route_idx > b.route_idx;
        return a.node_idx > b.node_idx;
    });
    
    for (const auto& c : to_delete) {
        sol.unassigned.push_back(c.client_id);
        sol.routes[c.route_idx].path.erase(sol.routes[c.route_idx].path.begin() + c.node_idx);
        sol.routes[c.route_idx].recalculate(sol.inst);
    }
    
    sol.updateMetrics();
}

