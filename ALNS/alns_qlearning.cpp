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

    repair_ops.push_back([](Solution& sol, bool anr) { greedyInsertion(sol, anr); });
    repair_ops.push_back([](Solution& sol, bool anr) { regret2Insertion(sol, anr); });
    repair_ops.push_back([](Solution& sol, bool anr) { regret3Insertion(sol, anr); });
    repair_ops.push_back([](Solution& sol, bool anr) { pGreedyInsertion(sol, anr); });

    // Mismo orden que destroy_ops. Solo removeSmallestRoute es un operador de
    // eliminacion de ruta propiamente dicho: vacia UNA sola ruta (la mas
    // chica), que es el unico caso en que exigir "cero rutas nuevas" es
    // alcanzable. routeRemoval vacia rutas enteras hasta juntar q clientes
    // (1-5 rutas en instancias R/RC); exigirle cero rutas nuevas lo volveria
    // infactible casi siempre y lo anularia como operador.
    destroy_is_route_elimination = {false, false, false, false, false, true};

    // Una accion por PAR (destroy, repair).
    num_actions = static_cast<int>(destroy_ops.size() * repair_ops.size());
    Q.assign(static_cast<size_t>(num_states), std::vector<double>(num_actions, 0.0));
}

int ALNS_QLearning::destroyOf(int action) const {
    return action / static_cast<int>(repair_ops.size());
}

int ALNS_QLearning::repairOf(int action) const {
    return action % static_cast<int>(repair_ops.size());
}

// Compresion logaritmica de una mejora relativa (ver nota de escala en el .h).
double ALNS_QLearning::shapeImprovement(double relative_gain) {
    if (relative_gain <= 0.0) return 0.0;
    return std::log1p(reward_gain * relative_gain);
}

int ALNS_QLearning::selectAction(int state, double epsilon) {
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    if (distr(rng) < epsilon) {
        std::uniform_int_distribution<int> act_distr(0, num_actions - 1);
        return act_distr(rng);
    }

    // Explotacion con desempate aleatorio: al inicio muchas celdas valen 0 y
    // un argmax que devuelve siempre el primer maximo sesgaria la politica
    // hacia el par de indice 0.
    const std::vector<double>& q_values = Q[state];
    double max_q = *std::max_element(q_values.begin(), q_values.end());

    std::vector<int> ties;
    for (int a = 0; a < num_actions; ++a) {
        if (q_values[a] >= max_q - 1e-12) ties.push_back(a);
    }
    std::uniform_int_distribution<int> tie_distr(0, static_cast<int>(ties.size()) - 1);
    return ties[tie_distr(rng)];
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
    start_temp = -(tuning::TEMP_SCALE * initial_d) / std::log(0.5);
    double T = start_temp;

    int n_customers = inst.clients.size() - 1;

    // Mismo grado de destruccion que ALNS clasico (misma base).
    int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
    std::uniform_int_distribution<int> q_distr(q_min, q_max);

    double curr_cost = cost(current_sol);
    double best_cost = cost(best_sol);

    double epsilon = epsilon_0;

    // Costo de oportunidad: maxima mejora registrada en la corrida. Es lo que
    // se le cobra a una iteracion que no mejoro nada, de modo que un operador
    // improductivo reciba recompensa NEGATIVA en vez de 0. Con recompensa 0
    // (el w4 del ALNS clasico) un operador que nunca funciona se queda cerca
    // del valor inicial de la tabla y sigue compitiendo con el resto.
    double opportunity_cost = 0.0;

    // Constante de normalizacion del factor e/C: pondera mas las mejoras
    // tardias, que son las dificiles, sobre las tempranas.
    const double C = static_cast<double>(max_iters);

    int state = 0; // s = 0: la iteracion previa no logro mejora
    int iters_since_best = 0;

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;
        int q = q_distr(rng);

        // Durante el learning loop inicial la accion es 100% aleatoria, para
        // poblar la tabla Q antes de empezar a explotarla.
        double eps_now = (iter <= learning_loop) ? 1.0 : epsilon;
        int action = selectAction(state, eps_now);
        int d_idx = destroyOf(action);
        int r_idx = repairOf(action);

        destroy_ops[d_idx](candidate, q);
        if (destroy_is_route_elimination[d_idx])
            repairTryEliminateRoute(candidate, repair_ops[r_idx]);
        else
            repair_ops[r_idx](candidate, true);

        // Mejoras relativas medidas ANTES de mover best/current.
        // cost() no penaliza clientes sin asignar (eso solo lo hace
        // cost_phase1). Si el repair no logro reinsertar a todos, la solucion
        // es infactible: no compite por costo y la iteracion cuenta como
        // improductiva (mejora 0 -> penalizacion por costo de oportunidad).
        double delta_global = 0.0;
        double delta_local = 0.0;
        bool improved_best = false;

        if (candidate.unassigned.empty()) {
            double cand_cost = cost(candidate);

            delta_global = std::max(0.0, (best_cost - cand_cost) / best_cost);
            delta_local  = std::max(0.0, (curr_cost - cand_cost) / curr_cost);

            if (cand_cost < best_cost) {
                best_sol = candidate;
                current_sol = candidate;
                curr_cost = cand_cost;
                best_cost = cand_cost;
                improved_best = true;
            } else if (cand_cost < curr_cost) {
                current_sol = candidate;
                curr_cost = cand_cost;
            } else if (accept(cand_cost, curr_cost, T)) {
                current_sol = candidate;
                curr_cost = cand_cost;
            }
        }

        // Recompensa continua: la magnitud de la mejora, no su categoria.
        // Esta es la diferencia de fondo con el ALNS clasico. Con scores
        // categoricos (33/13/9/0) la tabla Q converge al mismo estadistico
        // que la ruleta -- la media del score por operador -- y ambos metodos
        // terminan ordenando los operadores igual. Aqui, en cambio, una mejora
        // del 4% y una del 0.01% dejan rastros distintos en la tabla.
        double improvement = eta * shapeImprovement(delta_global)
                           + (1.0 - eta) * shapeImprovement(delta_local);
        bool improved = improvement > 0.0;

        double scale = static_cast<double>(iter) / C;
        double reward;
        if (improved) {
            reward = improvement * scale;
            opportunity_cost = std::max(opportunity_cost, improvement);
        } else {
            reward = (improvement - opportunity_cost) * scale;
        }

        int next_state = improved ? 1 : 0;

        double max_next_q = *std::max_element(Q[next_state].begin(), Q[next_state].end());
        Q[state][action] += alpha * (reward + gamma * max_next_q - Q[state][action]);

        state = next_state;

        // Reinicio por estancamiento (capa externa del bucle de dos capas):
        // tras STAGNATION_LIMIT iteraciones sin mejorar el mejor global, la
        // busqueda vuelve al mejor conocido y recalienta la temperatura. Sin
        // esto, una vez que T se enfria no hay ningun mecanismo de escape y
        // las colas largas de estancamiento se desperdician.
        iters_since_best = improved_best ? 0 : (iters_since_best + 1);
        if (tuning::STAGNATION_LIMIT > 0 && iters_since_best >= tuning::STAGNATION_LIMIT) {
            current_sol = best_sol;
            curr_cost = best_cost;
            T = start_temp * tuning::REHEAT_FACTOR;
            iters_since_best = 0;
        }

        T = T * cooling_rate;
        if (iter > learning_loop) epsilon = std::max(epsilon_min, epsilon * beta);
    }
    return best_sol;
}
