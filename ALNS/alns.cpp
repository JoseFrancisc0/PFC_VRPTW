#include "alns.h"

namespace {
const double TEMP_SCALE   = 0.2;
const double COOLING_RATE = 0.9995;

const int    STAGNATION_LIMIT = 1500;
const double REHEAT_FACTOR    = 0.5;

// Fase 1 (minimizacion de rutas): criterio de corte de Ropke & Pisinger (2006)
const int    ROUTE_PHASE_STALL = 2000;
const int    ROUTE_PHASE_BANK  = 5;

int vehicleLowerBound(const Instance& inst) {
    double total_demand = 0.0;
    for (const Client& c : inst.clients) total_demand += c.demand;
    return static_cast<int>(std::ceil(total_demand / inst.capacity));
}

// Mejora relativa de cand sobre ref en el primer criterio en que difieren:
// clientes sin asignar, vehiculos o distancia (el mismo orden que impone cost())
double relativeGain(const Solution& ref, const Solution& cand) {
    if (cand.unassigned.size() != ref.unassigned.size())
        return std::max(0.0, (static_cast<double>(ref.unassigned.size()) - cand.unassigned.size()) / ref.unassigned.size());

    if (cand.used_vehicles != ref.used_vehicles)
        return std::max(0.0, static_cast<double>(ref.used_vehicles - cand.used_vehicles) / ref.used_vehicles);

    return std::max(0.0, (ref.total_distance - cand.total_distance) / ref.total_distance);
}

// Quita la ruta con menos clientes y los deja sin asignar
void removeSmallestRoute(Solution& sol) {
    std::vector<Route>& routes = sol.routes;
    routes.erase(std::remove_if(routes.begin(), routes.end(),
        [](const Route& r) { return r.path.size() <= 2; }), routes.end());

    auto smallest = std::min_element(routes.begin(), routes.end(),
        [](const Route& a, const Route& b) { return a.path.size() < b.path.size(); });

    sol.unassigned.insert(sol.unassigned.end(), smallest->path.begin() + 1, smallest->path.end() - 1);
    routes.erase(smallest);
    sol.updateMetrics();
}

}

ALNS::ALNS(const Solution& _initial_sol, const SolverParams& _params)
    : params(_params), current_sol(_initial_sol), best_sol(_initial_sol) {}

bool ALNS::accept(double cand_cost, double curr_cost, double temp) {
    double delta = cand_cost - curr_cost;
    if (delta <= 0) return true;

    double prob = std::exp(-delta / temp);
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    return distr(rng) < prob;
}

Solution ALNS::solve(int _max_iters) {
    // La fase 1 usa a lo sumo _max_iters iteraciones; la fase 2, siempre _max_iters
    const int route_phase_limit = _max_iters;
    max_iters = 2 * _max_iters;

    const double start_temp = -(TEMP_SCALE * current_sol.total_distance) / std::log(0.5);
    const int min_vehicles = vehicleLowerBound(current_sol.inst);

    // Ultima solucion completa: la fase 2 parte de ella
    Solution feasible_sol = current_sol;
    bool route_phase = true;

    double T, curr_cost, best_cost;
    int iters_since_best, iters_since_bank;

    // Fase 1: arranca de feasible_sol con una ruta menos. Fase 2: de feasible_sol tal cual.
    auto startStage = [&](int iter) {
        route_phase = route_phase && iter < route_phase_limit && feasible_sol.used_vehicles > min_vehicles;
        if (!route_phase) {
            max_iters = iter + _max_iters;
            onDistancePhase(iter);
        }

        current_sol = feasible_sol;
        if (route_phase) removeSmallestRoute(current_sol);
        best_sol = current_sol;

        curr_cost = best_cost = cost(current_sol);
        T = start_temp;
        iters_since_best = 0;
        iters_since_bank = 0;
    };

    startStage(0);

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;

        auto [d_idx, r_idx] = selectOperators(iter);
        applyOperators(candidate, d_idx, r_idx);

        double cand_cost = cost(candidate);
        size_t prev_bank = best_sol.unassigned.size();

        Outcome outcome;
        outcome.gain_best    = relativeGain(best_sol, candidate);
        outcome.gain_current = relativeGain(current_sol, candidate);
        outcome.new_best = cand_cost < best_cost;
        outcome.improved = outcome.new_best || cand_cost < curr_cost;
        outcome.accepted = outcome.improved || accept(cand_cost, curr_cost, T);

        if (outcome.accepted) {
            current_sol = candidate;
            curr_cost = cand_cost;
        }
        if (outcome.new_best) {
            best_sol = candidate;
            best_cost = cand_cost;
        }

        iters_since_best = outcome.new_best ? 0 : (iters_since_best + 1);
        if (iters_since_best >= STAGNATION_LIMIT) {
            current_sol = best_sol;
            curr_cost = best_cost;
            // Solo la fase 1 recalienta: en la fase 2 impediria que la temperatura baje
            if (route_phase) T = start_temp * REHEAT_FACTOR;
            iters_since_best = 0;
        }

        learn(iter, outcome);

        T = T * COOLING_RATE;

        if (!route_phase) continue;

        size_t bank = best_sol.unassigned.size();
        iters_since_bank = bank < prev_bank ? 0 : (iters_since_bank + 1);

        if (bank == 0) {
            // Ruta eliminada: se intenta con la siguiente
            feasible_sol = best_sol;
            startStage(iter);
        }
        else if (iter >= route_phase_limit ||
                 (bank >= ROUTE_PHASE_BANK && iters_since_bank >= ROUTE_PHASE_STALL)) {
            route_phase = false;
            startStage(iter);
        }
    }

    return best_sol;
}
