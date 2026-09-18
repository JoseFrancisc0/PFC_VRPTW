#ifndef INSTANCE_H
#define INSTANCE_H

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <stdexcept>
#include <vector>

extern std::mt19937 rng;

// Struct del cliente
struct Client {
    int id;
    double x;
    double y;
    double demand;
    double ready_time;
    double due_date;
    double service_time;

    Client(int _id, double _x, double _y, double d, double a, double b, double s): 
        id(_id), x(_x), y(_y), demand(d), ready_time(a), due_date(b), service_time(s)  {};
};

class Instance;

// Struct de ruta / vehiculo
struct Route {
    std::vector<int> path;
    double load = 0.0;
    double distance = 0.0;

    // Para evaluaciones O(1)
    std::vector<double> arrival_times;
    std::vector<double> wait_times;
    std::vector<double> time_slacks;

    Route () {
        path.push_back(0);
        path.push_back(0);

        arrival_times.resize(2, 0.0);
        wait_times.resize(2, 0.0);
        time_slacks.resize(2, 0.0);
    }

    void recalculate(const Instance& inst);
};

// Clase de la instancia
class Instance {
    public:
        int num_vehicles;
        int capacity;
        std::vector<Client> clients;
        std::vector<std::vector<double>> dist_mat;
        std::vector<std::vector<bool>> is_reachable;

        Instance(const std::string& path) {
            loadInstance(path);
            precomputeDistances();
            precomputeFeasibility();
        }
    
    private:
        void loadInstance(const std::string& path);
        void precomputeDistances();
        void precomputeFeasibility();
};

#endif //INSTANCE_H