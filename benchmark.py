import subprocess
import os
import csv
import re
import sys
import json

# Name der CSV-Datenbank
CSV_FILENAME = "benchmark_results.csv"

def init_csv():
    """Erstellt die CSV-Datei mit Headern, falls sie noch nicht existiert."""
    if not os.path.isfile(CSV_FILENAME):
        with open(CSV_FILENAME, mode='w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([
                "iteration", "n_cores", "quantum", "async_mode", "sim_async_rate", 
                "inval_regs", "speedup_wfi", "simulate_atomics", "elf_path", 
                "duration", "runtime", "cycles per core", "nins"
            ])

def get_completed_runs():
    """Liest die CSV-Datei und gibt ein Set mit allen bereits gelaufenen Signaturen zurück."""
    completed = set()
    if os.path.isfile(CSV_FILENAME):
        with open(CSV_FILENAME, mode='r', newline='') as f:
            reader = csv.reader(f)
            next(reader, None)  # Header überspringen
            for row in reader:
                if len(row) >= 9:
                    # Die ersten 9 Spalten definieren eine einzigartige Ausführung.
                    # Alles als String speichern, um Type-Mismatches (z.B. bool vs "True") zu vermeiden.
                    signature = tuple(str(col).strip() for col in row[:9])
                    completed.add(signature)
    return completed

def run_benchmark(iteration, n_cores, quantum, async_mode, sim_async_rate, inval_regs, speedup_wfi, simulate_atomics, elf_path):
    print(f"\n--- Starting Benchmark [Run {iteration}]: {n_cores} cores, quantum={quantum}, async_mode: {async_mode}, async_rate: {sim_async_rate}, inval_regs= {inval_regs}, speedup_wfi: {speedup_wfi}, simulate_atomics: {simulate_atomics}, elf_path= {elf_path} ---")
    
    cmd = [
        "./launch_benchmark.sh", 
        "benchmark/zepyhr/benchmarkIS.cfg"
    ]
    
    cmd.extend(["-c", f"n_cores={n_cores}"])
    cmd.extend(["-c", f"sim_quantum={quantum}"])
    cmd.extend(["-c", f"sim_async={str(async_mode).lower()}"])
    cmd.extend(["-c", f"sim_async_rate={sim_async_rate}"])
    cmd.extend(["-c", f"sim_inval_regs={str(inval_regs).lower()}"])
    cmd.extend(["-c", f"speedup_wfi={str(speedup_wfi).lower()}"])
    cmd.extend(["-c", f"simulate_atomics={str(simulate_atomics).lower()}"])
    cmd.extend(["-c", f"sim_elf={elf_path}"])

    res_duration = "N/A"
    res_runtime = "N/A"
    res_cycles = "N/A"
    res_nins = "N/A"

    try:
        process = subprocess.Popen(
            cmd, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.STDOUT, 
            text=True
        )

        for line in process.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()

            if "duration" in line and ":" in line:
                match = re.search(r"duration\s*:\s*([0-9.]+)", line)
                if match: res_duration = match.group(1)
                
            elif "runtime" in line and ":" in line:
                match = re.search(r"runtime\s*:\s*([0-9.]+)", line)
                if match: res_runtime = match.group(1)
                
            elif "cycles per core" in line and ":" in line:
                match = re.search(r"cycles per core\s*:\s*([0-9]+)", line)
                if match: res_cycles = match.group(1)
                
            elif "nins" in line and ":" in line:
                match = re.search(r"nins\s*:\s*([0-9]+)", line)
                if match: res_nins = match.group(1)

        process.wait()
        
        if process.returncode == 0:
            print("\n[SUCCESS] Simulation completed.")
        else:
            print(f"\n[FAILED] Simulation crashed with return code {process.returncode}.")

    except Exception as e:
        print(f"\n[FAILED] Execution error: {e}")

    with open(CSV_FILENAME, mode='a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            iteration, n_cores, quantum, async_mode, sim_async_rate, inval_regs, speedup_wfi, simulate_atomics, elf_path, 
            res_duration, res_runtime, res_cycles, res_nins
        ])
    
    print(f"[INFO] Saved results for this run to {CSV_FILENAME}")

if __name__ == "__main__":
    init_csv()
    
    # 1. Bisherige Ergebnisse laden
    completed_runs = get_completed_runs()
    
    config_path = "benchmark/zepyhr/configs/4cores.json"
    with open(config_path, "r", encoding="utf-8") as f:
        configurations = json.load(f)
    
    NUM_RUNS = 3
    
    # 2. Durch iterieren und filtern
    for config in configurations:
        for i in range(1, NUM_RUNS + 1):
            
            # Signatur aus der aktuellen Konfiguration erstellen
            # (Format muss zwingend mit row[:9] in get_completed_runs() matchen)
            current_signature = (
                str(i),
                str(config["n_cores"]),
                str(config["quantum"]),
                str(config["async_mode"]),
                str(config["sim_async_rate"]),
                str(config["inval_regs"]),
                str(config["speedup_wfi"]),
                str(config["simulate_atomics"]),
                str(config["elf_path"])
            )
            
            if current_signature in completed_runs:
                print(f"Skipping already completed run: Iteration {i} | quantum: {config['quantum']} | async_rate: {config['sim_async_rate']} | elf: {config['elf_path']}")
                continue
                
            run_benchmark(iteration=i, **config)