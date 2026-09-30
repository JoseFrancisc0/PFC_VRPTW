#ifndef PARAMS_H
#define PARAMS_H

#include <string>
#include <stdexcept>

// Parametros ajustables en tiempo de ejecucion, pasados a main.cpp como
// argumentos 'clave=valor' despues de la semilla. Permiten barrer
// hiperparametros sin recompilar. Los valores por defecto reproducen la
// configuracion de referencia.
//
// Division importante para la hipotesis de la tesis:
//  - 'destroy_levels' y 'checkpoint_every' son COMPARTIDOS: cambian el pool de
//    operadores o el registro, y se aplican identicos a ALNS y a ALNS_QLearning.
//  - El resto son internos del selector Q-learning (como aprende y como elige),
//    y no tocan nada fuera del criterio de seleccion de operadores.
struct SolverParams {
    // --- Compartidos -------------------------------------------------------
    // Numero de niveles del grado de destruccion q. Con 1, cada destroy toma q
    // uniforme en [q_min, q_max] (comportamiento original). Con L > 1, cada
    // destroy parametrico se desdobla en L operadores, uno por subrango
    // contiguo de [q_min, q_max]: "shaw pequeno" y "shaw grande" pasan a ser
    // operadores distintos del pool. Ambos selectores eligen entre el mismo
    // pool ampliado, asi que la unica diferencia sigue siendo la seleccion.
    int destroy_levels = 1;

    // Si > 0, imprime "[CHECKPOINT] iter veh dist" cada tantas iteraciones
    // (curvas de convergencia / comparacion a presupuestos menores).
    int checkpoint_every = 0;

    // --- Selector Q-learning -----------------------------------------------
    double alpha = 0.5;          // tasa de aprendizaje
    double gamma = 0.7;          // factor de descuento
    double epsilon_min = 0.01;   // piso de exploracion
    double beta = 0.99;          // decaimiento de epsilon por paso
    int learning_loop = 200;     // iteraciones iniciales 100% aleatorias
    double eta = 0.8;            // peso de la mejora global en la recompensa
    double reward_gain = 1.0e4;  // ganancia de la compresion log1p

    // Costo de oportunidad cobrado a las iteraciones sin mejora.
    // 0  -> maximo historico de la mejora (original).
    // >0 -> media movil exponencial de las mejoras con esa tasa: el cobro sigue
    //       la escala de mejora tipica en vez del mayor evento de la corrida.
    double opp_ema = 0.0;

    // Espacio de estados.
    // 0 -> S = {mejoro, no mejoro} (original, 2 estados).
    // 1 -> ademas, 3 niveles de estancamiento (iteraciones sin mejorar el
    //      mejor global), 6 estados: permite aprender operadores distintos
    //      segun la busqueda este progresando o atascada.
    int state_mode = 0;

    void set(const std::string& kv) {
        size_t eq = kv.find('=');
        if (eq == std::string::npos) throw std::runtime_error("Parametro sin '=': " + kv);
        std::string k = kv.substr(0, eq);
        std::string v = kv.substr(eq + 1);

        if      (k == "destroy_levels")   destroy_levels = std::stoi(v);
        else if (k == "checkpoint_every") checkpoint_every = std::stoi(v);
        else if (k == "alpha")            alpha = std::stod(v);
        else if (k == "gamma")            gamma = std::stod(v);
        else if (k == "epsilon_min")      epsilon_min = std::stod(v);
        else if (k == "beta")             beta = std::stod(v);
        else if (k == "learning_loop")    learning_loop = std::stoi(v);
        else if (k == "eta")              eta = std::stod(v);
        else if (k == "reward_gain")      reward_gain = std::stod(v);
        else if (k == "opp_ema")          opp_ema = std::stod(v);
        else if (k == "state_mode")       state_mode = std::stoi(v);
        else throw std::runtime_error("Parametro desconocido: " + k);

        if (destroy_levels < 1) throw std::runtime_error("destroy_levels debe ser >= 1");
    }
};

#endif // PARAMS_H
