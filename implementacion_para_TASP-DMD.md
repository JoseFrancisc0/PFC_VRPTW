# Estructura e Implementación del Algoritmo ALNS con Q-Learning

## 1. Estructura e Implementación del ALNS Base

El algoritmo ALNS está estructurado en una arquitectura de dos bucles anidados (*two-layer loop*) para optimizar de forma simultánea el modo de los muelles, la asignación de camiones y su programación\[^67\]:

* **Bucle externo (Decisión de modo de muelle):** Aplica operadores de perturbación ($p$) que modifican la configuración operativa de los muelles (asignándolos como solo descarga, solo carga o modo mixto)\[^68\].
* **Bucle interno (Asignación y programación de camiones):** Bajo la configuración de muelles fijada en el bucle externo, ejecuta una búsqueda local utilizando pares de operadores de destrucción (*destroy*) y reparación (*repair*)\[^67\].

### Operadores de Búsqueda Local y Filtrado (Operator Filtering - OF)

Inicialmente, se diseñaron 16 combinaciones de operadores de búsqueda local\[^9\]\[^10\]:

* **Operadores de destrucción:**
  * $rRd$: Eliminación aleatoria de un camión\[^9\]\[^11\].
  * $rMxTar$: Eliminación del camión con mayor retraso/tardanza\[^9\]\[^11\].
  * $rMxM$: Eliminación del camión con mayor distancia ponderada de transporte\[^9\]\[^11\].
* **Operadores de reparación y ajuste:**
  * $iBck$: Intercambiar con el predecesor\[^9\].
  * $iFwd$: Intercambiar con el sucesor\[^9\].
  * $iSwap$: Intercambiar con otro camión de carga/descarga\[^9\].
  * $iUp$: Reorganizar a un muelle con mejor ranking de distancia\[^9\].
  * $iDown$: Reorganizar a un muelle con peor ranking\[^9\].
  * $iDockInsert$: Probar inserción en todas las posiciones del mismo muelle\[^9\].
  * $iBtwInsert$: Inserción aleatoria entre secuencias de distintos muelles\[^9\].
* **Operadores integrales:**
  * $riInD2D$: Intercambio entre muelles de descarga\[^9\].
  * $riOuD2D$: Intercambio entre muelles de carga\[^9\].
  * $riFlxD2D$: Intercambio entre muelles mixtos\[^9\].

Mediante un experimento de filtrado de operadores (OF) basado en pruebas de dominancia estadística de Wilcoxon (con un umbral de dominancia del $40\%$), el conjunto inicial de 16 combinaciones se redujo a los 3 operadores de mejor rendimiento\[^15\]:

1. $a_1$ ($op_2$): $rRd$ & $iFwd$ (Ajuste de secuencia)\[^10\]\[^17\].
2. $a_2$ ($op_9$): $rMxM$ & $iUp$ (Ajuste de secuencia y muelle)\[^10\]\[^17\].
3. $a_3$ ($op_{11}$): $rRd$ & $iBtwInsert$ (Ajuste de secuencia y muelle)\[^10\]\[^17\].

> **Perturbación (bucle externo):** Tras el filtrado, se determinó que la combinación óptima consiste en un único operador eficaz de perturbación ($P_1$: cambiar el modo de un muelle aleatorio a descarga) junto a los 3 operadores de búsqueda local ($P+3op$)\[^8\].

### Criterios de Aceptación Multi-objetivo

Dado que el modelo optimiza tres objetivos de forma jerárquica\[^16\]:

* $f_1$: Minimizar tardanza total de camiones.
* $f_2$: Minimizar *makespan* (tiempo máximo de finalización).
* $f_3$: Minimizar distancia de transporte de vehículos guiados autónomos (AGVs).

El algoritmo mantiene y actualiza un frente/conjunto de soluciones Pareto\[^23\]. Una nueva solución $\chi'$ se acepta bajo cuatro condiciones\[^23\]:

1. Si domina a la mejor solución global ($\chi^*$), actualiza $\chi^*$\[^23\]\[^24\].
2. Si domina a la mejor solución local ($\chi$), actualiza $\chi$\[^23\]\[^24\].
3. Si es mutuamente no dominada con respecto a la mejor solución global, se añade al conjunto Pareto\[^23\]\[^24\].
4. Si no cumple las anteriores, se acepta probabilísticamente siguiendo el criterio de temperatura de Recocido Simulado (Metropolis)\[^23\]\[^25\]. La temperatura inicial se ajusta como:
   $$T = -\frac{\sum f_k(sol)}{\log(0.5)}$$
   y decrece gradualmente\[^24\]\[^25\].

---

## 2. Hibridación con Q-Learning (AOS Adaptativo)

El algoritmo sustituye la Selección Adaptativa de Operadores (AOS) tradicional (basada en ruleta o puntuaciones fijas) por un modelo de Q-Learning\[^7\]:

* **Espacio de Acciones ($A$):** Corresponde al conjunto filtrado de combinaciones de operadores de búsqueda local:
  $$A = \{1, 2, 3\}\quad [^29]$$
* **Espacio de Estados ($S$):** Se define como un conjunto binario:
  $$S = \{0, 1\}\quad [^32]$$
  * $s' = 1$: La búsqueda local en la iteración reciente logró una mejor solución (mejora global o local).
  * $s' = 0$: No se logró ninguna mejora (señal de estancamiento en un óptimo local)\[^32\].

### Esquema de Recompensa (Reward Function)

Utiliza un mecanismo basado en valor con penalización por costo de oportunidad\[^33\]\[^34\]:

1. Calcula la mejora relativa global ($\Delta Global$) y local ($\Delta Local$)\[^34\].
2. Combina ambas mediante un peso $\eta$\[^34\]:
   $$\Delta Improvement = \Delta Global \cdot \eta + \Delta Local \cdot (1 - \eta)$$
3. **Si hay mejora:**
   $$r = \frac{\Delta Improvement \cdot e}{C}$$
   *(donde $e$ es el número de iteración actual y $C$ es una constante de normalización)*\[^34\].
4. **Si no hay mejora:** Aplica una penalización por costo de oportunidad ($OC$, equivalente al valor máximo de mejora previa registrado)\[^34\]\[^36\]:
   $$r = \frac{(\Delta Improvement - OC) \cdot e}{C}$$

### Ecuación de Actualización de la Tabla Q

$$Q(s, a) \leftarrow Q(s, a) + \alpha \left( r + \gamma \max_{a'} Q(s', a') - Q(s, a) \right)$$

* $\alpha$: Tasa de aprendizaje\[^37\]\[^38\].
* $\gamma$: Factor de descuento\[^37\]\[^38\].

### Política de Selección de Acciones ($\epsilon$-greedy con decaimiento)

* **Fase inicial:** Durante los primeros $l$ ciclos del bucle de aprendizaje (*learning loop*), las acciones se seleccionan de forma completamente aleatoria para explorar la matriz Q inicial\[^39\].
* **Fase posterior:** En cada iteración se selecciona una acción aleatoria con probabilidad $\epsilon$ (exploración) o la acción con mayor valor $\max_{a'} Q(s', a')$ con probabilidad $1 - \epsilon$ (explotación)\[^37\]\[^38\].
* **Decaimiento:** En cada paso, el valor de exploración se reduce multiplicándolo por un factor de decaimiento:
  $$\epsilon \leftarrow \epsilon \cdot \beta\quad [^37][^38]$$

---

## 3. Detalles de Implementación e Hiperparámetros Seleccionados

Los hiperparámetros fueron sintonizados mediante la Metodología de Superficie de Respuesta (RSM) combinada con un diseño Box-Behnken (evaluando 112 combinaciones en 10 instancias)\[^40\].

| Hiperparámetro | Notación | Niveles evaluados (-1, 0, 1) | Valor Óptimo Ajustado (*Tuned Value*) | Descripción y Función en el Algoritmo |
| :--- | :---: | :---: | :---: | :--- |
| **Escala de temperatura** | $\tau$ | $0.01,\; 0.105,\; 0.2$ | $0.2$ | Controla la escala de temperatura inicial para el criterio de aceptación de Recocido Simulado\[^41\]. |
| **Exploración Inicial ($\epsilon$-greedy)** | $\epsilon$ | $0.7,\; 0.85,\; 1.0$ | $1.0$ | Probabilidad inicial de elegir una acción aleatoria (comienza con 100% exploración)\[^41\]. |
| **Decaimiento de $\epsilon$** | $\beta$ | $0.99,\; 0.995,\; 1.0$ | $0.99$ | Factor multiplicativo para reducir gradualmente $\epsilon$ hacia la explotación ($\epsilon \leftarrow \epsilon \cdot \beta$)\[^37\]\[^41\]. |
| **Tasa de aprendizaje** | $\alpha$ | $0.5,\; 0.75,\; 1.0$ | $0.5$ | Ponderación de la nueva información/recompensa recibida en la actualización de la tabla Q\[^38\]\[^41\]. |
| **Factor de descuento** | $\gamma$ | $0.7,\; 0.85,\; 1.0$ | $0.7$ | Importancia otorgada a las recompensas futuras esperadas en la ecuación de Q-Learning\[^38\]\[^41\]. |
| **Límite de no mejora** | $t$ | $10,\; 25,\; 40$ | $20$ | Número máximo de iteraciones consecutivas sin mejora en el bucle interno antes de detener la búsqueda local\[^25\]\[^41\]. |
| **Peso de mejora global** | $\eta$ | $0.6,\; 0.7,\; 0.8$ | $0.8$ | Otorga 80% de peso a la mejora global ($\Delta Global$) y 20% a la local ($\Delta Local$) en el cálculo de la recompensa\[^34\]\[^41\]. |
| **Bucle de aprendizaje inicial** | $l$ | $100,\; 200,\; 300$ | $200$ | Número de iteraciones iniciales con selección aleatoria pura para inicializar los valores de la tabla Q\[^39\]\[^41\]. |

---

## 4. Evaluación Experimental y Comparativas

Se comparó **Q-ALNS** contra los siguientes algoritmos y solvers de referencia:
* **RLNS:** ALNS con selección aleatoria de operadores.
* **S-ALNS:** ALNS tradicional basado en puntuación / rueda de ruleta.
* **ALNS 1, ALNS 2, ALNS 3:** Variantes limitadas a un solo operador.
* **Gurobi 11.0.1:** Solver matemático comercial exacto.

### 4.1. Brecha de Optimidad (RPD) y Tiempo de Cómputo (CPU)

#### Comparación de Rendimiento de Solución

| Referencia de Comparación | Mejora en ARPD (Desviación Promedio) | Mejora en RPD Mejor Solución (BS) |
| :--- | :---: | :---: |
| **Frente a RLNS (Selección aleatoria)** | $+9.0\%$ | $+52.9\%$ |
| **Frente a S-ALNS (Selección por puntuación)** | $+9.4\%$ | $+49.8\%$ |

* **Variantes individuales vs. Múltiples operadores:** Tanto RLNS como Q-ALNS superaron categóricamente a las variantes de operador único (ALNS 1, ALNS 2 y ALNS 3), demostrando que la cooperación y combinación de múltiples operadores es indispensable para la competitividad heurística.

#### Escalabilidad y Tiempo de Cómputo

| Tipo de Instancia | Configuración (Muelles, Camiones) | Tiempo Q-ALNS | Tiempo RLNS | Observación / Comportamiento |
| :--- | :---: | :---: | :---: | :--- |
| **Pequeña** | 3 muelles, 20 camiones | $28.7\text{ s}$ | $25.3\text{ s}$ | Ligera sobrecarga inicial debida a la inicialización y actualización de la matriz Q. |
| **Grande** | 10 muelles, 200 camiones | $1973.5\text{ s}$ | $2172.7\text{ s}$ | Q-ALNS converge más rápido gracias a la política dirigida, reduciendo el tiempo total en instancias complejas. |

* **Comportamiento de Convergencia:** Las curvas de convergencia de Q-ALNS exhiben un descenso inicial significativamente más rápido, alcanzando valores finales más bajos con trayectorias notablemente más suaves y estables.

### 4.2. Calidad del Frente de Pareto (Evaluación Multi-objetivo)

#### Comparativa con Gurobi 11.0.1 (Instancias pequeñas: 6_20, 7_20, 8_20)

| Métrica Multi-objetivo | Q-ALNS | Gurobi 11.0.1 | Conclusión |
| :--- | :---: | :---: | :--- |
| **Ratio de No-Dominancia (NR)** | **$51.9\%$** | $48.1\%$ | Mayor porcentaje de soluciones no dominadas en el frente combinado. |
| **Hipervolumen (HV)** | **$0.638$** | $0.577$ | Exploración más extensa y completa del espacio de soluciones. |

#### Comparativa con RLNS (Instancias grandes: 3_20 a 10_200)

| Métrica Multi-objetivo | Q-ALNS | RLNS | Conclusión / Interpretación |
| :--- | :---: | :---: | :--- |
| **Ratio de No-Dominancia (NR)** | **$0.653$** | $0.347$ | Q-ALNS domina con holgura a las soluciones obtenidas por selección aleatoria. |
| **Hipervolumen (HV)** | **$0.732$** | $0.668$ | Mayor cobertura del volumen objetivo. |
| **Conteo de Clusters Jerárquicos (HCC)** | **$15.011$** | $8.524$ | Mayor diversidad y mejor dispersión/distribución de soluciones en el Frente de Pareto. |