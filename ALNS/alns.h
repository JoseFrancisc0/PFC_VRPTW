#ifndef ALNS_H
#define ALNS_H

#include <vector>
#include <functional>
#include <string>
#include "../Operators/operators.h"
#include "../Utils/tuning.h"

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&, bool)>;

class ALNS {
    public:
        ALNS(const Instance& _inst, const Solution& _initial_sol);
        Solution solve(int max_iters);

    private:
        const Instance& inst;
        Solution current_sol;
        Solution best_sol;

        // Operadores de destroy (Omega^-)
        std::vector<DestroyOp> destroy_ops;
        std::vector<double> destroy_weights;

        // Marca los destroy cuyo proposito es eliminar una ruta: tras ellos el
        // repair intenta primero no abrir rutas nuevas, para que el intento de
        // eliminacion no lo deshaga el propio repair.
        std::vector<bool> destroy_is_route_elimination;

        // Operadores de repair (Omega^+)
        std::vector<RepairOp> repair_ops;
        std::vector<double> repair_weights;

        // Hiperparametros del ALNS
        double decay = 0.9;
        double w1 = 33.0;
        double w2 = 13.0;
        double w3 = 9.0;
        double w4 = 0.0;

        // Actualizacion de pesos por segmento (Ropke & Pisinger): los scores se
        // acumulan durante 'segment_size' iteraciones y los pesos se actualizan
        // una sola vez al cierre del segmento, con el promedio del segmento.
        static const int segment_size = 100;
        std::vector<double> destroy_scores;
        std::vector<int> destroy_uses;
        std::vector<double> repair_scores;
        std::vector<int> repair_uses;

        // Hiperparametros del Simulated Annealing
        double start_temp;
        double cooling_rate = 0.9995;

        void initOps();
        int selectDestroyOp();
        int selectRepairOp();
        bool accept(double cand_cost, double curr_cost, double current_temp);
        void registerScore(int used_destroy_idx, int used_repair_idx, double score);
        void updateWeightsSegment();
};

#endif //ALNS_H