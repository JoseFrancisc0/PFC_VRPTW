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

const double NOISE = 0.1;

struct RouteInsertion {
    int route_idx;
    int insert_pos;
    double cost;
};

inline bool routeOpenFor(const Solution& sol, const Client& u_client, const Route& route, bool allow_new_routes) {
    if (!allow_new_routes && route.path.size() <= 2) return false;
    if (route.load + u_client.demand > sol.inst.capacity) return false;
    return true;
}

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

inline bool openNewRoute(Solution& sol, bool allow_new_routes) {
    if (!allow_new_routes) return false;

    if (!sol.routes.empty() && sol.routes.back().path.size() <= 2) {
        std::cerr << "[!] " << sol.unassigned.size()
                  << " clientes no pudieron ser insertados de forma factible. \n";
        return false;
    }

    sol.routes.push_back(Route());
    sol.routes.back().recalculate(sol.inst);
    return true;
}

struct InsEntry {
    double best = INF;
    double second = INF;
    int pos = -1;

    bool operator!=(const InsEntry& o) const {
        return best != o.best || second != o.second || pos != o.pos;
    }
};

InsEntry computeEntry(const Solution& sol, int client_id, size_t r, bool allow_new_routes) {
    InsEntry e;
    const Route& route = sol.routes[r];
    if (!routeOpenFor(sol, sol.inst.clients[client_id], route, allow_new_routes)) return e;

    for (size_t i = 0; i < route.path.size() - 1; ++i) {
        double delta_cost = 0.0;
        if (!evalInsertion(sol, client_id, route, i, delta_cost)) continue;

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
void cachedInsertion(Solution& sol, bool allow_new_routes) {
    std::vector<std::vector<InsEntry>> rows(sol.unassigned.size());
    std::vector<Agg> aggs(sol.unassigned.size());
    for (size_t u = 0; u < rows.size(); ++u) {
        rows[u].resize(sol.routes.size());
        for (size_t r = 0; r < sol.routes.size(); ++r)
            rows[u][r] = computeEntry(sol, sol.unassigned[u], r, allow_new_routes);
        aggs[u].rebuild(rows[u]);
    }

    while (!sol.unassigned.empty()) {
        int best_u = -1;
        for (size_t u = 0; u < aggs.size(); ++u) {
            if (!aggs[u].valid()) continue;
            if (best_u == -1 || aggs[u].key() > aggs[best_u].key()) best_u = u;
        }

        if (best_u != -1) {
            int r = aggs[best_u].route;
            commitInsertion(sol, best_u, r, rows[best_u][r].pos);
            swapRemove(rows, best_u);
            swapRemove(aggs, best_u);

            // Solo cambio la ruta r: se recalcula esa columna
            for (size_t u = 0; u < rows.size(); ++u) {
                InsEntry old = rows[u][r];
                rows[u][r] = computeEntry(sol, sol.unassigned[u], r, allow_new_routes);
                if (rows[u][r] != old) aggs[u].update(rows[u], r, old);
            }
        }
        else {
            if (!openNewRoute(sol, allow_new_routes)) break;

            size_t r = sol.routes.size() - 1;
            for (size_t u = 0; u < rows.size(); ++u) {
                rows[u].push_back(computeEntry(sol, sol.unassigned[u], r, allow_new_routes));
                aggs[u].rebuild(rows[u]);
            }
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

struct Regret3Agg {
    double regret = -1.0;
    int route = -1;

    double top[3] = {INF, INF, INF};
    int count = 0;

    void rebuild(const std::vector<InsEntry>& row) {
        static thread_local std::vector<RouteInsertion> best_per_route;
        best_per_route.clear();
        for (size_t r = 0; r < row.size(); ++r)
            if (row[r].best != INF)
                best_per_route.push_back({static_cast<int>(r), row[r].pos, row[r].best});

        count = best_per_route.size();
        route = -1;
        top[0] = top[1] = top[2] = INF;
        if (best_per_route.empty()) return;

        std::sort(best_per_route.begin(), best_per_route.end(),
            [](const RouteInsertion& a, const RouteInsertion& b) {
                return a.cost < b.cost;
            });

        double best_cost = best_per_route[0].cost;
        regret = 0.0;

        int m = std::min(3, count);
        for (int k = 1; k < m; ++k)
            regret += (best_per_route[k].cost - best_cost);

        int missing_routes = 3 - m;
        regret += missing_routes * 10000.0;

        route = best_per_route[0].route_idx;
        for (int k = 0; k < m; ++k) top[k] = best_per_route[k].cost;
    }

    void update(const std::vector<InsEntry>& row, size_t r, const InsEntry& old) {
        double old_cost = old.best;
        double new_cost = row[r].best;
        if (new_cost == old_cost) return;

        if (count >= 3 && top[0] < top[1] && old_cost > top[2] && new_cost > top[2]) {
            int new_count = count - (old_cost != INF) + (new_cost != INF);
            if (new_count >= 3) {
                count = new_count;
                return;
            }
        }
        rebuild(row);
    }

    bool valid() const { return route != -1; }
    double key() const { return regret; }
};

struct NoisyCand {
    int route;
    int pos;
    double delta;
};

void appendCands(const Solution& sol, int client_id, size_t r, bool allow_new_routes, std::vector<NoisyCand>& out) {
    const Route& route = sol.routes[r];
    if (!routeOpenFor(sol, sol.inst.clients[client_id], route, allow_new_routes)) return;

    for (size_t i = 0; i < route.path.size() - 1; ++i) {
        double delta_cost = 0.0;
        if (!evalInsertion(sol, client_id, route, i, delta_cost)) continue;
        out.push_back({static_cast<int>(r), static_cast<int>(i + 1), delta_cost});
    }
}

} 

void greedyInsertion(Solution& sol, bool allow_new_routes){
    cachedInsertion<GreedyAgg>(sol, allow_new_routes);
}

void regret2Insertion(Solution& sol, bool allow_new_routes){
    cachedInsertion<Regret2Agg>(sol, allow_new_routes);
}

void regret3Insertion(Solution& sol, bool allow_new_routes){
    cachedInsertion<Regret3Agg>(sol, allow_new_routes);
}

void pGreedyInsertion(Solution& sol, bool allow_new_routes){
    std::uniform_real_distribution<double> noise_distr(1.0 - NOISE, 1.0 + NOISE);

    std::vector<std::vector<NoisyCand>> rows(sol.unassigned.size());
    for (size_t u = 0; u < rows.size(); ++u)
        for (size_t r = 0; r < sol.routes.size(); ++r)
            appendCands(sol, sol.unassigned[u], r, allow_new_routes, rows[u]);

    std::vector<NoisyCand> fresh;

    while (!sol.unassigned.empty()) {
        double best_cost = INF;
        int best_client_idx_in_unassigned = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;

        for (size_t u_idx = 0; u_idx < rows.size(); ++u_idx) {
            for (const NoisyCand& c : rows[u_idx]) {
                double perturbed_cost = c.delta * noise_distr(rng);
                if (perturbed_cost < best_cost) {
                    best_cost = perturbed_cost;
                    best_client_idx_in_unassigned = u_idx;
                    best_route_idx = c.route;
                    best_insert_pos = c.pos;
                }
            }
        }

        if (best_client_idx_in_unassigned != -1) {
            commitInsertion(sol, best_client_idx_in_unassigned, best_route_idx, best_insert_pos);
            swapRemove(rows, best_client_idx_in_unassigned);

            for (size_t u = 0; u < rows.size(); ++u) {
                std::vector<NoisyCand>& row = rows[u];
                auto lo = std::lower_bound(row.begin(), row.end(), best_route_idx,
                    [](const NoisyCand& c, int r) { return c.route < r; });
                auto hi = std::upper_bound(lo, row.end(), best_route_idx,
                    [](int r, const NoisyCand& c) { return r < c.route; });

                fresh.clear();
                appendCands(sol, sol.unassigned[u], best_route_idx, allow_new_routes, fresh);

                size_t at = lo - row.begin();
                row.erase(lo, hi);
                row.insert(row.begin() + at, fresh.begin(), fresh.end());
            }
        }
        else {
            if (!openNewRoute(sol, allow_new_routes)) break;

            size_t r = sol.routes.size() - 1;
            for (size_t u = 0; u < rows.size(); ++u)
                appendCands(sol, sol.unassigned[u], r, allow_new_routes, rows[u]);
        }
    }

    sol.updateMetrics();
}
