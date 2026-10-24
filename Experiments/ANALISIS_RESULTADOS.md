# Análisis de resultados: ALNS clásico vs ALNS con Q-learning

Campañas completas con el código del commit `7960966` (Q-learning con la regla de exploración para rutas largas).

- Solomon: 56 instancias × 10 corridas.
- Homberger: 200, 400, 600 y 800 clientes, 60 instancias × 10 corridas cada uno.

## 1. Qué se compara

Los dos métodos comparten todo el ALNS base:

- solución inicial;
- las dos fases (primero reducir vehículos, luego distancia con recocido simulado);
- 25 000 iteraciones por fase;
- operadores;
- ruleta adaptativa para el operador de **reparación**.

Lo único que cambia es cómo se elige el operador de **destrucción**:

| Método | Selección del destructor |
|---|---|
| CLASSIC | Ruleta adaptativa de Ropke & Pisinger, por segmentos de 100 iteraciones. |
| QLEARNING | Un agente Q-learning elige un destructor por episodio de 50 iteraciones. En la fase 1 puede además empezar el episodio con una perturbación. |

Detalles del agente:

- **Estado:** la fase y si el episodio anterior mejoró.
- **Recompensa:** la mejora del episodio normalizada por su media móvil.
- **Política:** epsilon-greedy.
- **Regla de rutas largas:** si al empezar la fase 2 hay 20 o más clientes por ruta (clases C2, R2 y RC2), el piso de epsilon sube de 0.1 a 0.5.

Cada corrida usa la misma semilla en ambos métodos (corrida *k* = semilla *k* en Homberger). Así la comparación es pareada por instancia y por semilla.

## 2. Cómo se mide

Por instancia se promedian las 10 corridas de cada método.

- **Criterio de victoria (lexicográfico, como el BKS):** primero el número medio de vehículos. Si empata, la distancia media.
- **Distancia con misma flota:** diferencia porcentual de distancia, solo en las instancias con igual flota media. **Negativo = Q-learning mejor.** Se prueba con Wilcoxon de rangos con signo (aproximación normal).
- **Prueba de signos:** sobre victorias contra derrotas, sin contar empates.
- **Gap vs BKS:** respecto de las mejores soluciones conocidas publicadas por SINTEF. Promedio por instancia, en vehículos y en distancia. Cuando la flota difiere, el gap de distancia no es comparable directamente: con menos vehículos suele recorrerse más.

Los p-valores de Wilcoxon se calcularon con aproximación normal. Los notebooks y `analyze_results_solomon.py` usan `scipy`, que puede dar valores algo distintos (en Solomon, 0.136 frente a 0.171). Las conclusiones no cambian.

## 3. Resumen global

| Benchmark | Gana / empata / pierde | Prueba de signos p | Flota mejor / peor | Δ vehículos medio | Distancia con misma flota | Wilcoxon p |
|---|---|---|---|---|---|---|
| Solomon (100) | 21 / 17 / 18 | 0.75 | 4 / 5 | +0.013 | −0.14 % (n=47) | 0.17 |
| GH 200 | **41** / 5 / 14 | **0.0004** | 3 / 0 | −0.017 | **−0.24 %** (n=57) | **0.0002** |
| GH 400 | 35 / 2 / 23 | 0.15 | 6 / 6 | −0.013 | **−0.66 %** (n=48) | **0.003** |
| GH 600 | **45** / 1 / 14 | **0.0001** | 12 / 4 | −0.023 | **−0.92 %** (n=44) | **<0.001** |
| GH 800 | **47** / 0 / 13 | **<0.001** | 12 / 4 | −0.052 | **−1.02 %** (n=44) | **<0.001** |

El tiempo de CPU medio por corrida es prácticamente igual (Solomon 4.9 s / 4.9 s; GH 800: 49.9 s / 50.1 s). El agente no añade costo apreciable.

### Gap medio vs BKS (vehículos % / distancia %)

| Benchmark | CLASSIC | QLEARNING |
|---|---|---|
| Solomon | +1.07 / +0.66 | +1.23 / +0.54 |
| GH 200 | +0.67 / +1.84 | +0.25 / +1.75 |
| GH 400 | +1.57 / +4.31 | +1.42 / +3.26 |
| GH 600 | +1.74 / +7.09 | +1.53 / +5.68 |
| GH 800 | +2.13 / +6.62 | +1.81 / +4.89 |

El gap del ALNS base crece con el tamaño: en distancia pasa de 0.7 % en Solomon a 6–7 % en 600–800. Desde 200 clientes el Q-learning tiene menor gap en ambos criterios, y la reducción del gap de distancia crece con el tamaño: 0.1, 1.0, 1.4 y 1.7 puntos.

## 4. Por clase

G/E/P = gana / empata / pierde. La columna de distancia es la media con misma flota (negativo = Q-learning mejor).

| Clase | Solomon | GH 200 | GH 400 | GH 600 | GH 800 |
|---|---|---|---|---|---|
| C1 | 0/9/0 (0.00) | 5/4/1 (−0.25) | 4/2/4 (−0.97) | 5/1/4 (−1.02) | 5/0/5 (−1.42) |
| R1 | 6/0/6 (−0.53) | 9/0/1 (−0.22) | 8/0/2 (−0.57) | 9/0/1 (−1.30) | 10/0/0 (−1.61) |
| RC1 | 6/0/2 (−0.37) | 8/0/2 (−0.63) | 9/0/1 (−1.46) | 10/0/0 (−1.90) | 10/0/0 (−1.47) |
| C2 | 0/8/0 (0.00) | 3/1/6 (+0.08) | 8/0/2 (−1.57) | 8/0/2 (−1.80) | 10/0/0 (−1.86) |
| R2 | 4/0/7 (0.00) | 9/0/1 (−0.27) | 2/0/8 (+0.16) | 6/0/4 (+0.06) | 3/0/7 (+0.25) |
| RC2 | 5/0/3 (−0.09) | 7/0/3 (−0.14) | 4/0/6 (+0.22) | 7/0/3 (+0.01) | 9/0/1 (−0.37) |

Agrupado por largo de ruta:

| Benchmark | Rutas cortas (C1, R1, RC1): G/P, dist., p | Rutas largas (C2, R2, RC2): G/P, dist., p |
|---|---|---|
| Solomon | 12/8, −0.28 %, p=0.033 | 9/10, −0.03 %, p=0.94 |
| GH 200 | 22/4, −0.37 %, p=0.001 | 19/10, −0.10 %, p=0.086 |
| GH 400 | 21/7, −0.99 %, p<0.001 | 14/16, −0.33 %, p=0.78 |
| GH 600 | 24/5, −1.44 %, p<0.001 | 21/9, −0.25 %, p=0.57 |
| GH 800 | 25/5, −1.51 %, p<0.001 | 22/8, −0.36 %, p=0.69 |

## 5. Lo bueno

1. **La ventaja es significativa desde 200 clientes.** En 200, 600 y 800 el Q-learning gana en más instancias de las que pierde (prueba de signos p<0.001). En 200 y 400 también reduce la distancia con misma flota de forma significativa.
2. **La ventaja crece con el tamaño.** La mejora en distancia con misma flota va de −0.14 % (Solomon, no significativa) a −0.24 %, −0.66 %, −0.92 % y −1.02 %. Esto respalda la hipótesis: cuanto más se aleja el ALNS base del BKS, más importa elegir bien el operador.
3. **No se compra distancia con vehículos.** En Homberger la flota media del Q-learning es igual o menor en todos los tamaños (Δ vehículos de −0.013 a −0.052), y las instancias con mejor flota superan a las de peor flota (3/0, 6/6, 12/4, 12/4).
4. **Es robusto en rutas cortas.** En R1 y RC1 gana prácticamente todas las instancias desde 400 (8–10 de 10), con mejoras de 0.6–1.9 % en distancia.
5. **Mismo costo computacional.** El tiempo por corrida es igual al del clásico.
6. **La regla de rutas largas redujo las pérdidas** sin tocar las clases de rutas cortas: sus resultados son idénticos a los de la versión anterior.

   | Pérdidas en rutas largas | Versión anterior | Versión actual |
   |---|---|---|
   | GH 200 | 11 de 30 | 10 de 30 |
   | GH 400 | 18 de 30 | 16 de 30 |
   | GH 600 | 15 de 30 | 9 de 30 |
   | GH 800 | 19 de 30 | 8 de 30 |

## 6. Lo malo y las limitaciones

1. **En Solomon no hay diferencia significativa** (21/17/18, Wilcoxon p≈0.14–0.17). Las clases C1 y C2 empatan siempre porque ambos métodos llegan al BKS. En R1 el Q-learning logra menos distancia pero usa algo más de flota (gap de vehículos +2.59 % frente a +1.88 %). En 100 clientes el ALNS base ya está cerca de su límite, y la selección de operadores no cambia el resultado.
2. **R2 sigue siendo la clase débil.** Pierde 8/10 en 400 y 7/10 en 800, aunque por márgenes pequeños (+0.16 % y +0.25 %, antes +0.38 % y +0.90 %). En R2 ningún destructor por sí solo supera a la mezcla que mantiene la ruleta. Un agente que tiende a fijar un operador está en desventaja.
3. **En rutas largas la ventaja no es significativa en distancia** en ningún tamaño (p entre 0.09 y 0.78). Lo que gana ahí viene sobre todo de la flota en C2 y RC2 y de C2 en distancia. La significancia global la sostienen las clases de rutas cortas.
4. **En 400 el balance de victorias no es significativo** (35/23, p=0.15) y la flota queda empatada (6/6). Solo la distancia con misma flota es significativa (p=0.003).
5. **La regla de rutas largas tiene dos hiperparámetros elegidos con un subconjunto del benchmark.**
   - **Qué mira el algoritmo:** no usa el nombre ni la clase de la instancia. Al empezar la fase 2 calcula clientes por ruta en su propia solución y, si son 20 o más, sube el piso de epsilon a 0.5.
   - **El umbral de 20:** en todos los tamaños las clases de tipo 1 tienen unos 10–11 clientes por ruta y las de tipo 2, entre 31 y 55. Cualquier umbral entre unos 12 y 30 separa igual, así que el resultado no depende de afinarlo.
   - **El piso de 0.5:** se eligió entre 0.3, 0.5 y 0.7 con las instancias de índices 2, 5 y 8 de GH 200 y GH 400. Luego se validó en los índices 1, 4, 7 y 10 de GH 200.
   - **Cuánto pesa ese ajuste:** sin las instancias de ajuste (quedan 42 por tamaño), GH 200 sigue significativo (29/4/9, signos p=0.002; distancia con misma flota −0.21 %, p=0.001). GH 400 conserva la mejora de distancia (−0.63 %, p=0.037), pero el balance de victorias baja a 23/18 (p=0.53) y la flota queda 3/5.
   - **Sin ajuste:** en GH 600, GH 800 y Solomon no se ajustó nada, y la regla también se activa en las clases de tipo 2 de Solomon.
   - **Otros parámetros del agente:** el horizonte de epsilon, las pruebas mínimas por acción y el tope de la recompensa se fijaron con pruebas cortas en Solomon y en un subconjunto de GH 200.
6. **El tamaño del efecto es moderado.** Mejoras de 0.2–1 % en distancia sobre un gap de 2–7 % frente al BKS. El Q-learning acerca el ALNS al BKS, pero no cierra la brecha.

## 7. Conclusión

Con el mismo ALNS base, cambiar solo el criterio de selección del operador de destrucción da:

- **en 100 clientes:** un resultado equivalente al de la ruleta adaptativa;
- **desde 200 clientes:** una mejora pequeña pero estadísticamente significativa;
- **en 600 y 800 clientes:** una mejora clara, de alrededor de 1 % de distancia con misma flota y menos vehículos en promedio.

La ventaja se concentra en las instancias de rutas cortas (tipo 1). En las de rutas largas (tipo 2) el resultado es, en general, un empate con ligera ventaja en flota, y R2 sigue siendo la única clase donde el clásico es mejor de forma sistemática.

## 8. Archivos y reproducción

**Datos:**
- `results/solomon/master_solomon.csv`
- `results/master_gh{200,400,600,800}.csv`: CLASSIC más QLEARNING, unidos con `unir_maestros.py` desde `results/master_gh*_ql.csv`.

CLASSIC no cambió entre versiones y es determinista por semilla, así que se reutilizó. Solo se volvió a correr QLEARNING.

**BKS:** `sintef/sintef_solomon.csv` y `sintef/sintef_{200,400,600,800}.csv`.

**Versiones del código:**
- Versión actual: commit `7960966`; jobs del cluster 58194–58198.
- Versión anterior, usada en la comparación de la sección 5: `d9951c2` para 600 y 800, y `f312365` para 200 y 400.
