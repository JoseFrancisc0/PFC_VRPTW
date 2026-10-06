#ifndef PARAMS_H
#define PARAMS_H

#include <string>
#include <stdexcept>

struct SolverParams {
    double alpha = 0.5; 
    double gamma = 0.7; 
    double epsilon_min = 0.01; 
    double beta = 0.99; 
    int learning_loop = 200;
    double eta = 0.8;
    double reward_gain = 1.0e4; 

    void set(const std::string& kv) {
        size_t eq = kv.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Parametro sin '=': " + kv);
        std::string k = kv.substr(0, eq);
        std::string v = kv.substr(eq + 1);

        if      (k == "alpha")            alpha = std::stod(v);
        else if (k == "gamma")            gamma = std::stod(v);
        else if (k == "epsilon_min")      epsilon_min = std::stod(v);
        else if (k == "beta")             beta = std::stod(v);
        else if (k == "learning_loop")    learning_loop = std::stoi(v);
        else if (k == "eta")              eta = std::stod(v);
        else if (k == "reward_gain")      reward_gain = std::stod(v);
        else throw std::runtime_error("Parametro desconocido: " + k);
    }
};

#endif // PARAMS_H
