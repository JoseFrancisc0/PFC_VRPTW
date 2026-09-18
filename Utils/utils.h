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

Solution solve_with_classic(const Instance& inst, const Solution& sol, int max_iters, std::string metrics_path = "");
Solution solve_with_qlearning(const Instance& inst, const Solution& sol, int max_iters, std::string metrics_path = "");

#endif //UTILS_H