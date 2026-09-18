#include "alns_qlearning.h"
#include <iostream>
#include <fstream>

extern std::mt19937 rng;

ALNS_QLearning::ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol)
    : inst(_inst), current_sol(_initial_sol), best_sol(_initial_sol) {
    initOps();
}

void ALNS_QLearning::initOps() {
    destroy_ops.push_back(randomRemoval);
    destroy_ops.push_back(routeRemoval);
    destroy_ops.push_back([](Solution& sol, int q) { worstRemoval(sol, q); });
    destroy_ops.push_back([](Solution& sol, int q) { shawRemoval(sol, q); });
    destroy_ops.push_back([](Solution& sol, int q) { timeWindowRemoval(sol, q); });

    repair_ops.push_back(greedyInsertion);
    repair_ops.push_back(regret2Insertion);
    repair_ops.push_back(regret3Insertion);
    repair_ops.push_back([](Solution& sol) { pGreedyInsertion(sol); });

    Q_table_D.assign(num_states, std::vector<double>(destroy_ops.size(), 0.0));
    Q_table_R.assign(num_states, std::vector<double>(repair_ops.size(), 0.0));
}

int ALNS_QLearning::selectOp(const std::vector<double>& q_values, double epsilon) {
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    if (distr(rng) < epsilon) {
        std::uniform_int_distribution<int> act_distr(0, q_values.size() - 1);
        return act_distr(rng);
    } else {
        auto it = std::max_element(q_values.begin(), q_values.end());
        return std::distance(q_values.begin(), it);
    }
}

double ALNS_QLearning::objectiveScore(int vehicles, double distance) const {
    return (vehicles * W_VEH) + (100.0 * distance / ref_dist);
}

double ALNS_QLearning::improvement(int ref_veh, double ref_val, const Solution& cand) const {
    return objectiveScore(ref_veh, ref_val) -
           objectiveScore(cand.used_vehicles, cand.total_distance);
}

Outcome ALNS_QLearning::evaluateCandidate(const Solution& cand, double T) const {
    double cand_cost = cost(cand);

    if (cand_cost < cost(best_sol)) return Outcome::NEW_BEST;
    if (cand_cost < cost(current_sol)) return Outcome::IMPROVED;

    std::uniform_real_distribution<double> distr(0.0, 1.0);
    int d_veh = cand.used_vehicles - current_sol.used_vehicles;

    if (d_veh == 0) {
        double delta = cand.total_distance - current_sol.total_distance;
        if (delta <= 0.0) return Outcome::ACCEPTED_SA;
        if (distr(rng) < std::exp(-delta / T)) return Outcome::ACCEPTED_SA;
        return Outcome::REJECTED;
    }

    double p = (P_VEH_INCREASE * (T / start_temp)) / static_cast<double>(d_veh);
    if (distr(rng) < p) return Outcome::ACCEPTED_SA;
    return Outcome::REJECTED;
}

Solution ALNS_QLearning::solve(int max_iters, bool save_history) {
    double initial_d = current_sol.total_distance;

    ref_dist = std::max(initial_d, 1.0);
    start_temp = -(START_TEMP_FRAC * initial_d) / std::log(0.5);
    double T = start_temp;

    cooling_rate = std::pow(T_END_RATIO, 1.0 / std::max(1, max_iters));

    double epsilon = 1.0;
    double epsilon_min = 0.15;
    double eps_horizon = std::max(1.0, EPS_DECAY_FRAC * max_iters);
    double epsilon_decay = std::pow(epsilon_min, 1.0 / eps_horizon);

    double opportunity_cost = 0.0;

    int current_state = 0;
    int n_customers = inst.clients.size() - 1;

    if (save_history) history.reserve(max_iters);

    const int Q_FLOOR_CAP = 30;
    const int Q_MAX_CAP   = 60;
    int q_min = std::min(Q_FLOOR_CAP, std::max(4, static_cast<int>(0.10 * n_customers)));
    int q_max = std::min(Q_MAX_CAP,   std::max(q_min + 1, static_cast<int>(0.40 * n_customers)));
    q_max = std::max(q_max, q_min + 1);
    std::uniform_int_distribution<int> q_distr(q_min, q_max);

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;
        int q = q_distr(rng);

        int d_idx = selectOp(Q_table_D[current_state], epsilon);
        int r_idx = selectOp(Q_table_R[current_state], epsilon);

        destroy_ops[d_idx](candidate, q);
        repair_ops[r_idx](candidate);

        // --- 1. Que hizo realmente el ALNS con este candidato ---
        Outcome outcome = evaluateCandidate(candidate, T);

        // --- 2. Magnitud de la mejora, en puntos objetivo ---
        double g = std::max(improvement(best_sol.used_vehicles, best_sol.total_distance, candidate), 0.0);
        double l = improvement(current_sol.used_vehicles, current_sol.total_distance, candidate);
        if (l < 0.0) l *= LAMBDA_WORSE;

        double delta_improvement = (eta * g) + ((1.0 - eta) * l);

        // --- 3. Recompensa = magnitud + bono por evento - coste de oportunidad ---
        double reward = delta_improvement;
        bool improved = (outcome == Outcome::NEW_BEST || outcome == Outcome::IMPROVED);

        switch (outcome) {
            case Outcome::NEW_BEST:    reward += R_NEW_BEST;  break;
            case Outcome::IMPROVED:    reward += R_IMPROVED;  break;
            case Outcome::ACCEPTED_SA: reward += R_ACCEPTED;  break;
            case Outcome::REJECTED:    reward += R_REJECTED;  break;
        }

        if (!improved) reward -= OC_WEIGHT * opportunity_cost;

        reward = std::max(-R_CLIP, std::min(R_CLIP, reward));

        opportunity_cost = ((1.0 - OC_RATE) * opportunity_cost) +
                           (OC_RATE * std::max(delta_improvement, 0.0));

        // --- 4. Aplicar el movimiento ---
        if (outcome == Outcome::NEW_BEST) {
            best_sol = candidate;
            current_sol = candidate;
        } else if (outcome != Outcome::REJECTED) {
            current_sol = candidate;
        }

        // --- 5. Update Q-Learning (2 estados, 2 tablas) ---
        int next_state = improved ? 1 : 0;

        double max_next_q_D = *std::max_element(Q_table_D[next_state].begin(), Q_table_D[next_state].end());
        double max_next_q_R = *std::max_element(Q_table_R[next_state].begin(), Q_table_R[next_state].end());

        Q_table_D[current_state][d_idx] += alpha * (reward + gamma * max_next_q_D - Q_table_D[current_state][d_idx]);
        Q_table_R[current_state][r_idx] += alpha * (reward + gamma * max_next_q_R - Q_table_R[current_state][r_idx]);

        if (save_history) {
            IterationDataQL data;
            data.iter = iter;
            data.best_vehicles = best_sol.used_vehicles;
            data.best_distance = best_sol.total_distance;
            data.curr_vehicles = current_sol.used_vehicles;
            data.curr_distance = current_sol.total_distance;
            data.d_idx = d_idx;
            data.r_idx = r_idx;
            data.reward = reward;
            data.temp = T;
            data.epsilon = epsilon;
            data.outcome = static_cast<int>(outcome);
            data.opportunity_cost = opportunity_cost;
            history.emplace_back(data);
        }

        current_state = next_state;
        T = T * cooling_rate;
        epsilon = std::max(epsilon_min, epsilon * epsilon_decay);
    }

    return best_sol;
}

void ALNS_QLearning::exportMetrics(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error al abrir archivo para metricas: " << filename << std::endl;
        return;
    }
    file << "iter,best_veh,best_dist,curr_veh,curr_dist,d_op,r_op,reward,temp,epsilon,outcome,opp_cost\n";
    for (const auto& data : history) {
        file << data.iter << "," << data.best_vehicles << "," << data.best_distance << ","
             << data.curr_vehicles << "," << data.curr_distance << "," << data.d_idx << ","
             << data.r_idx << "," << data.reward << "," << data.temp << "," << data.epsilon << ","
             << data.outcome << "," << data.opportunity_cost << "\n";
    }
    file.close();
    std::cout << "-> Metricas exportadas a " << filename << "\n";
}