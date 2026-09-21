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
    destroy_ops.push_back([](Solution& sol, int q) { removeSmallestRoute(sol); });

    repair_ops.push_back([](Solution& sol) { greedyInsertion(sol); });
    repair_ops.push_back([](Solution& sol) { regret2Insertion(sol); });
    repair_ops.push_back([](Solution& sol) { regret3Insertion(sol); });
    repair_ops.push_back([](Solution& sol) { pGreedyInsertion(sol); });

    Q_table_D.assign(num_states, std::vector<double>(destroy_ops.size(), 0.0));
    Q_table_R.assign(num_states, std::vector<double>(repair_ops.size(), 0.0));
    visits_D.assign(num_states, std::vector<int>(destroy_ops.size(), 0));
    visits_R.assign(num_states, std::vector<int>(repair_ops.size(), 0));
}

int ALNS_QLearning::computeState(int phase, int stagnation_level) {
    return phase * num_stagnation + stagnation_level;
}

double ALNS_QLearning::learningRate(int visits) const {
    int capped = std::min(visits, alpha_cap);
    return std::max(alpha_min, 1.0 / (1.0 + capped));
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

// Identico al criterio SA de ALNS clasico (misma cost(), mismo peso de
// vehiculos): la unica diferencia entre ambos algoritmos debe ser el
// criterio de seleccion de operadores, no el de aceptacion.
bool ALNS_QLearning::accept(double cand_cost, double curr_cost, double current_temp) {
    double delta = cand_cost - curr_cost;
    if (delta <= 0) return true;

    double prob = std::exp(-delta / current_temp);
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    return distr(rng) < prob;
}

Solution ALNS_QLearning::solve(int max_iters, bool save_metrics) {
    double initial_d = current_sol.total_distance;
    start_temp = -(0.10 * initial_d) / std::log(0.5);
    double T = start_temp;

    // Piso de exploracion de la ruleta clasica (destroy_weights/repair_weights
    // nunca bajan de 0.01): reservamos un epsilon minimo comparable para que la
    // explotacion greedy nunca colapse del todo, incluso en corridas largas.
    double epsilon = 1.0;
    double epsilon_min = 0.05;
    // La ventana de decaimiento se ata a max_iters (no a un valor fijo de
    // iteracion) para que el punto de la corrida en que se llega al piso de
    // exploracion sea proporcional al presupuesto de iteraciones, igual que el
    // enfriamiento del SA ya escala con 'cooling_rate' sobre toda la corrida.
    double epsilon_decay = std::pow(epsilon_min, 1.0 / (0.5 * max_iters));

    int n_customers = inst.clients.size() - 1;

    // Mismo grado de destruccion que ALNS clasico (misma base).
    int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
    std::uniform_int_distribution<int> q_distr(q_min, q_max);

    double curr_cost = cost(current_sol);
    double best_cost = cost(best_sol);
    int best_vehicles = best_sol.used_vehicles;

    int iters_since_improvement = 0;
    int current_state = computeState(0, 0);

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;
        int q = q_distr(rng);

        // Unico punto de diferencia real con ALNS clasico: el operador se elige
        // por epsilon-greedy sobre Q-valores en vez de ruleta sobre pesos.
        int d_idx = selectOp(Q_table_D[current_state], epsilon);
        int r_idx = selectOp(Q_table_R[current_state], epsilon);

        destroy_ops[d_idx](candidate, q);
        repair_ops[r_idx](candidate);

        // Recompensa: mismas categorias base que ALNS clasico, mas un nivel
        // extra (w0) cuando la mejora reduce vehiculos, para que el agente
        // distinga explicitamente el evento de mayor prioridad (menos
        // vehiculos) de una simple reduccion de distancia con la misma flota.
        double reward = w4;
        bool improved_best = false;

        // cost() no penaliza clientes sin asignar (eso solo lo hace
        // cost_phase1). Si el repair no logro reinsertar a todos, la
        // solucion es infactible y no debe competir por costo: se trata
        // igual que cualquier rechazo (reward=w4).
        if (candidate.unassigned.empty()) {
            double cand_cost = cost(candidate);

            if (cand_cost < best_cost) {
                bool fewer_vehicles = candidate.used_vehicles < best_vehicles;
                best_sol = candidate;
                current_sol = candidate;
                curr_cost = cand_cost;
                best_cost = cand_cost;
                best_vehicles = candidate.used_vehicles;
                reward = fewer_vehicles ? w0 : w1;
                improved_best = true;
            } else if (cand_cost < curr_cost) {
                current_sol = candidate;
                curr_cost = cand_cost;
                reward = w2;
            } else if (accept(cand_cost, curr_cost, T)) {
                current_sol = candidate;
                curr_cost = cand_cost;
                reward = w3;
            }
            // else: rechazada, reward = w4
        }

        iters_since_improvement = improved_best ? 0 : (iters_since_improvement + 1);

        // Fase de busqueda por fraccion de iteraciones transcurridas (0..1),
        // en 3 tercios; estancamiento en 3 niveles por iteraciones sin mejorar
        // el mejor global. Ambos son independientes de 'reward', a diferencia
        // del estado de la version anterior (que era una recodificacion 1:1
        // del propio reward y anulaba el termino de bootstrap gamma*max_Q).
        double frac = static_cast<double>(iter) / max_iters;
        int phase = frac < (1.0 / 3.0) ? 0 : (frac < (2.0 / 3.0) ? 1 : 2);
        int stagnation_level = iters_since_improvement < 50 ? 0 : (iters_since_improvement < 300 ? 1 : 2);
        int next_state = computeState(phase, stagnation_level);

        double max_next_q_D = *std::max_element(Q_table_D[next_state].begin(), Q_table_D[next_state].end());
        double max_next_q_R = *std::max_element(Q_table_R[next_state].begin(), Q_table_R[next_state].end());

        int& vd = visits_D[current_state][d_idx];
        int& vr = visits_R[current_state][r_idx];
        double alpha_d = learningRate(vd++);
        double alpha_r = learningRate(vr++);

        Q_table_D[current_state][d_idx] += alpha_d * (reward + gamma * max_next_q_D - Q_table_D[current_state][d_idx]);
        Q_table_R[current_state][r_idx] += alpha_r * (reward + gamma * max_next_q_R - Q_table_R[current_state][r_idx]);

        current_state = next_state;

        T = T * cooling_rate;
        epsilon = std::max(epsilon_min, epsilon * epsilon_decay);
    }
    return best_sol;
}