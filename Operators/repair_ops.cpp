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

// ---------------------------------------------------------------------------
// Cache de inserciones
//
// Los repair insertan un cliente por vuelta. Entre una vuelta y la siguiente
// solo cambia UNA ruta (la que recibio al cliente), asi que las evaluaciones
// de los demas pendientes contra el resto de las rutas siguen valiendo. El
// cache guarda, por cada (pendiente, ruta), el resultado de recorrer todas las
// posiciones de esa ruta, y tras cada insercion recalcula solo la columna de
// la ruta modificada.
//
// Es una optimizacion pura: cada repair elige exactamente al mismo cliente, en
// la misma ruta y posicion, que la version que reevaluaba todo en cada vuelta
// (mismos desempates y mismo consumo del generador aleatorio).
// ---------------------------------------------------------------------------
namespace {

const double INF = std::numeric_limits<double>::max();

// Se puede intentar insertar el cliente en esta ruta?
inline bool routeOpenFor(const Solution& sol, const Client& u_client, const Route& route, bool allow_new_routes) {
    // Una ruta vacia es, en los hechos, una ruta nueva: si el repair tiene
    // prohibido abrirlas tampoco puede rellenar los cascarones que hayan
    // quedado de iteraciones anteriores.
    if (!allow_new_routes && route.path.size() <= 2) return false;
    if (route.load + u_client.demand > sol.inst.capacity) return false;
    return true;
}

// Quita el elemento idx igual que se quita de sol.unassigned (el ultimo ocupa
// su lugar), para que los vectores auxiliares sigan paralelos a el.
template <typename T>
inline void swapRemove(std::vector<T>& v, size_t idx) {
    if (idx + 1 != v.size()) v[idx] = std::move(v.back());
    v.pop_back();
}

// Inserta el pendiente u_idx en (route_idx, pos) y lo saca de los pendientes.
inline void commitInsertion(Solution& sol, size_t u_idx, int route_idx, int pos) {
    Route& target_route = sol.routes[route_idx];
    target_route.path.insert(target_route.path.begin() + pos, sol.unassigned[u_idx]);
    target_route.recalculate(sol.inst);
    swapRemove(sol.unassigned, u_idx);
}

// Nadie entra en ninguna ruta: abre una ruta nueva si se puede. Devuelve false
// si el repair debe terminar.
inline bool openNewRoute(Solution& sol, bool allow_new_routes) {
    // Sin permiso para abrir rutas nuevas, lo que no entra queda sin asignar y
    // el solver descartara el candidato. Es el desenlace esperado de un
    // intento fallido de eliminar una ruta, no una anomalia, asi que no se
    // reporta por stderr.
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

// Resumen de insertar un cliente en una ruta: las dos desviaciones mas bajas
// entre todas sus posiciones factibles y la posicion de la mejor. INF = no hay.
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

// Esqueleto comun de greedy / regret-2 / regret-3. rows[u][r] es el InsEntry
// del pendiente sol.unassigned[u] en la ruta r. 'Agg' resume la fila de un
// pendiente, que es lo que el repair compara entre pendientes:
//   rebuild(row)          recalcula el resumen desde la fila completa
//   update(row, r, old)   la entrada de la ruta r cambio (antes valia 'old')
//   valid()               el pendiente tiene alguna insercion factible
//   key()                 valor a maximizar entre pendientes (gana el primero)
//   route                 ruta elegida para ese pendiente
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

// Greedy: la insercion mas barata del pendiente.
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

// Regret-2: diferencia entre las dos mejores posiciones del pendiente.
struct Regret2Agg {
    double best_cost = INF;
    double second_best_cost = INF;
    double regret = -1.0;
    int route = -1;

    void rebuild(const std::vector<InsEntry>& row) {
        best_cost = INF;
        second_best_cost = INF;
        route = -1;

        // Las dos mejores de cada ruta alcanzan para las dos mejores globales
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
        // Una ruta que estaba y sigue estrictamente por encima de la segunda
        // mejor posicion no participa del regret.
        if (static_cast<int>(r) != route && old.best > second_best_cost && row[r].best > second_best_cost) return;
        rebuild(row);
    }

    bool valid() const { return route != -1; }
    double key() const { return regret; }
};

// Regret-3: suma de las diferencias entre las 3 mejores rutas del pendiente.
struct Regret3Agg {
    double regret = -1.0;
    int route = -1;

    // Tres costos mas bajos y cuantas rutas son factibles: permiten saber
    // cuando un cambio en una ruta no puede alterar el resultado.
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
        if (new_cost == old_cost) return; // solo se movio la posicion

        // Si el minimo es unico y la ruta r estaba y sigue estrictamente por
        // encima del tercer costo, ni el podio ni la ruta elegida cambian.
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

// Posicion factible de un pendiente (para el greedy con ruido, que necesita
// todas las posiciones y no solo la mejor de cada ruta).
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

} // namespace

// Insertamos lo que minimiza el aumento inmediato de costo
void greedyInsertion(Solution& sol, bool allow_new_routes){
    cachedInsertion<GreedyAgg>(sol, allow_new_routes);
}

// Greedy anticipando la segunda mejor posicion del cliente
void regret2Insertion(Solution& sol, bool allow_new_routes){
    cachedInsertion<Regret2Agg>(sol, allow_new_routes);
}

// Regret-2 pero para las m=3 mejores rutas
void regret3Insertion(Solution& sol, bool allow_new_routes){
    cachedInsertion<Regret3Agg>(sol, allow_new_routes);
}

// Greedy con ruido
void pGreedyInsertion(Solution& sol, bool allow_new_routes, double eta){
    std::uniform_real_distribution<double> noise_distr(1.0 - eta, 1.0 + eta);

    // rows[u] = posiciones factibles del pendiente u, ordenadas por (ruta, pos).
    // El ruido se sortea de nuevo en cada vuelta para cada posicion factible,
    // asi que lo unico que se guarda es la factibilidad y la desviacion.
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

            // Solo cambio una ruta: se reemplaza su tramo en cada fila
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
            sol.routes.back().recalculate(sol.inst);
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
            sol.routes.back().recalculate(sol.inst);
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
            sol.routes.back().recalculate(sol.inst);
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
            sol.routes.back().recalculate(sol.inst);
        }
    }
    sol.updateMetrics();
}
