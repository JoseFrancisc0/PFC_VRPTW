#include "alns_classic.h"

namespace {

const double W_NEW_BEST = 33.0;
const double W_IMPROVED = 13.0;
const double W_ACCEPTED = 9.0;
const double W_REJECTED = 0.0;

const int    SEGMENT_SIZE = 100;
const double DECAY        = 0.9;
const double MIN_WEIGHT   = 0.01;

} 

ALNS_Classic::ALNS_Classic(const Solution& _initial_sol, const SolverParams& _params)
    : ALNS(_initial_sol, _params), destroy(NUM_DESTROY), repair(NUM_REPAIR) {}

int ALNS_Classic::Roulette::select() {
    std::discrete_distribution<int> distr(weights.begin(), weights.end());
    return distr(rng);
}

void ALNS_Classic::Roulette::addScore(int idx, double score) {
    scores[idx] += score;
    uses[idx]++;
}

void ALNS_Classic::Roulette::closeSegment() {
    for (size_t j = 0; j < weights.size(); ++j) {
        if (uses[j] > 0) {
            double avg_score = scores[j] / uses[j];
            weights[j] = (DECAY * weights[j]) + ((1.0 - DECAY) * avg_score);
            if (weights[j] < MIN_WEIGHT) weights[j] = MIN_WEIGHT;
        }
        scores[j] = 0.0;
        uses[j] = 0;
    }
}

std::pair<int, int> ALNS_Classic::selectOperators(int) {
    d_idx = destroy.select();
    r_idx = repair.select();
    return {d_idx, r_idx};
}

void ALNS_Classic::learn(int iter, const Outcome& outcome) {
    double score = outcome.new_best ? W_NEW_BEST
                 : outcome.improved ? W_IMPROVED
                 : outcome.accepted ? W_ACCEPTED
                 : W_REJECTED;

    destroy.addScore(d_idx, score);
    repair.addScore(r_idx, score);

    if (iter % SEGMENT_SIZE == 0) {
        destroy.closeSegment();
        repair.closeSegment();
    }
}
