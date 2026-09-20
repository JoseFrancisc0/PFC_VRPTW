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

    // Mismo esquema de exploracion decreciente; piso mas bajo que antes (0.15)
    // porque ahora la recompensa es estable y conviene explotar mas al final.
    double epsilon = 1.0;
    double epsilon_decay = 0.998;
    double epsilon_min = 0.05;

    int current_state = 0;
    int n_customers = inst.clients.size() - 1;

    // Mismo grado de destruccion que ALNS clasico (misma base).
    int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
    std::uniform_int_distribution<int> q_distr(q_min, q_max);

    double curr_cost = cost(current_sol);
    double best_cost = cost(best_sol);

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;
        int q = q_distr(rng);

        // Unico punto de diferencia real con ALNS clasico: el operador se elige
        // por epsilon-greedy sobre Q-valores en vez de ruleta sobre pesos.
        int d_idx = selectOp(Q_table_D[current_state], epsilon);
        int r_idx = selectOp(Q_table_R[current_state], epsilon);

        destroy_ops[d_idx](candidate, q);
        repair_ops[r_idx](candidate);

        // Mismas categorias de score que ALNS clasico (w1..w4), usadas aqui
        // directamente como recompensa: acotada y estacionaria a lo largo de
        // la corrida (a diferencia de la version anterior, que escalaba con
        // 'iter' y rompia la convergencia de Q-learning).
        double score = w4;
        int next_state = 0; // rechazada (por defecto, incluye candidato infactible)

        // cost() no penaliza clientes sin asignar (eso solo lo hace
        // cost_phase1). Si el repair no logro reinsertar a todos, la
        // solucion es infactible y no debe competir por costo (si no,
        // parece "mas barata" al faltarle clientes y se acepta, perdiendo
        // clientes en cascada). Se trata igual que cualquier rechazo:
        // score=w4/estado 0, y el Q-update de abajo sigue corriendo normal.
        if (candidate.unassigned.empty()) {
            double cand_cost = cost(candidate);

            if (cand_cost < best_cost) {
                best_sol = candidate;
                current_sol = candidate;
                curr_cost = cand_cost;
                best_cost = cand_cost;
                score = w1;
                next_state = 2; // nuevo mejor global
            } else if (cand_cost < curr_cost) {
                current_sol = candidate;
                curr_cost = cand_cost;
                score = w2;
                next_state = 1; // aceptada (mejora local)
            } else if (accept(cand_cost, curr_cost, T)) {
                current_sol = candidate;
                curr_cost = cand_cost;
                score = w3;
                next_state = 1; // aceptada (SA)
            }
            // else: rechazada, score = w4, next_state = 0
        }

        double reward = score;

        double max_next_q_D = *std::max_element(Q_table_D[next_state].begin(), Q_table_D[next_state].end());
        double max_next_q_R = *std::max_element(Q_table_R[next_state].begin(), Q_table_R[next_state].end());

        Q_table_D[current_state][d_idx] += alpha * (reward + gamma * max_next_q_D - Q_table_D[current_state][d_idx]);
        Q_table_R[current_state][r_idx] += alpha * (reward + gamma * max_next_q_R - Q_table_R[current_state][r_idx]);

        current_state = next_state;

        T = T * cooling_rate;
        epsilon = std::max(epsilon_min, epsilon * epsilon_decay);
    }
    return best_sol;
}