#include "operators.h"

bool evalInsertion(const Solution& sol, int client_id, const Route& route, size_t i, double& delta_cost) {
    int prev = route.path[i];
    int next = route.path[i + 1];
    const Client& u_client = sol.inst.clients[client_id];

    if (!sol.inst.is_reachable[prev][client_id]) return false;
    if (!sol.inst.is_reachable[client_id][next]) return false;

    double start_prev = std::max(route.arrival_times[i], sol.inst.clients[prev].ready_time);
    double arrival_u = start_prev + sol.inst.clients[prev].service_time + sol.inst.dist_mat[prev][client_id];

    if (arrival_u > u_client.due_date) return false;

    double start_u = std::max(arrival_u, u_client.ready_time);
    double arrival_j_new = start_u + u_client.service_time + sol.inst.dist_mat[client_id][next];
    double delay = std::max(0.0, arrival_j_new - route.arrival_times[i + 1]);
    
    if (delay > route.wait_times[i+1] + route.time_slacks[i+1]) return false;

    delta_cost = sol.inst.dist_mat[prev][client_id] + 
                 sol.inst.dist_mat[client_id][next] - 
                 sol.inst.dist_mat[prev][next];
                 
    return true;
}

namespace {

const double INF = std::numeric_limits<double>::max();

const double NOISE_RATE = 0.025;

template <typename T>
inline void swapRemove(std::vector<T>& v, size_t idx) {
    if (idx + 1 != v.size()) v[idx] = std::move(v.back());
    v.pop_back();
}

inline void commitInsertion(Solution& sol, size_t u_idx, int route_idx, int pos) {
    Route& target_route = sol.routes[route_idx];
    target_route.path.insert(target_route.path.begin() + pos, sol.unassigned[u_idx]);
    target_route.recalculate(sol.inst);
    swapRemove(sol.unassigned, u_idx);
}

struct InsEntry {
    double best = INF;
    double second = INF;
    int pos = -1;

    bool operator!=(const InsEntry& o) const {
        return best != o.best || second != o.second || pos != o.pos;
    }
};

InsEntry computeEntry(const Solution& sol, int client_id, size_t r, double max_noise) {
    InsEntry e;
    const Route& route = sol.routes[r];
    if (route.load + sol.inst.clients[client_id].demand > sol.inst.capacity) return e;

    std::uniform_real_distribution<double> noise_distr(-max_noise, max_noise);

    for (size_t i = 0; i < route.path.size() - 1; ++i) {
        double delta_cost = 0.0;
        if (!evalInsertion(sol, client_id, route, i, delta_cost)) continue;

        if (max_noise > 0.0) delta_cost = std::max(0.0, delta_cost + noise_distr(rng));

        if (delta_cost < e.best) {
            e.second = e.best;
            e.best = delta_cost;
            e.pos = i + 1;
        }
        else if (delta_cost < e.second)
            e.second = delta_cost;
    }
    return e;
}

template <typename Agg>
void cachedInsertion(Solution& sol, bool noise) {
    double max_noise = noise ? NOISE_RATE * sol.inst.max_dist : 0.0;

    std::vector<std::vector<InsEntry>> rows(sol.unassigned.size());
    std::vector<Agg> aggs(sol.unassigned.size());
    for (size_t u = 0; u < rows.size(); ++u) {
        rows[u].resize(sol.routes.size());
        for (size_t r = 0; r < sol.routes.size(); ++r)
            rows[u][r] = computeEntry(sol, sol.unassigned[u], r, max_noise);
        aggs[u].rebuild(rows[u]);
    }

    while (!sol.unassigned.empty()) {
        int best_u = -1;
        for (size_t u = 0; u < aggs.size(); ++u) {
            if (!aggs[u].valid()) continue;
            if (best_u == -1 || aggs[u].key() > aggs[best_u].key()) best_u = u;
        }

        // Flota fija: lo que no entra en ninguna ruta queda sin asignar
        if (best_u == -1) break;

        int r = aggs[best_u].route;
        commitInsertion(sol, best_u, r, rows[best_u][r].pos);
        swapRemove(rows, best_u);
        swapRemove(aggs, best_u);

        // Solo cambio la ruta r: se recalcula esa columna
        for (size_t u = 0; u < rows.size(); ++u) {
            InsEntry old = rows[u][r];
            rows[u][r] = computeEntry(sol, sol.unassigned[u], r, max_noise);
            if (rows[u][r] != old) aggs[u].update(rows[u], r, old);
        }
    }

    sol.updateMetrics();
}

struct GreedyAgg {
    double cost = INF;
    int route = -1;

    void rebuild(const std::vector<InsEntry>& row) {
        cost = INF;
        route = -1;
        for (size_t r = 0; r < row.size(); ++r) {
            if (row[r].best < cost) {
                cost = row[r].best;
                route = r;
            }
        }
    }

    void update(const std::vector<InsEntry>& row, size_t r, const InsEntry&) {
        if (static_cast<int>(r) == route) { rebuild(row); return; }

        // Otra ruta: solo importa si ahora es mejor (o empata y va antes)
        double c = row[r].best;
        if (c < cost || (c == cost && c != INF && static_cast<int>(r) < route)) {
            cost = c;
            route = r;
        }
    }

    bool valid() const { return route != -1; }
    double key() const { return -cost; }
};

struct Regret2Agg {
    double best_cost = INF;
    double second_best_cost = INF;
    double regret = -1.0;
    int route = -1;

    void rebuild(const std::vector<InsEntry>& row) {
        best_cost = INF;
        second_best_cost = INF;
        route = -1;

        for (size_t r = 0; r < row.size(); ++r) {
            for (double delta_cost : {row[r].best, row[r].second}) {
                if (delta_cost == INF) break;

                if (delta_cost < best_cost) {
                    second_best_cost = best_cost;
                    best_cost = delta_cost;
                    route = r;
                }
                else if (delta_cost < second_best_cost)
                    second_best_cost = delta_cost;
            }
        }

        if (route != -1)
            regret = (second_best_cost == INF) ? 1e9 : (second_best_cost - best_cost);
    }

    void update(const std::vector<InsEntry>& row, size_t r, const InsEntry& old) {
        if (static_cast<int>(r) != route && old.best > second_best_cost && row[r].best > second_best_cost) return;
        rebuild(row);
    }

    bool valid() const { return route != -1; }
    double key() const { return regret; }
};

// Regret-k sobre el mejor costo por ruta; K = 0 es regret-m (todas las rutas)
template <int K>
struct RegretAgg {
    double regret = -1.0;
    int route = -1;

    double kth = INF;

    void rebuild(const std::vector<InsEntry>& row) {
        static thread_local std::vector<double> costs;
        costs.clear();
        route = -1;
        kth = INF;

        for (size_t r = 0; r < row.size(); ++r) {
            if (row[r].best == INF) continue;
            if (route == -1 || row[r].best < row[route].best) route = r;
            costs.push_back(row[r].best);
        }
        if (costs.empty()) return;

        std::sort(costs.begin(), costs.end());

        int k = K > 0 ? K : static_cast<int>(row.size());
        int m = std::min(k, static_cast<int>(costs.size()));

        int missing_routes = k - m;
        regret = missing_routes * 10000.0;
        for (int j = 1; j < m; ++j)
            regret += (costs[j] - costs[0]);

        if (m == k) kth = costs[k - 1];
    }

    void update(const std::vector<InsEntry>& row, size_t r, const InsEntry& old) {
        if (row[r].best == old.best) return;

        // Una ruta fuera de las k mejores, antes y despues, no cambia el regret
        if (old.best > kth && row[r].best > kth) return;
        rebuild(row);
    }

    bool valid() const { return route != -1; }
    double key() const { return regret; }
};

}

void greedyInsertion(Solution& sol, bool noise){
    cachedInsertion<GreedyAgg>(sol, noise);
}

void regret2Insertion(Solution& sol, bool noise){
    cachedInsertion<Regret2Agg>(sol, noise);
}

void regret3Insertion(Solution& sol, bool noise){
    cachedInsertion<RegretAgg<3>>(sol, noise);
}

void regret4Insertion(Solution& sol, bool noise){
    cachedInsertion<RegretAgg<4>>(sol, noise);
}

void regretMInsertion(Solution& sol, bool noise){
    cachedInsertion<RegretAgg<0>>(sol, noise);
}
