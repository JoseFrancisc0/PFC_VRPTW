"""Utilidades estadisticas sin dependencias externas (no requiere scipy)."""
import math

VEHICLE_WEIGHT = 10000  # identico a VEHICLE_COST en cost() del solver C++


def costo_unificado(veh, dist):
    return VEHICLE_WEIGHT * veh + dist


def wilcoxon_signed_rank(x, y):
    """Test de Wilcoxon de rangos con signo, bilateral, para muestras pareadas.

    Usa la aproximacion normal con correccion por empates y por continuidad
    (adecuada para n >= ~20 pares no nulos). Las diferencias nulas se descartan
    (metodo de Wilcoxon). Devuelve (W+, W-, n_efectivo, p_valor).
    """
    d = [a - b for a, b in zip(x, y) if abs(a - b) > 1e-9]
    n = len(d)
    if n == 0:
        return 0.0, 0.0, 0, 1.0

    orden = sorted(range(n), key=lambda i: abs(d[i]))
    rangos = [0.0] * n
    empates = []
    i = 0
    while i < n:
        j = i
        while j + 1 < n and abs(abs(d[orden[j + 1]]) - abs(d[orden[i]])) < 1e-9:
            j += 1
        r = (i + j) / 2.0 + 1.0
        for k in range(i, j + 1):
            rangos[orden[k]] = r
        if j > i:
            empates.append(j - i + 1)
        i = j + 1

    w_pos = sum(r for r, di in zip(rangos, d) if di > 0)
    w_neg = sum(r for r, di in zip(rangos, d) if di < 0)

    media = n * (n + 1) / 4.0
    var = n * (n + 1) * (2 * n + 1) / 24.0 - sum(t ** 3 - t for t in empates) / 48.0
    if var <= 0:
        return w_pos, w_neg, n, 1.0
    z = (abs(w_pos - media) - 0.5) / math.sqrt(var)
    p = math.erfc(max(z, 0.0) / math.sqrt(2.0))
    return w_pos, w_neg, n, p


def comparar_pareado(costos_classic, costos_ql):
    """Resumen de una comparacion pareada: IMP (%) sobre la media, victorias,
    empates y derrotas de QL, y p-valor de Wilcoxon."""
    mc = sum(costos_classic) / len(costos_classic)
    mq = sum(costos_ql) / len(costos_ql)
    wins = sum(q < c - 1e-9 for c, q in zip(costos_classic, costos_ql))
    losses = sum(q > c + 1e-9 for c, q in zip(costos_classic, costos_ql))
    ties = len(costos_classic) - wins - losses
    _, _, n, p = wilcoxon_signed_rank(costos_classic, costos_ql)
    return {"imp": (mc - mq) / mc * 100.0, "wins": wins, "ties": ties, "losses": losses, "p": p, "n": n}
