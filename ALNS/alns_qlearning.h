#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include "alns.h"

class ALNS_QLearning : public ALNS {
    public:
        ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params);

    private:
        static const int NUM_STATES = 2;

        std::vector<std::vector<double>> Q;
        int state = 0;
        int action = 0;
        double epsilon = 1.0;

        int explore_until;

        std::vector<double> recent_improvement;

        std::pair<int, int> selectOperators(int iter) override;
        void learn(int iter, const Outcome& outcome) override;
        void onDistancePhase(int iter) override;
        double shapeImprovement(double relative_gain) const;
};

#endif //ALNS_QLEARNING_H
