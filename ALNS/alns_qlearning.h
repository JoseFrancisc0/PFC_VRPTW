#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include "alns.h"

// Tabla Q de un tipo de operador: q[estado][operador]. Tiene la misma forma que Roulette del ALNS clasico:
// select elige en cada iteracion, record acumula la recompensa del operador usado y closeSegment aprende
// al cerrar cada segmento de Roulette::SEGMENT_SIZE iteraciones
struct QTable {
    std::vector<std::vector<double>> q;
    std::vector<double> rewards;  // suma de recompensas de cada operador en el segmento
    std::vector<int> uses;

    QTable(int num_states, int num_actions)
        : q(num_states, std::vector<double>(num_actions, 0.0)), rewards(num_actions, 0.0), uses(num_actions, 0) {}
    int select(int state, double epsilon);  // epsilon-greedy; los empates se resuelven al azar
    void record(int action, double reward);
    // Q-learning con la recompensa media de cada operador usado en el segmento
    void closeSegment(int state, int next_state, double alpha, double gamma);
};

// Seleccion por Q-learning: una tabla para la destruccion y otra para la reparacion, con el mismo ritmo que
// las ruletas del clasico. Las dos comparten estado y recompensa; el estado no cambia dentro del segmento
class ALNS_QLearning : public ALNS {
    public:
        ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params);

    private:
        // Estado = 2 * fase + (el segmento anterior encontro una nueva mejor solucion). Fase 0: rutas; 1: distancia
        static const int NUM_STATES = 4;

        QTable destroy;
        QTable repair;
        int d_idx = 0;
        int r_idx = 0;

        int phase = 0;
        int state = 0;
        bool found_best = false;  // el segmento en curso encontro una nueva mejor solucion
        int phase_start = 0;      // iteracion en la que empezo la fase en curso
        double baseline = 0.0;    // media movil de las mejoras positivas de la fase

        Move selectOperators(int iter) override;
        void learn(int iter, const Outcome& outcome) override;
        void onDistancePhase(int iter) override;
        void closeSegment(int next_state);
        double epsilon(int iter) const;
        double reward(double improvement);
};

#endif //ALNS_QLEARNING_H
