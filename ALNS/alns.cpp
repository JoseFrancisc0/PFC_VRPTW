#include "alns.h"

ALNS::ALNS(const Instance& _inst, const Solution& _initial_sol) 
    : inst(_inst), current_sol(_initial_sol), best_sol(_initial_sol) {
    initOps();
}

void ALNS::initOps() {
    destroy_ops.push_back(randomRemoval);
    destroy_ops.push_back(routeRemoval);
    destroy_ops.push_back([](Solution& sol, int q) {
        worstRemoval(sol, q);
    });
    destroy_ops.push_back([](Solution& sol, int q) {
        shawRemoval(sol, q);
    });
    destroy_ops.push_back([](Solution& sol, int q) {
        timeWindowRemoval(sol, q);
    });
    destroy_ops.push_back([](Solution& sol, int q) {
        removeSmallestRoute(sol);
    });

    repair_ops.push_back([](Solution& sol) { greedyInsertion(sol); });
    repair_ops.push_back([](Solution& sol) { regret2Insertion(sol); });
    repair_ops.push_back([](Solution& sol) { regret3Insertion(sol); });
    repair_ops.push_back([](Solution& sol) {
        pGreedyInsertion(sol);
    });

    int num_destroy = destroy_ops.size();
    destroy_weights.assign(num_destroy, 1.0);
    destroy_scores.assign(num_destroy, 0.0);
    destroy_uses.assign(num_destroy, 0);

    int num_repair = repair_ops.size();
    repair_weights.assign(num_repair, 1.0);
    repair_scores.assign(num_repair, 0.0);
    repair_uses.assign(num_repair, 0);
}

int ALNS::selectDestroyOp() {
    std::discrete_distribution<int> distr(destroy_weights.begin(), destroy_weights.end());
    int selected_idx = distr(rng);
    return selected_idx;
}

int ALNS::selectRepairOp() {
    std::discrete_distribution<int> distr(repair_weights.begin(), repair_weights.end());
    int selected_idx = distr(rng);
    return selected_idx;
}

bool ALNS::accept(double cand_cost, double curr_cost, double current_temp) {
    double delta = cand_cost - curr_cost;
    if (delta <= 0) return true;
    
    double prob  = std::exp(-delta / current_temp);
    std::uniform_real_distribution<double> distr(0.0, 1.0);
    double random_val = distr(rng);

    // Aceptada por el simulated annealing?
    if (random_val < prob) return true;

    return false; // Rechazada
}

// Acumula el score del operador usado en esta iteracion (no actualiza pesos todavia).
void ALNS::registerScore(int used_destroy_idx, int used_repair_idx, double score) {
    destroy_scores[used_destroy_idx] += score;
    destroy_uses[used_destroy_idx]++;

    repair_scores[used_repair_idx] += score;
    repair_uses[used_repair_idx]++;
}

// Cierre de segmento (Ropke & Pisinger): rho = lambda*rho + (1-lambda)*(pi/theta).
// Si un operador no se uso en el segmento, su peso no se toca.
void ALNS::updateWeightsSegment() {
    for (size_t j = 0; j < destroy_weights.size(); ++j) {
        if (destroy_uses[j] > 0) {
            double avg_score = destroy_scores[j] / destroy_uses[j];
            destroy_weights[j] = (decay * destroy_weights[j]) + ((1.0 - decay) * avg_score);
            if (destroy_weights[j] < 0.01) destroy_weights[j] = 0.01;
        }
        destroy_scores[j] = 0.0;
        destroy_uses[j] = 0;
    }

    for (size_t j = 0; j < repair_weights.size(); ++j) {
        if (repair_uses[j] > 0) {
            double avg_score = repair_scores[j] / repair_uses[j];
            repair_weights[j] = (decay * repair_weights[j]) + ((1.0 - decay) * avg_score);
            if (repair_weights[j] < 0.01) repair_weights[j] = 0.01;
        }
        repair_scores[j] = 0.0;
        repair_uses[j] = 0;
    }
}

Solution ALNS::solve(int max_iters) {
    double initial_d = current_sol.total_distance;
    start_temp = -(0.10 * initial_d) / std::log(0.5);
    double T = start_temp;
    int n_customers = inst.clients.size() - 1;

    int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));
    std::uniform_int_distribution<int> q_distr(q_min, q_max);

    double curr_cost = cost(current_sol);
    double best_cost = cost(best_sol);

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;
        int q = q_distr(rng); // Grado de destruccion (cuantos clientes se busca eliminar)

        // Seleccion de operadores
        int d_idx = selectDestroyOp();
        int r_idx = selectRepairOp();

        // r(d(x))
        destroy_ops[d_idx](candidate, q);
        repair_ops[r_idx](candidate);

        // Evaluacion y scores
        double score = w4; // por defecto, incluye candidato infactible

        // cost() no penaliza clientes sin asignar (solo lo hace cost_phase1,
        // usado en otra fase). Si el repair no logro reinsertar a todos
        // (posible cuando el destroy elimina muchos clientes de golpe, p.ej.
        // routeRemoval/removeSmallestRoute en instancias con capacidad/TW
        // ajustados), la solucion es infactible y no debe competir por
        // costo: se trata igual que cualquier rechazo (score=w4). De lo
        // contrario cost() la ve mas barata (le faltan clientes) y la
        // acepta, perdiendo clientes en cascada iteracion tras iteracion.
        if (candidate.unassigned.empty()) {
            double cand_cost = cost(candidate);

            if (cand_cost < best_cost) {
                // Nuevo mejor global
                best_sol = candidate;
                current_sol = candidate;
                curr_cost = cand_cost;
                best_cost = cand_cost;
                score = w1;
            }
            else if (cand_cost < curr_cost) {
                // Nuevo mejor actual
                current_sol = candidate;
                curr_cost = cand_cost;
                score = w2;
            }
            else if (accept(cand_cost, curr_cost, T)) {
                // Solucion aceptada
                current_sol = candidate;
                curr_cost = cand_cost;
                score = w3;
            }
            // else: rechazada, score = w4
        }

        // Acumulacion de scores + cierre de segmento
        registerScore(d_idx, r_idx, score);
        if (iter % segment_size == 0) updateWeightsSegment();

        // Actualizacion de temperatura
        T = T * cooling_rate;
    }

    return best_sol;
}