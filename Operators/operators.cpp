#include "operators.h"

namespace {

const DestroyOp DESTROY_OPS[] = {
    randomRemoval,
    routeRemoval,
    worstRemoval,
    shawRemoval,
    clusterRemoval,
    timeWindowRemoval,
};

const RepairOp REPAIR_OPS[] = {
    greedyInsertion,
    regret2Insertion,
    regret3Insertion,
    regret4Insertion,
    regretMInsertion,
};

// Grado de destruccion de Pisinger & Ropke (2007): entre min(0.1n, 30) y min(0.4n, 60)
const double Q_MIN_RATIO = 0.10;
const double Q_MAX_RATIO = 0.40;
const int    Q_MIN_CAP   = 30;
const int    Q_MAX_CAP   = 60;
const int    Q_MIN_FLOOR = 4;

const double NOISE_PROB = 0.5;

}

const int NUM_DESTROY = std::size(DESTROY_OPS);
const int NUM_REPAIR  = std::size(REPAIR_OPS);

void applyOperators(Solution& sol, int destroy_idx, int repair_idx) {
    int n_customers = sol.inst.clients.size() - 1;
    int q_min = std::max(Q_MIN_FLOOR, std::min(Q_MIN_CAP, static_cast<int>(Q_MIN_RATIO * n_customers)));
    int q_max = std::max(q_min + 1, std::min(Q_MAX_CAP, static_cast<int>(Q_MAX_RATIO * n_customers)));

    std::uniform_int_distribution<int> q_distr(q_min, q_max);
    int q = q_distr(rng);

    std::bernoulli_distribution noise_distr(NOISE_PROB);

    DESTROY_OPS[destroy_idx](sol, q);
    REPAIR_OPS[repair_idx](sol, noise_distr(rng));
}
