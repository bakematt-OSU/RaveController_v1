import serial
import json
import time
import sys
import argparse

# --- Main Script ---
def test_arduino_segments(serial_port, baud_rate):
    print(f"Connecting to {serial_port} at {baud_rate} baud...")
    try:
        ser = serial.Serial(serial_port, baud_rate, timeout=5)
        time.sleep(2)  # Give some time for the serial connection to establish
        print("Connection established.")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        print("Please check if the Arduino is connected and the port is correct.")
        sys.exit(1)

    all_segments_data = []
    expected_segment_count = -1
    received_segment_count = 0

    try:
        # 1. Send the command to Arduino
        command = "getallsegmentconfigs\n"
        print(f"\nSending command: '{command.strip()}'")
        ser.write(command.encode('utf-8'))
        print(f"[SENT] ASCII: '{command.strip()}' | Bytes: {command.encode('utf-8').hex()}")

        # 2. Wait for the initial segment count message (human-readable string)
        print("Waiting for segment count from Arduino...")
        start_time = time.time()
        while expected_segment_count == -1:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                print(f"[RECV] Line: '{line}'")
                if "-> Sending total segments count via Serial:" in line:
                    try:
                        # Extract the integer count from the end of the line
                        expected_segment_count = int(line.split(':')[-1].strip())
                        print(f"Received segment count: {expected_segment_count}")
                        break
                    except ValueError:
                        print(f"Could not parse segment count from line: {line}")
            
            if time.time() - start_time > 10: # 10 second timeout for initial response
                print("Timeout: Did not receive segment count within 10 seconds.")
                return

            time.sleep(0.01) # Small delay to prevent busy-waiting

        if expected_segment_count == -1:
            print("Failed to get expected segment count. Exiting.")
            return

        # 3. Loop to receive each segment's JSON (human-readable string)
        print(f"\nExpecting {expected_segment_count} segment configurations...")
        while received_segment_count < expected_segment_count:
            json_str = ""
            json_start_time = time.time()
            
            # Read lines until a JSON object is found (starts with '{' and ends with '}')
            while True:
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    print(f"[RECV] Line: '{line}'")
                    # Check if the line is a complete JSON object
                    if line.startswith('{') and line.endswith('}'):
                        json_str = line
                        break
                    # Reset timeout if data is coming in, even if it's just debug messages
                    json_start_time = time.time()
                elif time.time() - json_start_time > 5: # 5 second timeout for each JSON
                    print(f"Timeout: Did not receive full JSON for segment {received_segment_count} within 5 seconds.")
                    print(f"Partial buffer (last line read): {line if 'line' in locals() else ''}")
                    return
                time.sleep(0.001) # Small delay

            try:
                # Attempt to parse the JSON
                segment_data = json.loads(json_str)
                all_segments_data.append(segment_data)
                received_segment_count += 1
                print(f"Received segment {received_segment_count}/{expected_segment_count}: ID {segment_data.get('id', 'N/A')}, Effect: {segment_data.get('effect', 'N/A')}")
                
            except json.JSONDecodeError as e:
                print(f"JSON decoding error: {e}")
                print(f"Problematic string: {json_str}")
                # If JSON is malformed, clear input buffer to try and recover
                ser.flushInput()
                time.sleep(0.1)
            except Exception as e:
                print(f"An unexpected error occurred during JSON processing: {e}")
                ser.flushInput()
                time.sleep(0.1)

    except KeyboardInterrupt:
        print("\nTesting interrupted by user.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        ser.close()
        print("Serial port closed.")
        print("\n--- All Received Segment Data ---")
        if all_segments_data:
            print(json.dumps(all_segments_data, indent=2))
        else:
            print("No segment data received.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Arduino Segment Config Serial Tester")
    parser.add_argument('--port', type=str, required=True,
                        help='The serial port connected to the Arduino (e.g., COM3, /dev/ttyACM0)')
    parser.add_argument('--baud', type=int, default=115200,
                        help='The baud rate for the serial connection (default: 115200)')
    args = parser.parse_args()
    test_arduino_segments(args.port, args.baud)