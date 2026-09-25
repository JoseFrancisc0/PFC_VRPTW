#ifndef TUNING_H
#define TUNING_H

// Interruptores de las mejoras incrementales, reunidos en un solo lugar para
// poder ablacionarlas de a una sin tocar los solvers.
//
// Estan ordenados de la mas nueva a la mas vieja: apagandolos de arriba hacia
// abajo se retrocede paso a paso y, con todos en su valor "off", se vuelve
// exactamente al comportamiento de la version anterior.
//
// Todos los ajustes son compartidos por ALNS y ALNS_QLearning a proposito: la
// unica diferencia entre ambos debe seguir siendo el criterio de seleccion de
// operadores.
namespace tuning {

// --- (3) Fase de eliminacion de ruta con pool de eyeccion ------------------
// Tras vaciar la ruta mas chica, si algun cliente no entra en las rutas
// restantes, se sacuden EJECTION_SHAKE clientes al azar y se reintenta, hasta
// EJECTION_BUDGET veces. Es la adaptacion a VRPTW de la minimizacion de rutas
// de Nagata & Braysy, y materializa el bucle de dos capas del paper: capa
// externa = el intento de eliminar una ruta, capa interna = la busqueda que
// trata de hacer caber a los clientes varados.
//
// OFF: EJECTION_BUDGET = 0 (queda solo el intento simple del punto 1).
// Costo: hasta BUDGET reparaciones extra en las iteraciones en que se dispara
// removeSmallestRoute (~1 de cada 6). Es el ajuste mas caro en tiempo.
inline constexpr int EJECTION_BUDGET = 10;
inline constexpr int EJECTION_SHAKE  = 3;

// --- (2) Reinicio por estancamiento ----------------------------------------
// Adaptacion del limite t de no mejora del paper (alli t=20 para su bucle
// interno; aca la escala de iteraciones es otra). Al cumplirse, la busqueda
// vuelve a la mejor solucion conocida y recalienta la temperatura, que es el
// analogo del operador de perturbacion de la capa externa.
//
// OFF: STAGNATION_LIMIT = 0.
inline constexpr int    STAGNATION_LIMIT = 1500;
inline constexpr double REHEAT_FACTOR    = 0.5;

// --- (1) Escala de temperatura inicial (tau) -------------------------------
// El paper sintonizo tau = 0.2 por RSM/Box-Behnken; el valor original aca era
// 0.10. OJO: se aplica sobre la distancia inicial, NO sobre cost(). Calibrarla
// sobre cost() -- que es lo que hace el paper con Sum f_k -- daria T ~ 29000 y
// el SA empezaria a aceptar libremente soluciones con MAS vehiculos, rompiendo
// la jerarquia lexicografica del VRPTW. Por eso se adapta tau, no la formula.
//
// OFF: TEMP_SCALE = 0.10.
inline constexpr double TEMP_SCALE = 0.2;

} // namespace tuning

#endif // TUNING_H
