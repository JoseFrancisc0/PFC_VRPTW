#ifndef PARAMS_H
#define PARAMS_H

#include <string>
#include <stdexcept>

struct SolverParams {
    double alpha = 0.1;
    double gamma = 0.8;
    double epsilon_min = 0.1;
    int episode_len = 50;         // iteraciones por decision del agente
    double learning_loop = 0.05;  // fraccion de los episodios de cada fase con exploracion pura
    double eta = 0.8;

    void set(const std::string& kv) {
        size_t eq = kv.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Parametro sin '=': " + kv);
        std::string k = kv.substr(0, eq);
        std::string v = kv.substr(eq + 1);

        if      (k == "alpha")            alpha = std::stod(v);
        else if (k == "gamma")            gamma = std::stod(v);
        else if (k == "epsilon_min")      epsilon_min = std::stod(v);
        else if (k == "episode_len")      episode_len = std::stoi(v);
        else if (k == "learning_loop")    learning_loop = std::stod(v);
        else if (k == "eta")              eta = std::stod(v);
        else throw std::runtime_error("Parametro desconocido: " + k);
    }
};

#endif // PARAMS_H
