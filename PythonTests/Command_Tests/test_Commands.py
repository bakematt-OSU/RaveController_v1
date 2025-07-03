import serial
import time

# List of test commands to validate functionality
test_commands = [
    "clearsegments\n",
    "addsegment 0 50\n",
    "addsegment 51 100\n",
    "listsegments\n",
    "select 1\n",
    "setsegrange 1 60 90\n",
    "setcolor 128 64 32\n",
    "seteffect Fire\n",
    "setbrightness 200\n",
    "setsegbrightness 1 150\n",
    "listeffectsjson\n",
    "getstatus\n",         # Replace with proper ASCII if JSON opcode unsupported
    "batchconfig {\"segments\":[{\"start\":0,\"end\":20,\"name\":\"segA\",\"brightness\":100,\"effect\":\"SolidColor\"}],\"brightness\":180,\"color\":[10,20,30]}\n",
    "listsegmentsjson\n"
]

def run_tests(port="/dev/ttyACM0", baud=115200):
    """
    Open the serial port, send each test command, and print the response.
    Adjust `port` to match your system (e.g., COM3 on Windows).
    """
    ser = serial.Serial(port, baud, timeout=1)
    time.sleep(2)  # give Arduino time to reset

    for cmd in test_commands:
        print(f">>> {cmd.strip()}")
        ser.write(cmd.encode())
        time.sleep(0.2)
        # Read and print available lines
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                break
            print(line)
        print()

    ser.close()

if __name__ == "__main__":
    run_tests(port="COM7")  # Adjust port as needed

# Use: python test_commands.py --port COM3 or adjust default port above
