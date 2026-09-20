#ifndef SOLUTION_H
#define SOLUTION_H

#include <iostream>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
#include "instance.h"

class Solution {
    public:
        const Instance& inst;
        std::vector<Route> routes;
        std::vector<int> unassigned;

        // Funcion objetivo f_1 y f_2
        int used_vehicles = 0;
        double total_distance = 0.0;
        double total_lateness = 0.0;

        Solution(const Instance& _inst) : inst(_inst) {
            generateInitialSolution();
        }

        friend std::ostream& operator<<(std::ostream& os, const Solution& s);
        bool operator<(const Solution& other) const;
        Solution& operator=(const Solution& other);

        void updateMetrics();

    private:
        void generateInitialSolution();
};

double cost(const Solution& sol);
double cost_phase1(const Solution& sol);

#endif //SOLUTION_H