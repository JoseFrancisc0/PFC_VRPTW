#include "alns.h"
#include <iostream>

ALNS::ALNS(const Instance& _inst, const Solution& _initial_sol, const SolverParams& _params)
    : inst(_inst), params(_params), current_sol(_initial_sol), best_sol(_initial_sol) {
    initOps();
}

void ALNS::initOps() {
    int n_customers = inst.clients.size() - 1;
    buildOperatorPool(n_customers, params, destroy_ops, repair_ops);

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
    start_temp = -(tuning::TEMP_SCALE * initial_d) / std::log(0.5);
    double T = start_temp;

    double curr_cost = cost(current_sol);
    double best_cost = cost(best_sol);

    int iters_since_best = 0;

    for (int iter = 1; iter <= max_iters; ++iter) {
        Solution candidate = current_sol;

        // Seleccion de operadores (ruleta de pesos adaptativos)
        int d_idx = selectDestroyOp();
        int r_idx = selectRepairOp();

        // r(d(x)), con el grado de destruccion propio del destroy elegido
        applyOperators(candidate, destroy_ops[d_idx], repair_ops[r_idx], rng);

        // Evaluacion y scores
        double score = w4; // por defecto, incluye candidato infactible
        bool improved_best = false;

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
                improved_best = true;
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

        // Acumulacion de scores + cierre de segmento
        registerScore(d_idx, r_idx, score);
        if (iter % segment_size == 0) updateWeightsSegment();

        // Actualizacion de temperatura
        T = T * cooling_rate;

        if (params.checkpoint_every > 0 && iter % params.checkpoint_every == 0)
            std::cout << "[CHECKPOINT] " << iter << " " << best_sol.used_vehicles << " " << best_sol.total_distance << "\n";
    }

    return best_sol;
}