#ifndef ALNS_QLEARNING_H
#define ALNS_QLEARNING_H

#include <vector>
#include "alns.h"

// El agente decide por episodios de episode_len iteraciones (Li et al., 2026): en cada uno fija
// el operador de destruccion y, en la fase 1, si el episodio arranca con una perturbacion.
// La reparacion se elige con la misma ruleta adaptativa del ALNS clasico.
class ALNS_QLearning : public ALNS {
    public:
        ALNS_QLearning(const Solution& _initial_sol, const SolverParams& _params);

    private:
        // Estado: fase (rutas / distancia) x si el episodio anterior logro mejora (la misma senal de la
        // recompensa). Mejorar la mejor global es raro: ese estado casi no se visitaba y sesgaba el bootstrap
        static const int NUM_STATES = 4;

        std::vector<std::vector<double>> Q;
        int phase = 0;
        int state = 0;
        int action = 0;
        int decisions = 0;  // episodios elegidos en la fase en curso

        int episode_step = 0;  // 0: el siguiente selectOperators abre un episodio
        double episode_gain_best = 0.0;
        double episode_gain_local = 0.0;  // mayor mejora de la solucion actual sobre episode_start
        Solution episode_start;

        double baseline = 0.0;  // media movil de las mejoras positivas de la fase

        Roulette repair;
        int r_idx = 0;

        Move selectOperators(int iter) override;
        void learn(int iter, const Outcome& outcome) override;
        void onDistancePhase(int iter) override;
        int numActions() const;  // con perturbacion solo en la fase 1 y si params.perturb
        int chooseAction();
        double reward(double improvement);
};

#endif //ALNS_QLEARNING_H
