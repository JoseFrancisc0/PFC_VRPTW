#ifndef OPERATORS_H
#define OPERATORS_H

#include <algorithm>
#include <cmath>
#include <vector>
#include <random>
#include "../VRPTW Environment/solution.h"

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
void greedyInsertion(Solution& sol);
void regret2Insertion(Solution& sol);
void regret3Insertion(Solution& sol);
void pGreedyInsertion(Solution& sol, double eta = 0.1);

// Reparacion Fase 1
void greedyInsertionRelaxed(Solution& sol);
void regret2InsertionRelaxed(Solution& sol);
void regret3InsertionRelaxed(Solution& sol);
void pGreedyInsertionRelaxed(Solution& sol, double eta = 0.1);

#endif //OPERATORS_H