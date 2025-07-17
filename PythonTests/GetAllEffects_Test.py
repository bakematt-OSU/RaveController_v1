import serial
import json
import time
import argparse

# Constants based on your Arduino code
CMD_GET_ALL_EFFECTS = b'\x10'  # Binary command for get all effects
CMD_ACK = b'\xA0'              # Binary command for acknowledgment

# --- Configuration ---
DEFAULT_BAUD_RATE = 115200

def run_get_all_effects_test(serial_port, baud_rate):
    """
    Connects to the Arduino, sends the 'getalleffects' command,
    processes the multi-part binary/JSON response, and prints a final
    JSON object containing all effects.
    """
    ser = None  # Initialize ser to None
    try:
        print(f"Attempting to connect to {serial_port} at {baud_rate} baud...")
        # --- Robust Serial Handling ---
        # The 'with' statement is not used here to allow for more complex,
        # multi-step communication logic, but we will use a try/finally
        # block to ensure the port is always closed.
        ser = serial.Serial(serial_port, baud_rate, timeout=5)
        time.sleep(2)  # Give Arduino time to reset and initialize serial

        if not ser.isOpen():
            print("Error: Failed to open serial port.")
            return

        print("Serial port opened successfully.")

        # 1. Send the "getalleffects" command via serial (text command)
        command_text = "getalleffects\n"
        print(f"\nSending command: '{command_text.strip()}'")
        ser.write(command_text.encode())
        time.sleep(0.1) # Short delay for Arduino to process

        print("\n--- Reading Initial Response ---")
        # First, attempt to read the 3-byte binary response directly
        binary_response = b''
        start_time_binary = time.time()
        while len(binary_response) < 3 and (time.time() - start_time_binary < 5):
            if ser.in_waiting > 0:
                # Read whatever is available up to the remaining needed bytes
                binary_response += ser.read(min(ser.in_waiting, 3 - len(binary_response)))

        if len(binary_response) != 3:
            print(f"Error: Did not receive expected 3-byte binary response for effect count. Received: {binary_response.hex()}")
            return

        # 2. Parse the initial 3-byte binary response (command + effect count)
        if binary_response[0] == CMD_GET_ALL_EFFECTS[0]:
            total_effects = (binary_response[1] << 8) | binary_response[2]
            print(f"Received initial binary response (0x{binary_response[0]:02X}, {total_effects} effects).")
            print(f"Total effects to retrieve: {total_effects}")
        else:
            print(f"Error: Expected command byte 0x{CMD_GET_ALL_EFFECTS[0]:02X}, but got 0x{binary_response[0]:02X}.")
            return

        # Now, read and print the subsequent text debug messages
        print("\n--- Arduino Debug Output (after binary count) ---")
        start_time_debug_text = time.time()
        while True:
            try:
                line = ser.readline().decode('utf-8').strip()
                if line:
                    print(line)
                    if "Now waiting for ACK to send first effect..." in line:
                        break # This is the last debug line before expecting ACK
                if time.time() - start_time_debug_text > 5: # Timeout for these debug lines
                    print("Timeout waiting for full debug output lines.")
                    break
            except UnicodeDecodeError:
                # This can happen if we try to decode binary data as text.
                # We can ignore it as we're just looking for the debug text.
                pass
        print("--------------------------------------------------")

        all_effects_data = [] # List to store all received effect JSON objects

        # 3. Loop to request and receive each effect's JSON
        for i in range(total_effects):
            print(f"\n--- Requesting Effect {i+1}/{total_effects} ---")
            # a. Send ACK
            print(f"Sending ACK (0x{CMD_ACK[0]:02X})...")
            ser.write(CMD_ACK)
            time.sleep(0.05) # Small delay for Arduino to process ACK

            # b. Read JSON response for the current effect
            json_buffer = ""
            start_time_json = time.time()
            while True:
                line = ser.readline().decode('utf-8').strip()
                if line:
                    # We need to filter for the JSON, which should be on its own line.
                    if line.startswith('{') and line.endswith('}'):
                        json_buffer = line
                        break # Found the JSON line
                    print(f"Arduino (debug): {line}") # Print other lines as debug
                if time.time() - start_time_json > 10: # Timeout for receiving JSON
                    print(f"Timeout waiting for JSON for effect {i+1}.")
                    break

            if json_buffer:
                try:
                    effect_data = json.loads(json_buffer)
                    # *** MODIFICATION: Print the detailed JSON for each effect as it's received ***
                    print(f"Received JSON for effect {i+1}:")
                    print(json.dumps(effect_data, indent=2))
                    all_effects_data.append(effect_data)
                except json.JSONDecodeError as e:
                    print(f"Error decoding JSON for effect {i+1}: {e}")
                    print(f"Raw JSON buffer: {json_buffer}")
            else:
                print(f"No JSON received for effect {i+1}. Stopping.")
                break # Exit the loop if an effect is missing

        # 4. Print the final combined JSON object
        print("\n" + "="*50)
        print("--- FINAL COMBINED JSON ---")
        print("="*50)
        if all_effects_data:
            print(json.dumps(all_effects_data, indent=2))
        else:
            print("No effects were successfully retrieved.")
        print("\n--- Test Complete ---")

    except serial.SerialException as e:
        print(f"Serial port error: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    finally:
        # This block will run no matter what, ensuring the port is closed.
        if ser and ser.isOpen():
            ser.close()
            print("\nSerial port closed.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test the 'getalleffects' command on an Arduino and get a combined JSON output.")
    parser.add_argument('--port', type=str, required=True,
                        help="Serial port connected to Arduino (e.g., COM3, /dev/ttyUSB0)")
    parser.add_argument('--baud', type=int, default=DEFAULT_BAUD_RATE,
                        help=f"Baud rate for serial communication (default: {DEFAULT_BAUD_RATE})")

    args = parser.parse_args()

    run_get_all_effects_test(args.port, args.baud)
