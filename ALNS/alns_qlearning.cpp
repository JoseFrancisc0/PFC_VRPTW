#include "alns_qlearning.h"

namespace {

const double IMPROVEMENT_DECAY = 0.9;

}

ALNS_QLearning::ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params)
    : ALNS(_initial_sol, _params),
      Q(NUM_STATES, std::vector<double>(NUM_DESTROY * NUM_REPAIR, 0.0)),
      explore_until(_params.learning_loop),
      recent_improvement(NUM_DESTROY * NUM_REPAIR, 0.0) {}

// La fase 2 tiene otro objetivo: se olvidan las mejoras de la fase 1 y se vuelve a explorar
void ALNS_QLearning::onDistancePhase(int iter) {
    std::fill(recent_improvement.begin(), recent_improvement.end(), 0.0);
    epsilon = 1.0;
    explore_until = iter + params.learning_loop;
}

double ALNS_QLearning::shapeImprovement(double relative_gain) const {
    if (relative_gain <= 0.0) return 0.0;
    return std::log1p(params.reward_gain * relative_gain);
}

std::pair<int, int> ALNS_QLearning::selectOperators(int iter) {
    const std::vector<double>& q_values = Q[state];
    int num_actions = q_values.size();
    double eps_now = (iter <= explore_until) ? 1.0 : epsilon;

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
    if (improved)
        reward = improvement * scale;
    else {
        // Costo de oportunidad: la mayor mejora reciente entre las acciones no elegidas
        double opportunity_cost = 0.0;
        for (int a = 0; a < static_cast<int>(recent_improvement.size()); ++a)
            if (a != action) opportunity_cost = std::max(opportunity_cost, recent_improvement[a]);

        reward = (improvement - opportunity_cost) * scale;
    }

    recent_improvement[action] = (IMPROVEMENT_DECAY * recent_improvement[action]) + ((1.0 - IMPROVEMENT_DECAY) * improvement);

    int next_state = improved ? 1 : 0;
    double max_next_q = *std::max_element(Q[next_state].begin(), Q[next_state].end());
    Q[state][action] += params.alpha * (reward + params.gamma * max_next_q - Q[state][action]);
    state = next_state;

    if (iter > explore_until) epsilon = std::max(params.epsilon_min, epsilon * params.beta);
}
