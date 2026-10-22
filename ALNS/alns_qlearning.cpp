#include "alns_qlearning.h"

namespace {

// Fraccion de phase_iters / episode_len en la que epsilon alcanza epsilon_min, igual en ambas fases.
// La fase 1 suele cortarse antes de phase_iters (en Solomon, a ~100 de 500 episodios): un horizonte
// relativo a su duracion maxima la dejaba explorando al azar casi entera
const double EXPLOIT_FROM = 0.15;

// Exploracion pura minima: cada accion se prueba unas veces antes de explotar (12 en fase 1, 6 en fase 2)
const int MIN_TRIES_PER_ACTION = 3;

// Recompensa normalizada de Kou et al. (2026): mejora / media movil. El tope 2 (antes 1) conserva
// la diferencia entre mejoras grandes y pequenas; sin mejora la recompensa es 0, la misma para todas.
// Excepto la perturbacion fallida: se acepta a la fuerza y deja peor la solucion. Sin este castigo
// el agente la usaba de mas en la fase 1 y la flota final empeoraba (r109: 11.8 vs 11.1 vehiculos)
const double NO_GAIN_REWARD      = 0.0;
const double PERTURB_FAIL_REWARD = -0.5;
const double REWARD_CAP     = 2.0;
const double BASELINE_DECAY = 0.9;

}

// Acciones: a < NUM_DESTROY usa el destructor a; a >= NUM_DESTROY, el mismo destructor tras perturbar.
// La perturbacion se acepta siempre, por eso solo se ofrece en la fase 1 (rutas)
ALNS_QLearning::ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params)
    : ALNS(_initial_sol, _params),
      Q(NUM_STATES, std::vector<double>(2 * NUM_DESTROY, 0.0)),
      episode_start(_initial_sol), repair(NUM_REPAIR) {}

// La fase 2 tiene sus propios estados: se descarta el episodio a medias y se vuelve a explorar
void ALNS_QLearning::onDistancePhase(int) {
    phase = 1;
    state = 2;
    decisions = 0;
    episode_step = 0;
    baseline = 0.0;
}

int ALNS_QLearning::numActions() const {
    return (phase == 0 && params.perturb) ? 2 * NUM_DESTROY : NUM_DESTROY;
}

int ALNS_QLearning::chooseAction() {
    const std::vector<double>& q_values = Q[state];
    int num_actions = numActions();

    int episodes = std::max(1, phase_iters / params.episode_len);
    int explore_until = std::max(static_cast<int>(std::ceil(params.learning_loop * episodes)),
                                 MIN_TRIES_PER_ACTION * num_actions);
    int exploit_from = std::max(explore_until + 1, static_cast<int>(EXPLOIT_FROM * episodes));

    // epsilon = 1 en el bucle de aprendizaje; luego decae exponencialmente hasta epsilon_min
    double progress = static_cast<double>(decisions - explore_until) / (exploit_from - explore_until);
    double eps_now = decisions < explore_until ? 1.0
                   : std::max(params.epsilon_min, std::pow(params.epsilon_min, progress));
    ++decisions;

    double max_q = *std::max_element(q_values.begin(), q_values.begin() + num_actions);

    // Softmax (Boltzmann) tras el bucle de aprendizaje: cada accion con probabilidad creciente en Q.
    // Conserva la mezcla de operadores en lugar de fijar casi siempre el de mayor Q
    if (params.softmax_tau > 0.0 && decisions > explore_until) {
        std::vector<double> w(num_actions);
        for (int a = 0; a < num_actions; ++a) w[a] = std::exp((q_values[a] - max_q) / params.softmax_tau);
        std::discrete_distribution<int> soft(w.begin(), w.end());
        return soft(rng);
    }

    std::uniform_real_distribution<double> distr(0.0, 1.0);
    if (distr(rng) < eps_now) {
        std::uniform_int_distribution<int> act_distr(0, num_actions - 1);
        return act_distr(rng);
    }

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
        episode_gain_local = 0.0;
    }

    r_idx = repair.select();
    return {action % NUM_DESTROY, r_idx, episode_step == 0 && action >= NUM_DESTROY};
}

double ALNS_QLearning::reward(double improvement) {
    if (improvement <= 0.0) return action >= NUM_DESTROY ? PERTURB_FAIL_REWARD : NO_GAIN_REWARD;

    double r = baseline > 0.0 ? std::min(improvement / baseline, REWARD_CAP) : 1.0;
    baseline = baseline > 0.0 ? (BASELINE_DECAY * baseline) + ((1.0 - BASELINE_DECAY) * improvement) : improvement;
    return r;
}

void ALNS_QLearning::learn(int iter, const Outcome& outcome) {
    repair.record(r_idx, outcome);
    if (iter % Roulette::SEGMENT_SIZE == 0) repair.closeSegment();

    // La mejora global se acumula por iteracion: asi cuenta la ruta eliminada aunque la etapa reinicie
    episode_gain_best += outcome.gain_best;

    // La mejora local es la mejor alcanzada en el episodio respecto de su inicio, no la del final:
    // la solucion final depende de que empeoramientos acepto el recocido, no del operador.
    // Se mide contra el inicio, asi una perturbacion no se premia por recuperar lo que ella empeoro
    episode_gain_local = std::max(episode_gain_local, relativeGain(episode_start, currentSolution()));

    if (++episode_step < params.episode_len) return;
    episode_step = 0;

    double improvement = params.eta * episode_gain_best + (1.0 - params.eta) * episode_gain_local;

    int next_state = 2 * phase + (improvement > 0.0 ? 1 : 0);
    int next_actions = numActions();
    double max_next_q = *std::max_element(Q[next_state].begin(), Q[next_state].begin() + next_actions);
    Q[state][action] += params.alpha * (reward(improvement) + params.gamma * max_next_q - Q[state][action]);
    state = next_state;
}
