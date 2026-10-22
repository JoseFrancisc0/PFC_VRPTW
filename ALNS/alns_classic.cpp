#include "alns_classic.h"

ALNS_Classic::ALNS_Classic(const Solution& _initial_sol, const SolverParams& _params)
    : ALNS(_initial_sol, _params), destroy(NUM_DESTROY), repair(NUM_REPAIR) {}

Move ALNS_Classic::selectOperators(int) {
    d_idx = destroy.select();
    r_idx = repair.select();
    return {d_idx, r_idx, false};
}

void ALNS_Classic::learn(int iter, const Outcome& outcome) {
    destroy.record(d_idx, outcome);
    repair.record(r_idx, outcome);

    if (iter % Roulette::SEGMENT_SIZE == 0) {
        destroy.closeSegment();
        repair.closeSegment();
    }
}
