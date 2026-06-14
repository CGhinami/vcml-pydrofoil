#!/usr/bin/env python3
import subprocess
import re
import csv
import sys
import time
from datetime import datetime

# Konfiguration
NUM_RUNS = 10
LAUNCH_SCRIPT = "./launch.sh"

def run_benchmark(cfg_file, run_index):
    print(f"[{run_index}/{NUM_RUNS}] Führe Benchmark aus...")
    
    # Starte den Prozess und fange die Ausgabe ein
    result = subprocess.run([LAUNCH_SCRIPT, cfg_file], capture_output=True, text=True)
    
    output = result.stdout + result.stderr
    print(output)

    if result.returncode != 0:
        print(f"WARNUNG: Lauf {run_index} endete mit Fehlercode {result.returncode}!")
    
    # Reguläre Ausdrücke (Regex), um die Werte aus den vcml::log_info Zeilen zu fischen.
    # Sie suchen nach dem Text und fangen die Zahl (mit Punkt oder Komma) ein.
    regex_duration = r"duration\s*:\s*([\d\.]+)s"
    regex_runtime  = r"runtime\s*:\s*([\d\.]+)s"
    regex_core0    = r"cycles core 0\s*:\s*(\d+)"
    regex_core1    = r"cycles core 1\s*:\s*(\d+)"
    regex_mips     = r"sim speed\s*:\s*([\d\.]+)\s*MIPS"
    regex_ratio    = r"realtime ratio\s*:\s*([\d\.]+)\s*/"

    # Extrahiere die Daten (liefert None, falls nicht gefunden)
    def extract_val(regex, text, type_cast=float):
        match = re.search(regex, text)
        return type_cast(match.group(1)) if match else None

    # Sammle die Daten für diesen Lauf
    data = {
        "run": run_index,
        "duration_s": extract_val(regex_duration, output),
        "runtime_s": extract_val(regex_runtime, output),
        "cycles_core0": extract_val(regex_core0, output, int),
        "cycles_core1": extract_val(regex_core1, output, int),
        "sim_speed_mips": extract_val(regex_mips, output),
        "realtime_ratio": extract_val(regex_ratio, output)
    }
    
    # Kurzer Check, ob das Skript erfolgreich geparst hat
    if data["runtime_s"] is None:
        print("FEHLER: Konnte Benchmark-Ausgaben nicht finden. Schau ins raw_log!")
        # Speichere Raw-Output zur Fehlersuche
        with open(f"error_log_run{run_index}.txt", "w") as f:
            f.write(output)
            
    return data

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 run_benchmarks.py <path_to_cfg>")
        sys.exit(1)
        
    cfg_file = sys.argv[1]
    
    # Generiere einen eindeutigen Dateinamen
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_filename = f"2elfs_async_rate10__q1000_{timestamp}.csv"
    
    all_results = []
    
    for i in range(1, NUM_RUNS + 1):
        data = run_benchmark(cfg_file, i)
        all_results.append(data)
        time.sleep(1) # Kurze Pause (Cooldown für die CPU)
        
    # Schreibe CSV
    # Verwende die Keys des ersten Dictionarys als Spaltenüberschriften
    fieldnames = all_results[0].keys()
    
    with open(csv_filename, mode='w', newline='') as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        
        writer.writeheader()
        for row in all_results:
            writer.writerow(row)
            
    print(f"\n✅ Alle {NUM_RUNS} Läufe beendet. Ergebnisse gespeichert in: {csv_filename}")
    
    # Kurze Vorschau/Statistik
    runtimes = [r["runtime_s"] for r in all_results if r["runtime_s"] is not None]
    if runtimes:
        avg_runtime = sum(runtimes) / len(runtimes)
        print(f"📊 Durchschnittliche Runtime: {avg_runtime:.4f}s")

if __name__ == "__main__":
    main()