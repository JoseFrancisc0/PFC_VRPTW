import os
import subprocess
import time
import glob
import csv
import threading
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

EXEC_PATH = "../build/ALNS_vrptw.exe"
BENCHMARK_DIR = "../solomon-100"
EXPERIMENTS_DIR = "."

ALGORITMOS = ["CLASSIC", "QLEARNING"]
ITERACIONES = 25000
RUNS = 10
MAX_WORKERS = os.cpu_count() or 4

def obtener_instancias():
    patron = os.path.join(BENCHMARK_DIR, "**", "*.txt")
    instancias = glob.glob(patron, recursive=True)
    return sorted(instancias)

def ejecutar_corrida(inst_path, algo, run):
    inst_name = os.path.basename(inst_path).replace('.txt', '')
    comando = [
        EXEC_PATH,
        inst_path,       
        algo,            
        str(ITERACIONES)
    ]
    veh = 0
    dist = 0.0
    try:
        resultado = subprocess.run(comando, capture_output=True, text=True, check=True)
        import re
        match = re.search(r'\[FINAL_RESULT\] Veh: (\d+), Dist: ([\d.]+)', resultado.stdout)
        if match:
            veh = int(match.group(1))
            dist = float(match.group(2))
            
        return {
            "Instancia": inst_name.lower(),
            "Algoritmo": algo,
            "Run": run,
            "Veh": veh,
            "Dist": dist,
            "Status": "OK"
        }
    except subprocess.CalledProcessError as e:
        return {
            "Instancia": inst_name.lower(),
            "Algoritmo": algo,
            "Run": run,
            "Veh": 0,
            "Dist": 0.0,
            "Status": "ERROR",
            "ErrorMsg": e.stderr
        }

def ejecutar_experimentos():
    instancias = obtener_instancias()
    if not instancias:
        print(f"[ERROR] No se encontraron instancias en {BENCHMARK_DIR}")
        return

    total_instancias = len(instancias)
    total_runs = total_instancias * len(ALGORITMOS) * RUNS
    
    print(f"\n=== Iniciando experimentos PARALELOS: {total_instancias} instancias | {total_runs} ejecuciones | Hilos: {MAX_WORKERS} ===")
    start_total = time.time()
    
    tareas = []
    for inst_path in instancias:
        for algo in ALGORITMOS:
            for run in range(1, RUNS + 1):
                tareas.append((inst_path, algo, run))
                
    agregados = defaultdict(lambda: {
        "Veh_sum": 0, "Dist_sum": 0.0,
        "Best_Veh": float('inf'), "Best_Dist": float('inf'), "Count_OK": 0, "Count_Total": 0,
        "Runs": {}
    })
    agregados_lock = threading.Lock()

    completadas = 0
    csv_path = os.path.join(EXPERIMENTS_DIR, "resultados_ejecuciones_iterativas.csv")
    
    base_fieldnames = [
        "Instancia", "Algoritmo", "Avg_Vehiculos", "Avg_Distancia", 
        "Best_Vehiculos", "Best_Distancia", "Runs_OK"
    ]
    run_fieldnames = []
    for i in range(1, RUNS + 1):
        run_fieldnames.append(f"Veh_Run{i}")
        run_fieldnames.append(f"Dist_Run{i}")

    with open(csv_path, mode='w', newline='') as file:
        writer = csv.DictWriter(file, fieldnames=base_fieldnames + run_fieldnames)
        writer.writeheader()
        
        with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
            futuros = {executor.submit(ejecutar_corrida, inst, algo, run): (inst, algo, run) for (inst, algo, run) in tareas}
            
            for futuro in as_completed(futuros):
                res = futuro.result()
                
                with agregados_lock:
                    completadas += 1
                    key = (res["Instancia"], res["Algoritmo"])
                    ag = agregados[key]
                    ag["Count_Total"] += 1
                    
                    if res["Status"] == "OK":
                        ag["Veh_sum"] += res["Veh"]
                        ag["Dist_sum"] += res["Dist"]
                        ag["Count_OK"] += 1
                        ag["Runs"][res["Run"]] = (res["Veh"], res["Dist"])
                        
                        if res["Veh"] < ag["Best_Veh"]:
                            ag["Best_Veh"] = res["Veh"]
                            ag["Best_Dist"] = res["Dist"]
                        elif res["Veh"] == ag["Best_Veh"] and res["Dist"] < ag["Best_Dist"]:
                            ag["Best_Dist"] = res["Dist"]

                        print(f"[{completadas}/{total_runs}] {res['Algoritmo']} | {res['Instancia'].upper()} | Run {res['Run']} [OK]")
                    else:
                        print(f"[{completadas}/{total_runs}] {res['Algoritmo']} | {res['Instancia'].upper()} | Run {res['Run']} [ERROR FATAL]")
                    
                    if ag["Count_Total"] == RUNS:
                        if ag["Count_OK"] > 0:
                            avg_veh = ag["Veh_sum"] / ag["Count_OK"]
                            avg_dist = ag["Dist_sum"] / ag["Count_OK"]
                            
                            row = {
                                "Instancia": res["Instancia"],
                                "Algoritmo": res["Algoritmo"],
                                "Avg_Vehiculos": round(avg_veh, 2),
                                "Avg_Distancia": round(avg_dist, 2),
                                "Best_Vehiculos": ag["Best_Veh"],
                                "Best_Distancia": round(ag["Best_Dist"], 2),
                                "Runs_OK": ag["Count_OK"]
                            }
                            for i in range(1, RUNS + 1):
                                if i in ag["Runs"]:
                                    row[f"Veh_Run{i}"] = ag["Runs"][i][0]
                                    row[f"Dist_Run{i}"] = round(ag["Runs"][i][1], 2)
                                else:
                                    row[f"Veh_Run{i}"] = ""
                                    row[f"Dist_Run{i}"] = ""
                            
                            writer.writerow(row)
                            file.flush()
                        del agregados[key]
                
    end_total = time.time()
    horas, rem = divmod(end_total - start_total, 3600)
    minutos, segundos = divmod(rem, 60)
    print(f"\n=== Experimentos paralelos terminados en {int(horas)}h {int(minutos)}m {segundos:.2f}s ===")
    print(f"-> Archivo de resultados guardado con exito en: {csv_path}")

if __name__ == '__main__':
    ejecutar_experimentos()
