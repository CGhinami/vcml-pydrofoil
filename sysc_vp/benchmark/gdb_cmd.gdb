# Run with gdb-multiarch -x gdb_cmd.gdb <path to the elf>
#set architecture riscv:rv64
#target remote :5555
#hbreak *0x80013F06

# 1. Bestätigungs-Dialoge abschalten
set confirm off

# 2. Die korrekte 64-Bit Architektur einstellen
set architecture riscv:rv64

# 3. Verbindung zum wartenden Simulator herstellen
target remote :5555

# 4. DIE MAGIE: Wir laden die Symbole manuell und verschieben 
# sie auf die Startadresse unseres RAMs (0x80000000)!
add-symbol-file /home/seibt/thesis/avp64_sw/linux/BUILD/buildroot/output/linux/build/opensbi-1.7/build/platform/generic/firmware/fw_jump.elf 0x80000000

# 5. Hardware-Breakpoint direkt auf die Absturz-Adresse setzen
hbreak *0x80013F06
set substitute-path /app/build /home/seibt/thesis/avp64_sw/linux/BUILD

# 6. Lass den Simulator bis genau dorthin laufen
# continue

