#ifndef ALNS_H
#define ALNS_H

#include <utility>
#include "../Operators/operators.h"
#include "../Utils/params.h"

struct Outcome {
    bool new_best = false;
    bool improved = false;
    bool accepted = false;
    double gain_best = 0.0;
};

// Lo que aplica una iteracion. Con perturb, la destruccion es amplia y el candidato se acepta siempre
struct Move {
    int destroy = 0;
    int repair = 0;
    bool perturb = false;
};

// Mejora relativa de cand sobre ref en el primer criterio en que difieren:
// clientes sin asignar, vehiculos o distancia (el mismo orden que impone cost())
double relativeGain(const Solution& ref, const Solution& cand);

class ALNS {
    public:
        ALNS(const Solution& _initial_sol, const SolverParams& _params);
        virtual ~ALNS() = default;

        Solution solve(int _max_iters);

    protected:
        SolverParams params;

        // Iteraciones de cada fase: tope de la fase 1 y duracion exacta de la fase 2
        int phase_iters = 0;

        virtual Move selectOperators(int iter) = 0;

        virtual void learn(int iter, const Outcome& outcome) = 0;

        // Aviso de que en iter empieza la fase 2 (el objetivo pasa de rutas a distancia)
        virtual void onDistancePhase(int iter) {}

        const Solution& currentSolution() const { return current_sol; }

    private:
        Solution current_sol;
        Solution best_sol;

        bool accept(double cand_cost, double curr_cost, double temp);
};

#endif //ALNS_H
