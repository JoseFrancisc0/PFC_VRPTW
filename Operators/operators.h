#ifndef OPERATORS_H
#define OPERATORS_H

#include <algorithm>
#include <cmath>
#include <vector>
#include <random>
#include "../VRPTW Environment/solution.h"
#include "../Utils/tuning.h"

struct RemovalCandidate {
    int route_idx;
    int node_idx;
    double deviation_cost;
};

struct RelatednessCandidate {
    int route_idx;
    int node_idx;
    int client_id;
    double relatedness;
};

struct RouteInsertion {
    int route_idx;
    int insert_pos;
    double cost;
};

// Operadores de destruccion
void randomRemoval(Solution& sol, int q);
void routeRemoval(Solution& sol, int q);
void worstRemoval(Solution& sol, int q, double p = 3.0);
void shawRemoval(Solution& sol, int q, double p = 3.0);
void timeWindowRemoval(Solution& sol, int q);
void removeSmallestRoute(Solution& sol);

// Operadores de Reparacion Fase 2
// allow_new_routes = false prohibe abrir rutas nuevas (y rellenar rutas
// vacias): lo que no entra en las rutas existentes queda en sol.unassigned.
// Es lo que convierte a removeSmallestRoute en un intento real de eliminar un
// vehiculo; con permiso para abrir rutas, el repair reconstruye la ruta que el
// destroy acababa de vaciar y el operador se anula solo.
void greedyInsertion(Solution& sol, bool allow_new_routes = true);
void regret2Insertion(Solution& sol, bool allow_new_routes = true);
void regret3Insertion(Solution& sol, bool allow_new_routes = true);
void pGreedyInsertion(Solution& sol, bool allow_new_routes = true, double eta = 0.1);

// Intento de eliminacion de ruta, no regresivo: primero repara sin permitir
// rutas nuevas; si TODOS los clientes entran, la solucion resultante tiene un
// vehiculo menos. Si alguno no entra, se descarta el intento y se repara de
// forma normal, con lo que el operador conserva su valor como perturbacion.
// Cuesta una copia de Solution y un repair extra solo cuando el intento falla.
template <typename RepairFn>
void repairTryEliminateRoute(Solution& sol, RepairFn&& repair) {
    Solution attempt = sol;
    repair(attempt, false);

    // Pool de eyeccion: los clientes que quedaron varados no cabian en las
    // rutas restantes TAL COMO ESTAN. Sacudir unos pocos clientes al azar y
    // reinsertar todo el pool (siempre sin abrir rutas) reordena el resto de
    // las rutas para hacerles lugar. Es el unico mecanismo del algoritmo capaz
    // de atravesar el valle "la ruta quedo casi vacia pero sobran 1-2
    // clientes", que es justo donde vive toda reduccion de flota.
    for (int k = 0; !attempt.unassigned.empty() && k < tuning::EJECTION_BUDGET; ++k) {
        randomRemoval(attempt, tuning::EJECTION_SHAKE);
        repair(attempt, false);
    }

    if (attempt.unassigned.empty()) {
        sol = attempt;
        return;
    }

    repair(sol, true);
}

// Reparacion Fase 1
void greedyInsertionRelaxed(Solution& sol);
void regret2InsertionRelaxed(Solution& sol);
void regret3InsertionRelaxed(Solution& sol);
void pGreedyInsertionRelaxed(Solution& sol, double eta = 0.1);

#endif //OPERATORS_H