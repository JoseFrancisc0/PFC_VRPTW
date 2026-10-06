#ifndef ALNS_CLASSIC_H
#define ALNS_CLASSIC_H

#include <vector>
#include "alns.h"

class ALNS_Classic : public ALNS {
    public:
        ALNS_Classic(const Solution& _initial_sol, const SolverParams& _params);

    private:
        struct Roulette {
            std::vector<double> weights;
            std::vector<double> scores;
            std::vector<int> uses;

            explicit Roulette(int n) : weights(n, 1.0), scores(n, 0.0), uses(n, 0) {}
            int select();
            void addScore(int idx, double score);
            void closeSegment();
        };

        Roulette destroy;
        Roulette repair;
        int d_idx = 0;
        int r_idx = 0;

        std::pair<int, int> selectOperators(int iter) override;
        void learn(int iter, const Outcome& outcome) override;
};

#endif //ALNS_CLASSIC_H
