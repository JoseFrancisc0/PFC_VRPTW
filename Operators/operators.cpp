#include "operators.h"

namespace {

const DestroyOp DESTROY_OPS[] = {
    randomRemoval,
    routeRemoval,
    worstRemoval,
    shawRemoval,
    timeWindowRemoval,
};

const RepairOp REPAIR_OPS[] = {
    greedyInsertion,
    regret2Insertion,
    regret3Insertion,
    pGreedyInsertion,
};

const int NUM_Q_DESTROY = std::size(DESTROY_OPS);

const double Q_MIN_RATIO = 0.10;
const double Q_MAX_RATIO = 0.40;
const int    Q_MIN_FLOOR = 4;

}

const int NUM_DESTROY = NUM_Q_DESTROY + 1;
const int NUM_REPAIR  = std::size(REPAIR_OPS);

void applyOperators(Solution& sol, int destroy_idx, int repair_idx) {
    int n_customers = sol.inst.clients.size() - 1;
    int q_min = std::max(Q_MIN_FLOOR, static_cast<int>(Q_MIN_RATIO * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(Q_MAX_RATIO * n_customers));

    bool elimination = (destroy_idx == NUM_Q_DESTROY);
    RepairOp repair = REPAIR_OPS[repair_idx];

    std::uniform_int_distribution<int> q_distr(q_min, elimination ? q_min : q_max);
    int q = q_distr(rng);

    if (elimination)
        smallestRouteElimination(sol, repair);
    else {
        DESTROY_OPS[destroy_idx](sol, q);
        repair(sol, true);
    }
}
