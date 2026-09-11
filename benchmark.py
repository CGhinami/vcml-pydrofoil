import subprocess
import os
import csv
import re
import sys

# Name der CSV-Datenbank
CSV_FILENAME = "benchmark_results.csv"

def init_csv():
    """Erstellt die CSV-Datei mit Headern, falls sie noch nicht existiert."""
    if not os.path.isfile(CSV_FILENAME):
        with open(CSV_FILENAME, mode='w', newline='') as f:
            writer = csv.writer(f)
            # Added "iteration" to track the run number (1 to 5)
            writer.writerow([
                "iteration", "n_cores", "quantum", "async", "async_rate", "invalidate_all_regs", 
                "elf", "duration", "runtime", "cycles per core", "nins"
            ])

def run_benchmark(iteration, n_cores, quantum, async_mode, sim_async_rate, inval_regs, elf_path):
    # Added [Run {iteration}] to the print output so you can track progress
    print(f"\n--- Starting Benchmark [Run {iteration}]: {n_cores} cores, quantum={quantum}, async_mode: {async_mode}, async_rate: {sim_async_rate}, inval_refs= {inval_regs}, elf_path= {elf_path} ---")
    
    # Base command: launch script and config file
    cmd = [
        "./launch_benchmark.sh", 
        "benchmark/zepyhr/benchmarkIS.cfg"
    ]
    
    # Append the VCML overrides
    cmd.extend(["-c", f"n_cores={n_cores}"])
    cmd.extend(["-c", f"sim_quantum={quantum}"])
    cmd.extend(["-c", f"sim_async={str(async_mode).lower()}"])
    cmd.extend(["-c", f"sim_async_rate={sim_async_rate}"])
    cmd.extend(["-c", f"sim_inval_regs={str(inval_regs).lower()}"])
    cmd.extend(["-c", f"sim_elf={elf_path}"])

    # Variablen zum Speichern der geparsten Ergebnisse
    res_duration = "N/A"
    res_runtime = "N/A"
    res_cycles = "N/A"
    res_nins = "N/A"

    try:
        # Popen für Live-Output und Parsing
        process = subprocess.Popen(
            cmd, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.STDOUT, 
            text=True
        )

        # Output live ins Terminal drucken und nach Metriken durchsuchen
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

        # Warten, bis der Simulator fertig ist
        process.wait()
        
        if process.returncode == 0:
            print("\n[SUCCESS] Simulation completed.")
        else:
            print(f"\n[FAILED] Simulation crashed with return code {process.returncode}.")

    except Exception as e:
        print(f"\n[FAILED] Execution error: {e}")

    # Speichern der Ergebnisse (inklusive iteration)
    with open(CSV_FILENAME, mode='a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            iteration, n_cores, quantum, async_mode, sim_async_rate, inval_regs, elf_path, 
            res_duration, res_runtime, res_cycles, res_nins
        ])
    
    print(f"[INFO] Saved results for this run to {CSV_FILENAME}")

# Example: Run a sweep over different core counts
if __name__ == "__main__":
    init_csv()
    
    # Hier definierst du deine Liste an Konfigurationen
    configurations = [
        # asnyc = false
        {
            "n_cores": 1,
            "quantum": "1000ns",
            "async_mode": False,
            "sim_async_rate": 10,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "100000ns",
            "async_mode": False,
            "sim_async_rate": 10,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "1000000ns",
            "async_mode": False,
            "sim_async_rate": 10,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        # async = true
        # rate = 1
        {
            "n_cores": 1,
            "quantum": "1000ns",
            "async_mode": True,
            "sim_async_rate": 1,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "100000ns",
            "async_mode": True,
            "sim_async_rate": 1,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "1000000ns",
            "async_mode": True,
            "sim_async_rate": 1,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        # async = true
        # rate = 5
        {
            "n_cores": 1,
            "quantum": "1000ns",
            "async_mode": True,
            "sim_async_rate": 5,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "100000ns",
            "async_mode": True,
            "sim_async_rate": 5,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "1000000ns",
            "async_mode": True,
            "sim_async_rate": 5,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        # async = true
        # rate = 10
        {
            "n_cores": 1,
            "quantum": "1000ns",
            "async_mode": True,
            "sim_async_rate": 10,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "100000ns",
            "async_mode": True,
            "sim_async_rate": 10,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },
        {
            "n_cores": 1,
            "quantum": "1000000ns",
            "async_mode": True,
            "sim_async_rate": 10,
            "inval_regs": True,
            "elf_path": "benchmark/zepyhr/1core/build/zephyrW.elf"
        },




    ]
    
    # Set how many times you want to run each configuration
    NUM_RUNS = 5
    
    # Durch die Liste iterieren und jeden Benchmark N mal starten
    for config in configurations:
        for i in range(1, NUM_RUNS + 1):
            # Pass the iteration number, and unpack the rest of the config
            run_benchmark(iteration=i, **config)