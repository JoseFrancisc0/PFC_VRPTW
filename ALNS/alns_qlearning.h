#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include "alns.h"

// Tabla Q de un tipo de operador: q[estado][operador]. Es el analogo de Roulette en el ALNS clasico:
// select elige un operador y update aprende del resultado, ambos en cada iteracion
struct QTable {
    std::vector<std::vector<double>> q;

    QTable(int num_states, int num_actions) : q(num_states, std::vector<double>(num_actions, 0.0)) {}
    int select(int state, double epsilon);  // epsilon-greedy; los empates se resuelven al azar
    void update(int state, int action, double reward, int next_state, double alpha, double gamma);
};

// Seleccion por Q-learning: una tabla para la destruccion y otra para la reparacion. Las dos comparten
// estado y recompensa, eligen en cada iteracion y no cambian de tamano entre fases
class ALNS_QLearning : public ALNS {
    public:
        ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params);

    private:
        // Estado = 2 * fase + (la iteracion anterior mejoro la solucion actual). Fase 0: rutas; 1: distancia
        static const int NUM_STATES = 4;

        QTable destroy;
        QTable repair;
        int d_idx = 0;
        int r_idx = 0;

        int phase = 0;
        int state = 0;
        int phase_start = 0;    // iteracion en la que empezo la fase en curso
        double baseline = 0.0;  // media movil de las mejoras positivas de la fase

        Move selectOperators(int iter) override;
        void learn(int iter, const Outcome& outcome) override;
        void onDistancePhase(int iter) override;
        double epsilon(int iter) const;
        double reward(double improvement);
};

#endif //ALNS_QLEARNING_H
