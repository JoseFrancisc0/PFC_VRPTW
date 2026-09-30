#ifndef OPERATOR_POOL_H
#define OPERATOR_POOL_H

#include <vector>
#include <functional>
#include <algorithm>
#include "../Operators/operators.h"
#include "../Utils/params.h"

using DestroyOp = std::function<void(Solution&, int)>;
using RepairOp  = std::function<void(Solution&, bool)>;

// Un destroy del pool: la funcion, el rango del grado de destruccion q que usa
// y si su proposito es eliminar una ruta (tras el, el repair intenta primero
// no abrir rutas nuevas; ver repairTryEliminateRoute).
struct DestroyEntry {
    DestroyOp fn;
    int q_lo;
    int q_hi;
    bool route_elimination;
};

// Pool de operadores COMPARTIDO por ALNS y ALNS_QLearning. Construirlo en un
// solo lugar garantiza que ambos solvers elijan exactamente entre los mismos
// operadores: la unica diferencia entre ellos es el criterio de seleccion.
inline void buildOperatorPool(int n_customers, const SolverParams& params,
                              std::vector<DestroyEntry>& destroy_pool,
                              std::vector<RepairOp>& repair_pool) {
    int q_min = std::max(4, static_cast<int>(0.10 * n_customers));
    int q_max = std::max(q_min + 1, static_cast<int>(0.4 * n_customers));

    std::vector<DestroyOp> parametric = {
        randomRemoval,
        routeRemoval,
        [](Solution& sol, int q) { worstRemoval(sol, q); },
        [](Solution& sol, int q) { shawRemoval(sol, q); },
        [](Solution& sol, int q) { timeWindowRemoval(sol, q); },
    };

    // Con L niveles, [q_min, q_max] se parte en L subrangos contiguos y cada
    // destroy parametrico aparece una vez por subrango. Con L = 1 el pool es
    // el original.
    int L = std::min(params.destroy_levels, q_max - q_min + 1);
    int values = q_max - q_min + 1;

    destroy_pool.clear();
    for (const DestroyOp& fn : parametric) {
        for (int k = 0; k < L; ++k) {
            int lo = q_min + (k * values) / L;
            int hi = q_min + ((k + 1) * values) / L - 1;
            destroy_pool.push_back({fn, lo, hi, false});
        }
    }

    // Solo removeSmallestRoute es un operador de eliminacion de ruta
    // propiamente dicho: vacia UNA sola ruta (la mas chica), que es el unico
    // caso en que exigir "cero rutas nuevas" es alcanzable. routeRemoval vacia
    // rutas enteras hasta juntar q clientes (1-5 rutas en instancias R/RC);
    // exigirle cero rutas nuevas lo volveria infactible casi siempre. No usa
    // q, asi que no se desdobla por niveles.
    destroy_pool.push_back({[](Solution& sol, int) { removeSmallestRoute(sol); }, q_min, q_min, true});

    repair_pool = {
        [](Solution& sol, bool anr) { greedyInsertion(sol, anr); },
        [](Solution& sol, bool anr) { regret2Insertion(sol, anr); },
        [](Solution& sol, bool anr) { regret3Insertion(sol, anr); },
        [](Solution& sol, bool anr) { pGreedyInsertion(sol, anr); },
    };
}

// Aplica r(d(x)) sobre 'candidate' con el grado de destruccion del operador.
template <typename Rng>
inline void applyOperators(Solution& candidate, const DestroyEntry& d, const RepairOp& r, Rng& rng) {
    std::uniform_int_distribution<int> q_distr(d.q_lo, d.q_hi);
    d.fn(candidate, q_distr(rng));
    if (d.route_elimination)
        repairTryEliminateRoute(candidate, r);
    else
        r(candidate, true);
}

#endif // OPERATOR_POOL_H
