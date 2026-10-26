#ifndef ALNS_H
#define ALNS_H

#include <utility>
#include "../Operators/operators.h"
#include "../Utils/params.h"

struct Outcome {
    bool new_best = false;
    bool improved = false;
    bool accepted = false;
    double gain_best = 0.0;     // ganancia relativa sobre la mejor solucion
    double gain_current = 0.0;  // ganancia relativa sobre la solucion actual
};

// Operadores que aplica una iteracion
struct Move {
    int destroy = 0;
    int repair = 0;
};

// Mejora relativa de cand sobre ref en el primer criterio en que difieren:
// clientes sin asignar, vehiculos o distancia (el mismo orden que impone cost())
double relativeGain(const Solution& ref, const Solution& cand);

// Seleccion por ruleta adaptativa de Ropke & Pisinger (2006): puntaje 33/13/9/0 por iteracion,
// pesos actualizados al cerrar cada segmento de SEGMENT_SIZE iteraciones
struct Roulette {
    static const int SEGMENT_SIZE = 100;

    std::vector<double> weights;
    std::vector<double> scores;
    std::vector<int> uses;

    explicit Roulette(int n) : weights(n, 1.0), scores(n, 0.0), uses(n, 0) {}
    int select();
    void record(int idx, const Outcome& outcome);
    void closeSegment();
};

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
