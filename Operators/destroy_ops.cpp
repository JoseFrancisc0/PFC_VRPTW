#include "operators.h"

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

namespace {

const double WORST_P = 3.0;
const double SHAW_P = 3.0;

struct RemovalCandidate {
    int route_idx;
    int node_idx;
    double deviation_cost;
};

struct RelatednessCandidate {
    int route_idx;
    int node_idx;
    int client_id;
    double relatedness;
};

int buildRouteOf(const Solution& sol, std::vector<int>& route_of) {
    int routed = 0;
    route_of.assign(sol.inst.clients.size(), -1);
    for (int r = 0; r < sol.routes.size(); ++r) {
        const Route& route = sol.routes[r];
        for (int i = 1; i < static_cast<int>(route.path.size()) - 1; ++i) {
            route_of[route.path[i]] = r;
            routed++;
        }
    }
    return routed;
}

inline int nodeIndexOf(const Route& route, int client_id) {
    return std::find(route.path.begin() + 1, route.path.end() - 1, client_id) - route.path.begin();
}

inline double removalCost(const Instance& inst, const Route& route, int i) {
    int prev = route.path[i-1];
    int curr = route.path[i];
    int next = route.path[i+1];
    return inst.dist_mat[prev][curr] + inst.dist_mat[curr][next] - inst.dist_mat[prev][next];
}

struct WorstEntry {
    double cost;
    int client_id;
};

inline bool worstBefore(const WorstEntry& a, const WorstEntry& b) {
    if (a.cost != b.cost) return a.cost > b.cost;
    return a.client_id < b.client_id;
}

int worstByFullSort(const Solution& sol, int chosen_idx) {
    std::vector<RemovalCandidate> candidates;

    for (int r = 0; r < sol.routes.size(); ++r) {
        const Route& route = sol.routes[r];
        if (route.path.size() <= 2) continue;

        for (int i = 1; i < route.path.size() - 1; ++i)
            candidates.push_back({r, i, removalCost(sol.inst, route, i)});
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const RemovalCandidate& a, const RemovalCandidate& b) {
            return a.deviation_cost > b.deviation_cost;
        });

    const RemovalCandidate& chosen = candidates[chosen_idx];
    return sol.routes[chosen.route_idx].path[chosen.node_idx];
}

const double SHAW_W_DIST = 9.0;
const double SHAW_W_TIME = 3.0;
const double SHAW_W_DEMAND = 2.0;

inline double shawRelatedness(const Instance& inst, int base_client_id, int target_id) {
    const Client& base_client = inst.clients[base_client_id];
    const Client& target_client = inst.clients[target_id];

    return (SHAW_W_DIST * inst.dist_mat[base_client_id][target_id]) +
           (SHAW_W_TIME * std::abs(base_client.ready_time - target_client.ready_time)) +
           (SHAW_W_DEMAND * std::abs(base_client.demand - target_client.demand));
}

const int* shawOrderOf(const Instance& inst, int base_client_id) {
    int N = inst.clients.size();
    int stride = N - 2;

    if (inst.shaw_order.empty()) {
        inst.shaw_order.resize(static_cast<size_t>(N) * stride);
        std::vector<double> score(N);

        for (int base = 1; base < N; ++base) {
            int* row = &inst.shaw_order[static_cast<size_t>(base) * stride];
            int k = 0;
            for (int t = 1; t < N; ++t) {
                if (t == base) continue;
                score[t] = shawRelatedness(inst, base, t);
                row[k++] = t;
            }
            std::sort(row, row + stride, [&](int a, int b) {
                if (score[a] != score[b]) return score[a] < score[b];
                return a < b;
            });
        }
    }

    return &inst.shaw_order[static_cast<size_t>(base_client_id) * stride];
}

int shawByFullSort(const Solution& sol, int base_client_id, int chosen_idx) {
    std::vector<RelatednessCandidate> candidates;
    for (int r = 0; r < sol.routes.size(); ++r) {
        const Route& route = sol.routes[r];
        if (route.path.size() <= 2) continue;

        for (int i = 1; i < route.path.size() - 1; ++i) {
            int target_id = route.path[i];
            candidates.push_back({r, i, target_id, shawRelatedness(sol.inst, base_client_id, target_id)});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const RelatednessCandidate& a, const RelatednessCandidate& b) {
            return a.relatedness < b.relatedness;
        });

    return candidates[chosen_idx].client_id;
}

} 

void worstRemoval(Solution& sol, int q){
    int N = sol.inst.clients.size();
    int removed = 0;

    std::vector<int> route_of;
    buildRouteOf(sol, route_of);

    std::vector<double> cost_of(N, 0.0);
    std::vector<WorstEntry> ranking;
    for (int r = 0; r < sol.routes.size(); ++r) {
        const Route& route = sol.routes[r];
        for (int i = 1; i < static_cast<int>(route.path.size()) - 1; ++i) {
            cost_of[route.path[i]] = removalCost(sol.inst, route, i);
            ranking.push_back({cost_of[route.path[i]], route.path[i]});
        }
    }
    std::sort(ranking.begin(), ranking.end(), worstBefore);

    auto eraseFromRanking = [&](int client_id) {
        WorstEntry key{cost_of[client_id], client_id};
        ranking.erase(std::lower_bound(ranking.begin(), ranking.end(), key, worstBefore));
    };

    while (removed < q && sol.unassigned.size() < N - 1) {
        if (ranking.empty()) break;

        std::uniform_real_distribution<double> distr(0.0, 1.0);
        double y = distr(rng);

        int chosen_idx = static_cast<int>(std::pow(y, WORST_P) * ranking.size());
        if (chosen_idx >= ranking.size())
            chosen_idx = ranking.size() - 1;

        bool tie = (chosen_idx > 0 && ranking[chosen_idx - 1].cost == ranking[chosen_idx].cost) ||
                   (chosen_idx + 1 < ranking.size() && ranking[chosen_idx + 1].cost == ranking[chosen_idx].cost);
        int client_id = tie ? worstByFullSort(sol, chosen_idx) : ranking[chosen_idx].client_id;

        Route& target_route = sol.routes[route_of[client_id]];
        int node_idx = nodeIndexOf(target_route, client_id);

        eraseFromRanking(client_id);
        route_of[client_id] = -1;

        sol.unassigned.push_back(client_id);
        target_route.path.erase(target_route.path.begin() + node_idx);
        target_route.recalculate(sol.inst);
        removed++;

        for (int i : {node_idx - 1, node_idx}) {
            int neighbor = target_route.path[i];
            if (neighbor == 0) continue;

            double new_cost = removalCost(sol.inst, target_route, i);
            if (new_cost == cost_of[neighbor]) continue;

            eraseFromRanking(neighbor);
            cost_of[neighbor] = new_cost;
            WorstEntry entry{new_cost, neighbor};
            ranking.insert(std::lower_bound(ranking.begin(), ranking.end(), entry, worstBefore), entry);
        }
    }

    sol.updateMetrics();
}

void shawRemoval(Solution& sol, int q){
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

    std::vector<int> route_of;
    int routed = buildRouteOf(sol, route_of);
    int stride = N - 2;

    while (removed < q && sol.unassigned.size() < N - 1) {
        std::uniform_int_distribution<int> base_distr(0, removed_clients.size() - 1);
        int base_client_id = removed_clients[base_distr(rng)];

        if (routed == 0) break;

        std::uniform_real_distribution<double> y_distr(0.0, 1.0);
        double y = y_distr(rng);

        int chosen_idx = static_cast<int>(std::pow(y, SHAW_P) * routed);
        if (chosen_idx >= routed)
            chosen_idx = routed - 1;

        const int* order = shawOrderOf(sol.inst, base_client_id);
        int prev_id = -1, client_id = -1, next_id = -1;
        int rank = 0;
        for (int k = 0; k < stride && next_id == -1; ++k) {
            int t = order[k];
            if (route_of[t] == -1) continue;

            if (rank == chosen_idx - 1) prev_id = t;
            else if (rank == chosen_idx) client_id = t;
            else if (rank == chosen_idx + 1) next_id = t;
            rank++;
        }

        double chosen_score = shawRelatedness(sol.inst, base_client_id, client_id);
        bool tie = (prev_id != -1 && shawRelatedness(sol.inst, base_client_id, prev_id) == chosen_score) ||
                   (next_id != -1 && shawRelatedness(sol.inst, base_client_id, next_id) == chosen_score);
        if (tie) client_id = shawByFullSort(sol, base_client_id, chosen_idx);

        Route& target_route = sol.routes[route_of[client_id]];
        int node_idx = nodeIndexOf(target_route, client_id);

        removed_clients.push_back(client_id);
        sol.unassigned.push_back(client_id);
        target_route.path.erase(target_route.path.begin() + node_idx);
        target_route.recalculate(sol.inst);
        route_of[client_id] = -1;
        routed--;
        removed++;
    }

    sol.updateMetrics();
}

void smallestRouteElimination(Solution& sol, RepairOp repair) {
    const int EJECTION_BUDGET = 10;
    const int EJECTION_SHAKE = 3;

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

    Solution attempt = sol;
    repair(attempt, false);

    for (int k = 0; !attempt.unassigned.empty() && k < EJECTION_BUDGET; ++k) {
        randomRemoval(attempt, EJECTION_SHAKE);
        repair(attempt, false);
    }

    if (attempt.unassigned.empty())
        sol = attempt;
    else
        repair(sol, true);
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

