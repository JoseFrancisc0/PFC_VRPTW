#ifndef OPERATORS_H
#define OPERATORS_H

#include <algorithm>
#include <cmath>
#include <vector>
#include <random>
#include "../VRPTW Environment/solution.h"

using DestroyOp = void (*)(Solution& sol, int q);
using RepairOp  = void (*)(Solution& sol, bool noise);

void randomRemoval(Solution& sol, int q);
void routeRemoval(Solution& sol, int q);
void worstRemoval(Solution& sol, int q);
void shawRemoval(Solution& sol, int q);
void clusterRemoval(Solution& sol, int q);
void timeWindowRemoval(Solution& sol, int q);

void greedyInsertion(Solution& sol, bool noise);
void regret2Insertion(Solution& sol, bool noise);
void regret3Insertion(Solution& sol, bool noise);
void regret4Insertion(Solution& sol, bool noise);
void regretMInsertion(Solution& sol, bool noise);

extern const int NUM_DESTROY;
extern const int NUM_REPAIR;

void applyOperators(Solution& sol, int destroy_idx, int repair_idx);

#endif //OPERATORS_H
