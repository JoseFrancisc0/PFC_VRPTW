#include "alns_qlearning.h"

namespace {

// Fraccion de cada fase en la que epsilon llega a epsilon_min. Se mide sobre phase_iters, no sobre la
// duracion real: la fase 1 suele cortarse antes y un horizonte relativo a ella la dejaria casi toda al azar
const double EXPLOIT_FROM = 0.15;

// Recompensa normalizada de Kou et al. (2026): mejora / media movil de las mejoras, con tope.
// Sin mejora la recompensa es 0
const double REWARD_CAP     = 2.0;
const double BASELINE_DECAY = 0.9;

}

int QTable::select(int state, double epsilon) {
    const std::vector<double>& values = q[state];
    int num_actions = static_cast<int>(values.size());

    std::uniform_real_distribution<double> distr(0.0, 1.0);
    if (distr(rng) < epsilon) {
        std::uniform_int_distribution<int> act_distr(0, num_actions - 1);
        return act_distr(rng);
    }

    double max_q = *std::max_element(values.begin(), values.end());
    std::vector<int> ties;
    for (int a = 0; a < num_actions; ++a) {
        if (values[a] >= max_q - 1e-12) ties.push_back(a);
    }
    std::uniform_int_distribution<int> tie_distr(0, static_cast<int>(ties.size()) - 1);
    return ties[tie_distr(rng)];
}

void QTable::record(int action, double reward) {
    rewards[action] += reward;
    uses[action]++;
}

void QTable::closeSegment(int state, int next_state, double alpha, double gamma) {
    double max_next = *std::max_element(q[next_state].begin(), q[next_state].end());
    for (size_t a = 0; a < uses.size(); ++a) {
        if (uses[a] > 0) {
            double avg_reward = rewards[a] / uses[a];
            q[state][a] += alpha * (avg_reward + (gamma * max_next) - q[state][a]);
        }
        rewards[a] = 0.0;
        uses[a] = 0;
    }
}

ALNS_QLearning::ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params)
    : ALNS(_initial_sol, _params), destroy(NUM_STATES, NUM_DESTROY), repair(NUM_STATES, NUM_REPAIR) {}

// El cambio de fase cierra el segmento en curso (su transicion lleva al primer estado de la fase 2).
// Despues el agente vuelve a explorar y reinicia la media de mejoras
void ALNS_QLearning::onDistancePhase(int iter) {
    closeSegment(2);
    phase = 1;
    phase_start = iter;
    baseline = 0.0;
}

void ALNS_QLearning::closeSegment(int next_state) {
    destroy.closeSegment(state, next_state, params.alpha, params.gamma);
    repair.closeSegment(state, next_state, params.alpha, params.gamma);
    state = next_state;
    found_best = false;
}

// Igual en ambas fases: 1 durante la exploracion inicial (learning_loop de la fase) y luego decae
// exponencialmente hasta epsilon_min, al que llega en EXPLOIT_FROM de la fase
double ALNS_QLearning::epsilon(int iter) const {
    int t = iter - phase_start - 1;
    double explore_until = params.learning_loop * phase_iters;
    double exploit_from = std::max(explore_until + 1.0, EXPLOIT_FROM * phase_iters);

    if (t < explore_until) return 1.0;
    double progress = (t - explore_until) / (exploit_from - explore_until);
    return std::max(params.epsilon_min, std::pow(params.epsilon_min, progress));
}

Move ALNS_QLearning::selectOperators(int iter) {
    double eps = epsilon(iter);
    d_idx = destroy.select(state, eps);
    r_idx = repair.select(state, eps);
    return {d_idx, r_idx};
}

double ALNS_QLearning::reward(double improvement) {
    if (improvement <= 0.0) return 0.0;

    double r = baseline > 0.0 ? std::min(improvement / baseline, REWARD_CAP) : 1.0;
    baseline = baseline > 0.0 ? (BASELINE_DECAY * baseline) + ((1.0 - BASELINE_DECAY) * improvement) : improvement;
    return r;
}

void ALNS_QLearning::learn(int iter, const Outcome& outcome) {
    double r = reward(params.eta * outcome.gain_best + (1.0 - params.eta) * outcome.gain_current);
    destroy.record(d_idx, r);
    repair.record(r_idx, r);
    found_best = found_best || outcome.new_best;

    if (iter % Roulette::SEGMENT_SIZE == 0) closeSegment(2 * phase + (found_best ? 1 : 0));
}
