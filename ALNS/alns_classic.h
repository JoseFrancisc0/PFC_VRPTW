#ifndef ALNS_CLASSIC_H
#define ALNS_CLASSIC_H

#include <vector>
#include "alns.h"

class ALNS_Classic : public ALNS {
    public:
        ALNS_Classic(const Solution& _initial_sol, const SolverParams& _params);

    private:
        Roulette destroy;
        Roulette repair;
        int d_idx = 0;
        int r_idx = 0;

        Move selectOperators(int iter) override;
        void learn(int iter, const Outcome& outcome) override;
};

#endif //ALNS_CLASSIC_H
