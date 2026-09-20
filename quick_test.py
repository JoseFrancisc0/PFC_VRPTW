import subprocess
import time
import os
import csv

EXEC_PATH = "./build/ALNS_VRPTW.exe"
INSTANCES = [
    "solomon-100/C1/c103.txt",
    "solomon-100/C2/c201.txt",
    "solomon-100/R1/r107.txt",
    "solomon-100/R1/r108.txt",
    "solomon-100/R2/r204.txt",
    "solomon-100/RC1/rc106.txt",
    "solomon-100/RC2/rc202.txt",
]
ITERS = "25000"

# Cargar BKS
bks_data = {}
sintef_path = "Experiments/sintef.csv"
if os.path.exists(sintef_path):
    with open(sintef_path, "r", encoding="latin1") as f:
        reader = csv.reader(f, delimiter=';')
        next(reader) # skip header
        for row in reader:
            if len(row) >= 3:
                inst = row[0].lower()
                bks_data[inst] = (int(row[1]), float(row[2]))

print(f"============================ QUICK TEST ({ITERS} Iters) ============================")
print(f"{'Instancia':<10} | {'BKS (NV/TD)':<15} | {'Classic (NV/TD)':<15} | {'Classic Time':<12} | {'Q-Learn (NV/TD)':<15} | {'Q-Learn Time':<12} | {'Gap BKS (%)':<11} | {'Imp ALNS (%)':<12}")
print("-" * 125)

for inst_path in INSTANCES:
    inst_name = inst_path.split('/')[-1].replace('.txt', '').lower()
    
    bks_nv, bks_td = bks_data.get(inst_name, (0, 0.0))
    bks_str = f"{bks_nv}/{bks_td:.2f}" if bks_nv else "?/?"
    
    results = {}
    
    for alg in ["CLASSIC", "QLEARNING"]:
        cmd = [EXEC_PATH, inst_path, alg, ITERS]
        veh, dist = None, None
        
        start_time = time.perf_counter()
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            end_time = time.perf_counter()
            exec_time = end_time - start_time
            
            output = result.stdout.strip()
            for line in output.split('\n'):
                if "[FINAL_RESULT]" in line:
                    parts = line.replace("[FINAL_RESULT] Veh: ", "").split(", Dist: ")
                    if len(parts) == 2:
                        veh = int(parts[0].strip())
                        dist = float(parts[1].strip())
            
            results[alg] = {"veh": veh, "dist": dist, "time": exec_time}
        except Exception as e:
            results[alg] = {"veh": None, "dist": None, "time": 0.0}

    c_veh = results["CLASSIC"]["veh"]
    c_dist = results["CLASSIC"]["dist"]
    c_time = results["CLASSIC"]["time"]
    
    q_veh = results["QLEARNING"]["veh"]
    q_dist = results["QLEARNING"]["dist"]
    q_time = results["QLEARNING"]["time"]
    
    c_str = f"{c_veh}/{c_dist:.2f}" if c_veh is not None else "ERROR"
    q_str = f"{q_veh}/{q_dist:.2f}" if q_veh is not None else "ERROR"
    
    gap_bks = "??"
    imp_alns = "??"
    
    if q_dist is not None and bks_td > 0:
        val = ((q_dist - bks_td) / bks_td) * 100
        gap_bks = f"{val:+.2f}%"
        
    if c_dist is not None and q_dist is not None and c_dist > 0:
        val = ((c_dist - q_dist) / c_dist) * 100
        imp_alns = f"{val:+.2f}%"
        
    print(f"{inst_name.upper():<10} | {bks_str:<15} | {c_str:<15} | {c_time:<10.2f}s | {q_str:<15} | {q_time:<10.2f}s | {gap_bks:<11} | {imp_alns:<12}")

print("-" * 125)
