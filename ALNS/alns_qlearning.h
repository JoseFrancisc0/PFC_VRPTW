#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include <functional>
#include <string>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random> 
#include "../Operators/operators.h"

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&)>;

class ALNS_QLearning {
    public:
        ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters, bool save_metrics = false);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;
        std::vector<DestroyOp> destroy_ops;
        std::vector<RepairOp> repair_ops;

        // Mismo esquema de enfriamiento SA que ALNS (base compartida).
        double start_temp;
        double cooling_rate = 0.9995;

        // Categorias de score identicas a ALNS clasico (misma base: la unica
        // diferencia entre algoritmos es como se usan estos scores para elegir
        // el proximo operador: promedio por segmento vs. recompensa de Q-learning).
        double w1 = 33.0; // nuevo mejor global
        double w2 = 13.0; // nuevo mejor actual
        double w3 = 9.0;  // aceptada por SA
        double w4 = 0.0;  // rechazada

        // Hiperparametros de Q-learning (criterio de seleccion de operadores)
        double alpha = 0.1;
        double gamma = 0.8;
        // Estados: 0 = movimiento rechazado, 1 = aceptado (mejora actual o SA),
        // 2 = mejoro la mejor solucion global.
        int num_states = 3;

        std::vector<std::vector<double>> Q_table_D;
        std::vector<std::vector<double>> Q_table_R;

        void initOps();
        int selectOp(const std::vector<double>& q_values, double epsilon);
        bool accept(double cand_cost, double curr_cost, double current_temp);
};

#endif