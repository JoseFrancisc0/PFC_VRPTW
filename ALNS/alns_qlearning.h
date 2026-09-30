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
#include "../Utils/tuning.h"
#include "../Utils/params.h"
#include "operator_pool.h"

class ALNS_QLearning {
    public:
        ALNS_QLearning(const Instance& _inst, const Solution& _initial_sol, const SolverParams& _params = SolverParams());
        Solution solve(int max_iters);

    private:
        const Instance& inst;
        SolverParams params;
        Solution current_sol;
        Solution best_sol;

        // Pool de operadores compartido con ALNS clasico (operator_pool.h).
        std::vector<DestroyEntry> destroy_ops;
        std::vector<RepairOp> repair_ops;

        // Mismo esquema de enfriamiento SA, mismo cost() y mismo grado de
        // destruccion que ALNS clasico: la unica diferencia entre ambos
        // algoritmos sigue siendo el criterio de seleccion de operadores.
        double start_temp;
        double cooling_rate = 0.9995;

        // --- Espacio de acciones -----------------------------------------
        // La accion es el PAR (destroy, repair), no dos elecciones
        // independientes. Con dos tablas Q separadas (una por familia de
        // operadores) el metodo era estructuralmente identico a las dos
        // ruletas independientes del ALNS clasico y no podia representar
        // sinergia entre operadores: p.ej. que routeRemoval rinda bien con
        // regret3Insertion y mal con greedyInsertion. Una unica tabla sobre
        // los |D|x|R| pares si puede aprender esa interaccion.
        int num_actions = 0;

        // --- Espacio de estados ------------------------------------------
        // state_mode = 0: S = {0, 1}, 1 si la iteracion previa logro alguna
        // mejora (global o local). Con 2 estados y 24 acciones son 48 celdas,
        // ~500 visitas cada una en una corrida de 25k iteraciones.
        // state_mode = 1: se cruza con 3 niveles de estancamiento (iteraciones
        // desde la ultima mejora del mejor global, relativas al limite de
        // reinicio), 6 estados. El estancamiento es la unica senal barata que
        // distingue "la busqueda progresa" de "la busqueda esta atascada", que
        // es donde conviene cambiar de operadores.
        int num_states = 2;
        int stateOf(bool improved, int iters_since_best) const;

        std::vector<std::vector<double>> Q;

        // --- Hiperparametros de Q-learning --------------------------------
        // Viven en 'params' (Utils/params.h) para poder barrerlos sin
        // recompilar. Referencia: alpha=0.5, gamma=0.7, epsilon_min=0.01,
        // beta=0.99, learning_loop=200, eta=0.8.
        //
        // --- Escala de la recompensa (adaptacion a VRPTW) ------------------
        // Con cost = 10000*veh + dist la mejora relativa abarca ~4 ordenes de
        // magnitud: ~1e-5 para un ajuste de distancia y ~1e-1 para eliminar un
        // vehiculo. Usando la mejora cruda, los (raros) eventos de vehiculo
        // dominan la media por operador y el estimador se vuelve de varianza
        // alta, que es justo lo que hace que un argmax elija ruido. La
        // compresion log1p(reward_gain*delta) preserva el orden -- un vehiculo
        // menos sigue valiendo ~7x una buena mejora de distancia -- pero acota
        // el rango para que la media sea estable.

        void initOps();
        int selectAction(int state, double epsilon);
        bool accept(double cand_cost, double curr_cost, double current_temp);
        double shapeImprovement(double relative_gain) const;
        int destroyOf(int action) const;
        int repairOf(int action) const;
};

#endif
