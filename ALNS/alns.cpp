#include "alns.h"

namespace {
const double TEMP_SCALE   = 0.2;
const double COOLING_RATE = 0.9995;

const int    STAGNATION_LIMIT = 1500;
const double REHEAT_FACTOR    = 0.5;

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
    max_iters = _max_iters;

    const double start_temp = -(TEMP_SCALE * current_sol.total_distance) / std::log(0.5);
    double T = start_temp;

    double curr_cost = cost(current_sol);
    double best_cost = cost(best_sol);

    int iters_since_best = 0;

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;

        auto [d_idx, r_idx] = selectOperators(iter);
        applyOperators(candidate, d_idx, r_idx);

        Outcome outcome;
        if (candidate.unassigned.empty()) {
            double cand_cost = cost(candidate);

            outcome.gain_best    = std::max(0.0, (best_cost - cand_cost) / best_cost);
            outcome.gain_current = std::max(0.0, (curr_cost - cand_cost) / curr_cost);
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
        }

        iters_since_best = outcome.new_best ? 0 : (iters_since_best + 1);
        if (iters_since_best >= STAGNATION_LIMIT) {
            current_sol = best_sol;
            curr_cost = best_cost;
            T = start_temp * REHEAT_FACTOR;
            iters_since_best = 0;
        }

        learn(iter, outcome);

        T = T * COOLING_RATE;
    }

    return best_sol;
}
