#include "instance.h"

// Recalcular costos de ruta usando Forward Slack Times
void Route::recalculate(const Instance& inst) {
    load = 0.0;
    distance = 0.0;
    int L = path.size();

    arrival_times.resize(L, 0.0);
    wait_times.resize(L, 0.0);
    time_slacks.resize(L, 0.0);

    arrival_times[0] = 0.0;
    wait_times[0] = 0.0;
    double current_time = 0.0;
    
    for (size_t i = 0; i < L - 1; ++i) {
        int curr = path[i];
        int next = path[i+1];

        distance += inst.dist_mat[curr][next];

        if (curr != 0)
            load += inst.clients[curr].demand;

        double start_service = std::max(current_time, inst.clients[curr].ready_time);
        double arrival_at_next= start_service + inst.clients[curr].service_time + inst.dist_mat[curr][next];

        arrival_times[i + 1] = arrival_at_next;
        wait_times[i + 1] = std::max(0.0, inst.clients[next].ready_time - arrival_at_next);
        current_time = arrival_at_next;
    }

    int last = path[L - 1];
    double start_service_last = std::max(arrival_times[L - 1], inst.clients[last].ready_time);
    time_slacks[L - 1] = inst.clients[last].due_date - start_service_last;

    for (int i = L - 2; i >= 0; --i) {
        int node = path[i];
        double start_service = std::max(arrival_times[i], inst.clients[node].ready_time);
        double local_slack = inst.clients[node].due_date - start_service;

        time_slacks[i] = std::min(local_slack, time_slacks[i+1] + wait_times[i+1]);
    }
}   

// Cargar instancia desde el benchmark
void Instance::loadInstance(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open())
        throw std::runtime_error("No se puede abrir " + path);

    std::string dummy;
    while (file >> dummy && dummy != "CAPACITY") {}

    file >> num_vehicles >> capacity;

    while (file >> dummy && dummy != "SERVICE") {}
    file >> dummy;

    int id;
    double x, y, demand, ready_time, due_date, service_time;
    while (file >> id >> x >> y >> demand >> ready_time >> due_date >> service_time) {
        clients.emplace_back(id, x, y, demand, ready_time, due_date, service_time);
    }
    file.close();

    int N = clients.size();
    dist_mat.assign(N, std::vector<double>(N, 0.0));
    is_reachable.assign(N, std::vector<bool>(N, false));
    
}

// Precomputar distancias
void Instance::precomputeDistances() {
    int N = clients.size();
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (i == j)
                dist_mat[i][j] = 0.0;
            else {
                double dx = clients[i].x - clients[j].x;
                double dy = clients[i].y - clients[j].y;
                dist_mat[i][j] = std::sqrt(dx * dx + dy * dy);
            }
        }
    }
}

// Precomputar rutas validas
void Instance::precomputeFeasibility() {
    int N = clients.size();
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (i == j) {
                is_reachable[i][j] = false;
                continue;
            }

            double earliest_arrival_at_j = clients[i].ready_time + clients[i].service_time + dist_mat[i][j];

            if (earliest_arrival_at_j > clients[j].due_date)
                is_reachable[i][j] = false;
            else
                is_reachable[i][j] = true;
        }
    }
}