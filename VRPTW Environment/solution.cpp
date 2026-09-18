#include "solution.h"

// Para imprimir soluciones
std::ostream& operator<<(std::ostream& os, const Solution& s){
    os << "Vehiculos usados: " << s.used_vehicles << "  - Distancia total: " << s.total_distance << "\n";

    int r = 0;
    for (const Route& route : s.routes) {
        if (route.path.size() > 2) {
            os << "Ruta [" << r << "]: ";
            for (const int& i : route.path) {
                if (i != 0)
                    os << i << " ";
            }
            os << "\n";
            r++;
        }
    }

    return os;
}

// Comparador de soluciones (jerarquia de objetivos)
bool Solution::operator<(const Solution& other) const {
    if (this->used_vehicles < other.used_vehicles) return true;
    if (this->used_vehicles > other.used_vehicles) return false;

    return this->total_distance < other.total_distance;
}

// Operador copia para soluciones
Solution& Solution::operator=(const Solution& other) {
    if (this == &other) return *this;

    this->routes = other.routes;
    this->unassigned = other.unassigned;
    this->total_distance = other.total_distance;
    this->used_vehicles = other.used_vehicles;

    return *this;
}

// Actualizar metricas objetivo
void Solution::updateMetrics() {
    used_vehicles = 0;
    total_distance = 0.0;

    for (const Route& route: routes) {
        if (route.path.size() > 2) {
            used_vehicles++;
            total_distance += route.distance;
        }
    }
}

// Generar solucion inicial (NN-Based)
void Solution::generateInitialSolution() {
    int N = inst.clients.size();
    std::vector<bool> visited(N, false);
    visited[0] = true;
    int unvisited = N - 1;

    while (unvisited > 0) {
        Route current_route;
        int current_node = 0;
        double current_time = 0.0;
        double current_load = 0.0;

        bool added_client = true;

        while (added_client) {
            added_client = false;
            int best_client = -1;
            double best_distance = std::numeric_limits<double>::max();

            for (int i = 1; i < N; ++i) {
                if (visited[i]) continue;
                if (!inst.is_reachable[current_node][i]) continue;
                if (current_load + inst.clients[i].demand > inst.capacity) continue;

                double arrival_time = current_time + inst.clients[current_node].service_time + inst.dist_mat[current_node][i];
                arrival_time = std::max(arrival_time, inst.clients[i].ready_time);

                if (arrival_time <= inst.clients[i].due_date) {
                    double return_time = arrival_time + inst.clients[i].service_time + inst.dist_mat[i][0];

                    if (return_time <= inst.clients[0].due_date) {
                        if (inst.dist_mat[current_node][i] < best_distance) {
                            best_distance = inst.dist_mat[current_node][i];
                            best_client = i;
                        }
                    }
                }
            }

            if (best_client != -1) {
                current_route.path.insert(current_route.path.end() - 1, best_client);
                visited[best_client] = true;
                unvisited--;
                
                current_load += inst.clients[best_client].demand;
                double arrival = current_time + inst.clients[current_node].service_time + inst.dist_mat[current_node][best_client];
                current_time = std::max(arrival, inst.clients[best_client].ready_time);
                
                current_route.distance += best_distance;
                current_route.load = current_load;
                
                current_node = best_client;
                added_client = true;
            }
            else {
                if (current_node == 0) {
                    for (int i = 1; i < N; ++i) {
                        if (!visited[i]) {
                            unassigned.push_back(i);
                            visited[i] = true;
                            unvisited--;
                        }
                    }
                    break;
                }
            }
        }
        
        current_route.distance += inst.dist_mat[current_node][0];
        current_route.recalculate(inst);
        routes.push_back(current_route);
    }

    updateMetrics();
}

// Calcular costos de solucion
double cost(const Solution& sol) {
    const double VEHICLE_COST = 50000.0;
    return (sol.used_vehicles * VEHICLE_COST) + sol.total_distance;
}
