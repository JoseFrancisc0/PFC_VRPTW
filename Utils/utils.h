#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <vector>
#include <limits>
#include <algorithm>
#include <cmath>
#include <string>
#include "../ALNS/alns.h"
#include "../ALNS/alns_qlearning.h"

bool verifySolution(const Instance& inst, const Solution& sol);
void solveExact(Solution& current_sol, std::vector<bool>& unassigned, int unassigned_count, double& best_cost, Solution& best_sol);

Solution solve_with_classic(const Instance& inst, const Solution& sol, int max_iters, const SolverParams& params);
Solution solve_with_qlearning(const Instance& inst, const Solution& sol, int max_iters, const SolverParams& params);

#endif //UTILS_H