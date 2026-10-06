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

        int used_vehicles = 0;
        double total_distance = 0.0;

        Solution(const Instance& _inst) : inst(_inst) {
            generateInitialSolution();
        }

        friend std::ostream& operator<<(std::ostream& os, const Solution& s);
        Solution& operator=(const Solution& other);

        void updateMetrics();

    private:
        void generateInitialSolution();
};

double cost(const Solution& sol);

#endif //SOLUTION_H