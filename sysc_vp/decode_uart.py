import sys
import re

def decode_log(filepath):
    output = ""
    # This regex looks for lines containing our UART address and captures the hex value at the end
    pattern = re.compile(r"mem\[0x0*1000951C\] <- 0x([0-9a-fA-F]+)")
    try:
        with open(filepath, 'r') as f:
            for line in f:
                match = pattern.search(line)
                if match:
                    # Extract the hex (e.g., '00000042'), convert to integer, then to ASCII char
                    hex_str = match.group(1)
                    char_val = int(hex_str, 16)
                    # Only append valid ASCII characters to avoid terminal garbage
                    if char_val < 128: 
                        output += chr(char_val)
                        
    except FileNotFoundError:
        print(f"Error: Could not find file '{filepath}'")
        sys.exit(1)

    return output

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python decode_uart.py <path_to_your_log_file.txt>")
    else:
        log_file = sys.argv[1]
        decoded_text = decode_log(log_file)
        print("\n--- DECODED UART OUTPUT ---")
        print(decoded_text)
        print("---------------------------\n")