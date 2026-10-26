#ifndef PARAMS_H
#define PARAMS_H

#include <string>
#include <stdexcept>

struct SolverParams {
    // Parametros del Q-learning
    double alpha = 0.1;           // tasa de aprendizaje
    double gamma = 0.8;           // factor de descuento
    double epsilon_min = 0.2;     // exploracion residual: fraccion de elecciones al azar tras la fase de aprendizaje
    double learning_loop = 0.05;  // fraccion de cada fase con exploracion pura
    double eta = 0.8;             // peso de la mejora sobre la mejor solucion frente a la de la actual

    void set(const std::string& kv) {
        size_t eq = kv.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Parametro sin '=': " + kv);
        std::string k = kv.substr(0, eq);
        std::string v = kv.substr(eq + 1);

        if      (k == "alpha")            alpha = std::stod(v);
        else if (k == "gamma")            gamma = std::stod(v);
        else if (k == "epsilon_min")      epsilon_min = std::stod(v);
        else if (k == "learning_loop")    learning_loop = std::stod(v);
        else if (k == "eta")              eta = std::stod(v);
        else throw std::runtime_error("Parametro desconocido: " + k);
    }
};

#endif // PARAMS_H
