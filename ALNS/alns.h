#ifndef ALNS_H
#define ALNS_H

#include <utility>
#include "../Operators/operators.h"
#include "../Utils/params.h"

struct Outcome {
    bool new_best = false; 
    bool improved = false;  
    bool accepted = false; 
    double gain_best = 0.0;  
    double gain_current = 0.0; 
};

class ALNS {
    public:
        ALNS(const Solution& _initial_sol, const SolverParams& _params);
        virtual ~ALNS() = default;

        Solution solve(int _max_iters);

    protected:
        SolverParams params;
        int max_iters = 0;

        virtual std::pair<int, int> selectOperators(int iter) = 0;

        virtual void learn(int iter, const Outcome& outcome) = 0;

    private:
        Solution current_sol;
        Solution best_sol;

        bool accept(double cand_cost, double curr_cost, double temp);
};

#endif //ALNS_H
