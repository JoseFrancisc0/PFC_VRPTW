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

        // Categorias de score base (misma escala que ALNS clasico), mas un
        // nivel adicional exclusivo para el evento de mayor prioridad segun
        // la jerarquia del problema: reducir vehiculos antes que distancia.
        double w0 = 45.0; // nuevo mejor global CON MENOS VEHICULOS (prioridad maxima)
        double w1 = 33.0;  // nuevo mejor global (misma cantidad de vehiculos, menos distancia)
        double w2 = 13.0;  // nuevo mejor actual
        double w3 = 9.0;   // aceptada por SA
        double w4 = 0.0;   // rechazada

        // Hiperparametros de Q-learning (criterio de seleccion de operadores)
        double gamma = 0.8;

        // Estado = (fase de busqueda) x (nivel de estancamiento), independiente
        // de la recompensa inmediata. Esto evita que el estado sea una simple
        // recodificacion del score de ese mismo paso (lo que en la version
        // anterior volvia inutil el termino de bootstrap gamma*max_Q): aqui la
        // fase (temperatura/iteracion) y el estancamiento (iteraciones sin
        // mejorar el mejor global) aportan informacion real sobre el progreso
        // de la busqueda, distinta de lo que ya dice la recompensa del paso.
        static const int num_phases = 3;      // 0=temprano, 1=medio, 2=tardio
        static const int num_stagnation = 3;  // 0=recien mejoro, 1=medio, 2=estancado
        int num_states = num_phases * num_stagnation;

        std::vector<std::vector<double>> Q_table_D;
        std::vector<std::vector<double>> Q_table_R;

        // Contadores de visitas por (estado, accion) para una tasa de
        // aprendizaje adaptativa (Robbins-Monro acotada): mas estable que un
        // alpha fijo, sobre todo en estados poco visitados.
        std::vector<std::vector<int>> visits_D;
        std::vector<std::vector<int>> visits_R;
        static constexpr int alpha_cap = 40; // visita a partir de la cual alpha deja de bajar
        static constexpr double alpha_min = 0.05;

        void initOps();
        int selectOp(const std::vector<double>& q_values, double epsilon);
        bool accept(double cand_cost, double curr_cost, double current_temp);
        static int computeState(int phase, int stagnation_level);
        double learningRate(int visits) const;
};

#endif