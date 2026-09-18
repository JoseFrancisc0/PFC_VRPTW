#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <deque>
#include "../Operators/operators.h"

enum class Outcome {
    REJECTED     = 0,
    ACCEPTED_SA  = 1,
    IMPROVED     = 2,
    NEW_BEST     = 3
};

struct IterationDataQL {
    int iter;
    int best_vehicles;
    double best_distance;
    int curr_vehicles;
    double curr_distance;
    int d_idx;
    int r_idx;
    double reward;
    double temp;
    double epsilon;
    int outcome;
    double opportunity_cost;
};

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&)>;

class ALNS_QLearning {
    public:
        ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters, bool save_history = false);
        void exportMetrics(const std::string& filename);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;
        std::vector<IterationDataQL> history;

        std::vector<DestroyOp> destroy_ops;
        std::vector<RepairOp> repair_ops;

        double start_temp = 0.0;
        double cooling_rate = 0.9998;
        double T_END_RATIO = 0.002;
        double START_TEMP_FRAC = 0.10;

        double P_VEH_INCREASE = 0.0;

        double alpha = 0.05;
        double gamma = 0.8;

        double EPS_DECAY_FRAC = 0.30;

        int num_states = 2;

        std::vector<std::vector<double>> Q_table_D;
        std::vector<std::vector<double>> Q_table_R;

        double ref_dist = 1.0;
        double W_VEH = 10.0;
        double eta = 0.6;
        double LAMBDA_WORSE = 0.25;
        double R_CLIP = 20.0;
        double OC_RATE = 0.005;
        double OC_WEIGHT = 0.5;

        double R_NEW_BEST = 3.0;
        double R_IMPROVED = 1.0;
        double R_ACCEPTED = 0.3;
        double R_REJECTED = -0.3;

        void initOps();
        int selectOp(const std::vector<double>& q_values, double epsilon);

        double objectiveScore(int vehicles, double distance) const;
        double improvement(int ref_veh, double ref_val, const Solution& cand) const;
        Outcome evaluateCandidate(const Solution& cand, double T) const;
};

#endif