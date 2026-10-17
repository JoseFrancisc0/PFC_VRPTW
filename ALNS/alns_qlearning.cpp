#include "alns_qlearning.h"

namespace {

// Fraccion de los episodios de la fase en la que epsilon alcanza epsilon_min
const double EXPLOIT_FROM = 0.5;

// Recompensa normalizada de Kou et al. (2026): mejora / media movil, recortada a 1
const double NO_GAIN_REWARD = -0.5;
const double BASELINE_DECAY = 0.9;

}

// Acciones: a < NUM_DESTROY usa el destructor a; a >= NUM_DESTROY, el mismo destructor tras perturbar
ALNS_QLearning::ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params)
    : ALNS(_initial_sol, _params),
      Q(NUM_STATES, std::vector<double>(2 * NUM_DESTROY, 0.0)),
      episode_start(_initial_sol) {}

// La fase 2 tiene sus propios estados: se descarta el episodio a medias y se vuelve a explorar
void ALNS_QLearning::onDistancePhase(int) {
    phase = 1;
    state = 2;
    decisions = 0;
    episode_step = 0;
    baseline = 0.0;
}

int ALNS_QLearning::chooseAction() {
    int episodes = std::max(1, phase_iters / params.episode_len);
    int explore_until = static_cast<int>(std::ceil(params.learning_loop * episodes));
    int exploit_from = std::max(explore_until + 1, static_cast<int>(EXPLOIT_FROM * episodes));

    // epsilon = 1 en el bucle de aprendizaje; luego decae exponencialmente hasta epsilon_min
    double progress = static_cast<double>(decisions - explore_until) / (exploit_from - explore_until);
    double eps_now = decisions < explore_until ? 1.0
                   : std::max(params.epsilon_min, std::pow(params.epsilon_min, progress));
    ++decisions;

    const std::vector<double>& q_values = Q[state];
    int num_actions = q_values.size();

    std::uniform_real_distribution<double> distr(0.0, 1.0);
    if (distr(rng) < eps_now) {
        std::uniform_int_distribution<int> act_distr(0, num_actions - 1);
        return act_distr(rng);
    }

    double max_q = *std::max_element(q_values.begin(), q_values.end());

    std::vector<int> ties;
    for (int a = 0; a < num_actions; ++a) {
        if (q_values[a] >= max_q - 1e-12) ties.push_back(a);
    }
    std::uniform_int_distribution<int> tie_distr(0, static_cast<int>(ties.size()) - 1);
    return ties[tie_distr(rng)];
}

Move ALNS_QLearning::selectOperators(int) {
    if (episode_step == 0) {
        action = chooseAction();
        episode_start = currentSolution();
        episode_gain_best = 0.0;
    }

    std::uniform_int_distribution<int> repair_distr(0, NUM_REPAIR - 1);
    return {action % NUM_DESTROY, repair_distr(rng), episode_step == 0 && action >= NUM_DESTROY};
}

double ALNS_QLearning::reward(double improvement) {
    if (improvement <= 0.0) return NO_GAIN_REWARD;

    double r = baseline > 0.0 ? std::min(improvement / baseline, 1.0) : 1.0;
    baseline = baseline > 0.0 ? (BASELINE_DECAY * baseline) + ((1.0 - BASELINE_DECAY) * improvement) : improvement;
    return r;
}

void ALNS_QLearning::learn(int, const Outcome& outcome) {
    // La mejora global se acumula por iteracion: asi cuenta la ruta eliminada aunque la etapa reinicie
    episode_gain_best += outcome.gain_best;
    if (++episode_step < params.episode_len) return;
    episode_step = 0;

    // La mejora local es neta: una perturbacion no se premia por recuperar lo que ella misma empeoro
    double gain_current = relativeGain(episode_start, currentSolution());
    double improvement = params.eta * episode_gain_best + (1.0 - params.eta) * gain_current;

    int next_state = 2 * phase + (episode_gain_best > 0.0 ? 1 : 0);
    double max_next_q = *std::max_element(Q[next_state].begin(), Q[next_state].end());
    Q[state][action] += params.alpha * (reward(improvement) + params.gamma * max_next_q - Q[state][action]);
    state = next_state;
}
