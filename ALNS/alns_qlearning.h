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

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&, bool)>;

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

        // Marca los destroy cuyo proposito es eliminar una ruta (ver alns.h:
        // el tratamiento es identico en ambos solvers, es base compartida).
        std::vector<bool> destroy_is_route_elimination;

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
        // S = {0, 1}: 1 si la iteracion previa logro alguna mejora (global o
        // local), 0 si no. Estado minimo y bien muestreado: con 2 estados y
        // 24 acciones son 48 celdas, ~500 visitas cada una en una corrida de
        // 25k iteraciones. Particionar mas (p.ej. agregando fase temporal)
        // fragmenta la muestra sin aportar informacion accionable.
        static constexpr int num_states = 2;

        std::vector<std::vector<double>> Q;

        // --- Hiperparametros de Q-learning --------------------------------
        // Valores sintonizados por RSM / Box-Behnken sobre el problema origen.
        // OJO con alpha: en VRPTW con 25k iteraciones la tasa de exito por
        // iteracion es baja (pocas mejoras entre muchos fracasos), y alpha=0.5
        // da un horizonte efectivo de ~2 muestras, o sea una estimacion muy
        // reactiva. Es el primer hiperparametro a ablacionar (0.5 -> 0.1 -> 0.05)
        // si la politica resulta demasiado ruidosa.
        double alpha = 0.5;          // tasa de aprendizaje (constante)
        double gamma = 0.7;          // factor de descuento
        double epsilon_0 = 1.0;      // exploracion inicial
        double beta = 0.99;          // decaimiento de epsilon por paso
        double epsilon_min = 0.01;   // piso de exploracion (salvaguarda)
        // Con beta=0.99 epsilon cae de 1.0 al piso en ~460 pasos tras el warm-up.
        // learning_loop es absoluto, no proporcional: con presupuestos chicos
        // (<= 1000 iteraciones) conviene reducirlo.
        int learning_loop = 200;     // iteraciones iniciales 100% aleatorias
        double eta = 0.8;            // peso de la mejora global en la recompensa

        // --- Escala de la recompensa (adaptacion a VRPTW) ------------------
        // Con cost = 10000*veh + dist la mejora relativa abarca ~4 ordenes de
        // magnitud: ~1e-5 para un ajuste de distancia y ~1e-1 para eliminar un
        // vehiculo. Usando la mejora cruda, los (raros) eventos de vehiculo
        // dominan la media por operador y el estimador se vuelve de varianza
        // alta, que es justo lo que hace que un argmax elija ruido. La
        // compresion log1p(gain*delta) preserva el orden -- un vehiculo menos
        // sigue valiendo ~7x una buena mejora de distancia -- pero acota el
        // rango para que la media sea estable.
        static constexpr double reward_gain = 1.0e4;

        void initOps();
        int selectAction(int state, double epsilon);
        bool accept(double cand_cost, double curr_cost, double current_temp);
        static double shapeImprovement(double relative_gain);
        int destroyOf(int action) const;
        int repairOf(int action) const;
};

#endif
