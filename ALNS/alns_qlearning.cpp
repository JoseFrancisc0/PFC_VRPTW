#include "alns_qlearning.h"

ALNS_QLearning::ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params)
    : ALNS(_initial_sol, _params),
      Q(NUM_STATES, std::vector<double>(NUM_DESTROY * NUM_REPAIR, 0.0)) {}

double ALNS_QLearning::shapeImprovement(double relative_gain) const {
    if (relative_gain <= 0.0) return 0.0;
    return std::log1p(params.reward_gain * relative_gain);
}

std::pair<int, int> ALNS_QLearning::selectOperators(int iter) {
    const std::vector<double>& q_values = Q[state];
    int num_actions = q_values.size();
    double eps_now = (iter <= params.learning_loop) ? 1.0 : epsilon;

    std::uniform_real_distribution<double> distr(0.0, 1.0);
    if (distr(rng) < eps_now) {
        std::uniform_int_distribution<int> act_distr(0, num_actions - 1);
        action = act_distr(rng);
    }
    else {

        double max_q = *std::max_element(q_values.begin(), q_values.end());

        std::vector<int> ties;
        for (int a = 0; a < num_actions; ++a) {
            if (q_values[a] >= max_q - 1e-12) ties.push_back(a);
        }
        std::uniform_int_distribution<int> tie_distr(0, static_cast<int>(ties.size()) - 1);
        action = ties[tie_distr(rng)];
    }

    return {action / NUM_REPAIR, action % NUM_REPAIR};
}

void ALNS_QLearning::learn(int iter, const Outcome& outcome) {
    double improvement = params.eta * shapeImprovement(outcome.gain_best)
                       + (1.0 - params.eta) * shapeImprovement(outcome.gain_current);
    bool improved = improvement > 0.0;

    double scale = static_cast<double>(iter) / static_cast<double>(max_iters);
    double reward;
    if (improved) {
        reward = improvement * scale;
        opportunity_cost = std::max(opportunity_cost, improvement);
    }
    else
        reward = (improvement - opportunity_cost) * scale;

    int next_state = improved ? 1 : 0;
    double max_next_q = *std::max_element(Q[next_state].begin(), Q[next_state].end());
    Q[state][action] += params.alpha * (reward + params.gamma * max_next_q - Q[state][action]);
    state = next_state;

    if (iter > params.learning_loop) epsilon = std::max(params.epsilon_min, epsilon * params.beta);
}
