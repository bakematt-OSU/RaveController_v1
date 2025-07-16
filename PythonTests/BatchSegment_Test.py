import serial
import json
import time
import sys
import argparse
import struct # For converting integer to bytes
import random # For generating random-ish values for test data

# --- Helper Functions for Serial Communication ---

def send_command(ser, command_str):
    """Sends a command string to Arduino and prints debug info."""
    print(f"\n[SEND] Command: '{command_str.strip()}'")
    ser.write(command_str.encode('utf-8'))
    print(f"[SENT] ASCII: '{command_str.strip()}' | Bytes: {command_str.encode('utf-8').hex()}")

def read_line_with_timeout(ser, timeout_s=5, expected_prefix=None):
    """Reads a line from serial with a timeout, optionally checking for a prefix."""
    start_time = time.time()
    buffer = ""
    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            try:
                char = ser.read(1).decode('utf-8', errors='ignore')
                buffer += char
                if '\n' in buffer:
                    line = buffer.split('\n')[0].strip()
                    print(f"[RECV] Line: '{line}'")
                    if expected_prefix and not line.startswith(expected_prefix):
                        # If we're expecting a specific prefix and don't get it,
                        # it might be a debug message, so we continue reading.
                        buffer = buffer.split('\n', 1)[1] if '\n' in buffer else ""
                        continue
                    return line
            except UnicodeDecodeError:
                # Handle cases where a byte might not be a valid UTF-8 character
                print("[RECV] UnicodeDecodeError: Skipping byte.")
                buffer = "" # Clear buffer on error to avoid accumulating bad data
        time.sleep(0.001) # Small delay to prevent busy-waiting
    print(f"[RECV] Timeout after {timeout_s}s. No line received or expected prefix '{expected_prefix}' not found.")
    return None

def wait_for_ack(ser, timeout_s=5):
    """Waits for an ACK message from the Arduino."""
    print("[RECV] Waiting for ACK...")
    start_time = time.time()
    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            print(f"[RECV] Line: '{line}'")
            # The Arduino sends "OK: Segment X (Name) config applied." for each segment
            # or "-> Sent ACK" for the initial command.
            if "-> Sent ACK" in line or "OK: Segment" in line or "OK: All segment configurations received" in line:
                print("[RECV] ACK received.")
                return True
            elif "ERR:" in line or "error" in line:
                print(f"[RECV] Error detected: {line}")
                return False
        time.sleep(0.001)
    print("[RECV] Timeout: No ACK received.")
    return False

# --- Arduino Data Fetching Functions ---

def get_available_effects(ser):
    """Fetches the list of available effects from the Arduino."""
    send_command(ser, "listeffects\n")
    print("Waiting for available effects list from Arduino...")
    
    json_start_time = time.time()
    json_str_buffer = ""

    while time.time() - json_start_time < 10: # Increased timeout for full JSON
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            print(f"[RECV] Line: '{line}'")
            
            # Look for the start of the JSON object
            if line.startswith('{'):
                json_str_buffer = line
                # Now read subsequent lines until the JSON is complete
                # This assumes the JSON will end with '}' on its own line or immediately after content
                while not json_str_buffer.endswith('}') and time.time() - json_start_time < 10:
                    if ser.in_waiting > 0:
                        next_line = ser.readline().decode('utf-8', errors='ignore').strip()
                        print(f"[RECV] Line: '{next_line}'")
                        json_str_buffer += next_line
                    time.sleep(0.001)
                break # Found and buffered the full JSON
            
            # Reset timeout if data is coming in, even if it's just debug messages
            json_start_time = time.time()
        time.sleep(0.001) # Small delay

    if not json_str_buffer:
        print("Timeout: Did not receive full JSON for available effects.")
        return []

    try:
        response_json = json.loads(json_str_buffer)
        effects = response_json.get("effects", [])
        print(f"Available effects fetched: {effects}")
        return effects
    except json.JSONDecodeError as e:
        print(f"Error decoding effects JSON: {e}")
        print(f"Problematic string: {json_str_buffer}")
        return []
    except Exception as e:
        print(f"An unexpected error occurred during JSON processing for effects: {e}")
        return []


def get_effect_info(ser, effect_name):
    """Fetches parameter info for a specific effect from the Arduino."""
    # Note: Arduino's handleGetEffectInfo takes segment ID and effect ID.
    # For serial command, it takes segment ID (which we'll use 0 for dummy)
    # and then the effect name as a string.
    # The serial command handler in Arduino maps the name to ID.
    command = f"geteffectinfo 0 {effect_name}\n" # Use segment 0 as a dummy
    send_command(ser, command)
    print(f"Waiting for effect info for '{effect_name}' from Arduino...")

    json_start_time = time.time()
    json_str_buffer = ""

    while time.time() - json_start_time < 10: # Increased timeout for full JSON
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            print(f"[RECV] Line: '{line}'")
            
            # Look for the start of the JSON object, specifically containing "effect" key
            # This makes it more robust against other debug JSONs
            if line.startswith('{') and '"effect":' in line:
                json_str_buffer = line
                # Now read subsequent lines until the JSON is complete
                while not json_str_buffer.endswith('}') and time.time() - json_start_time < 10:
                    if ser.in_waiting > 0:
                        next_line = ser.readline().decode('utf-8', errors='ignore').strip()
                        print(f"[RECV] Line: '{next_line}'")
                        json_str_buffer += next_line
                    time.sleep(0.001)
                break # Found and buffered the full JSON
            
            json_start_time = time.time() # Reset timeout if data is coming in
        time.sleep(0.001) # Small delay

    if not json_str_buffer:
        print(f"Timeout: Did not receive full JSON for effect info for '{effect_name}'.")
        return []

    try:
        response_json = json.loads(json_str_buffer)
        # Verify that the 'effect' key matches the requested effect_name
        if response_json.get("effect") != effect_name:
            print(f"Warning: Received effect info for '{response_json.get('effect')}' but expected '{effect_name}'. Skipping.")
            return []
        
        params = response_json.get("params", [])
        return params
    except json.JSONDecodeError as e:
        print(f"Error decoding effect info JSON for {effect_name}: {e}")
        print(f"Problematic string: {json_str_buffer}")
        return []
    except Exception as e:
        print(f"An unexpected error occurred during JSON processing for effect info: {e}")
        return []

# --- Test Functions ---

def get_all_segment_configs(ser):
    """Sends 'getallsegmentconfigs' and receives all segment JSONs."""
    all_segments_data = []

    send_command(ser, "getallsegmentconfigs\n")

    # Consume initial debug lines until we hit the JSON
    print("Waiting for segment configurations JSON from Arduino...")
    json_start_time = time.time()
    json_str_buffer = ""
    
    while time.time() - json_start_time < 15: # Increased timeout for full JSON
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            print(f"[RECV] Line: '{line}'")
            
            # Look for the start of the JSON array of segments
            if line.startswith('{"segments":'):
                json_str_buffer = line
                # Now read subsequent lines until the JSON is complete
                while not json_str_buffer.endswith(']}') and time.time() - json_start_time < 15:
                    if ser.in_waiting > 0:
                        next_line = ser.readline().decode('utf-8', errors='ignore').strip()
                        print(f"[RECV] Line: '{next_line}'")
                        json_str_buffer += next_line
                    time.sleep(0.001)
                break # Found and buffered the full JSON
            
            # Reset timeout if data is coming in, even if it's just debug messages
            json_start_time = time.time()
        time.sleep(0.001) # Small delay

    if not json_str_buffer:
        print("Timeout: Did not receive full JSON for segment configurations.")
        return None

    try:
        # Attempt to parse the complete JSON
        response_json = json.loads(json_str_buffer)
        all_segments_data = response_json.get("segments", [])
        print(f"Received total segments: {len(all_segments_data)}")
        
    except json.JSONDecodeError as e:
        print(f"JSON decoding error for all segment configs: {e}")
        print(f"Problematic string: {json_str_buffer}")
        return None
    except Exception as e:
        print(f"An unexpected error occurred during JSON processing for all segments: {e}")
        return None
    
    print("\n--- All Received Segment Data ---")
    if all_segments_data:
        print(json.dumps(all_segments_data, indent=2))
    else:
        print("No segment data received.")
    return all_segments_data


def set_all_segment_configs(ser, segments_to_send):
    """Sends 'setallsegmentconfigs' and then the segment data."""
    print("\n--- Initiating Set All Segment Configurations ---")
    
    # 1. Send the initial command (text command to SerialCommandHandler)
    send_command(ser, "setallsegmentconfigs\n")
    if not wait_for_ack(ser):
        print("Failed to receive ACK for setallsegmentconfigs command initiation.")
        return False

    # 2. Send the 2-byte segment count (binary data)
    num_segments = len(segments_to_send)
    count_bytes = struct.pack('>H', num_segments) # >H means Big-endian, unsigned short (2 bytes)
    print(f"[SEND] Segment Count: {num_segments} | Bytes: {count_bytes.hex()}")
    ser.write(count_bytes)
    
    if not wait_for_ack(ser):
        print("Failed to receive ACK for segment count.")
        return False

    # 3. Send each segment's JSON configuration (text data)
    print(f"\nSending {num_segments} segment configurations...")
    for i, segment_data in enumerate(segments_to_send):
        json_segment = json.dumps(segment_data) + "\n" # Add newline for readline on Arduino
        print(f"[SEND] Segment {i+1}/{num_segments} JSON: '{json_segment.strip()}'")
        ser.write(json_segment.encode('utf-8'))
        
        # Wait for ACK for each segment
        if not wait_for_ack(ser):
            print(f"Failed to receive ACK for segment {i+1}. Aborting.")
            return False
        
        print(f"Successfully sent segment {i+1}.")
        time.sleep(0.05) # Small delay between segments

    print("\n--- All segment configurations sent successfully! ---")
    return True

def generate_test_segments(ser, led_count=45): # Default changed to 45
    """
    Generates a set of test segments dynamically:
    - Fetches available effects from Arduino.
    - Creates specific segments as requested.
    - Assigns a random effect and random parameters to each.
    """
    segments = []
    
    # Fetch available effects
    available_effects = get_available_effects(ser)
    if not available_effects:
        print("No effects available from Arduino. Cannot generate dynamic segments.")
        return []

    # Get detailed parameter info for each effect
    effect_param_info = {}
    for effect_name in available_effects:
        effect_param_info[effect_name] = get_effect_info(ser, effect_name)
    
    # Filter out "None" and "SolidColor" for dynamic assignment to avoid static effects
    dynamic_effects = [e for e in available_effects if e not in ["None", "SolidColor"]]
    if not dynamic_effects:
        print("Warning: No dynamic effects available for segment assignment. Using all available effects.")
        dynamic_effects = available_effects # Fallback to all if no "dynamic" ones

    num_dynamic_effects = len(dynamic_effects)
    if num_dynamic_effects == 0:
        print("Error: No effects available to assign to segments.")
        return []

    # Segment 0: The "all" segment (always present, usually covers the whole strip)
    segments.append({
        "id": 0,
        "name": "all",
        "startLed": 0,
        "endLed": led_count - 1,
        "brightness": 150,
        "effect": "SolidColor", # Default for 'all' segment
        "color": 0xFF00FF # Purple
    })

    segment_id_counter = 1 # Start IDs for new segments from 1
    
    # Calculate segment length for even distribution
    # Ensure each segment has at least 1 LED
    segment_length = max(1, led_count // num_dynamic_effects)
    
    current_led_start = 0
    effect_index = 0

    while current_led_start < led_count and effect_index < num_dynamic_effects:
        start_led = current_led_start
        end_led = min(current_led_start + segment_length - 1, led_count - 1)

        # If it's the last segment, ensure it goes to the end of the strip
        if effect_index == num_dynamic_effects - 1:
            end_led = led_count - 1

        if start_led > end_led: # Skip if segment is invalid
            break

        segment = {
            "id": segment_id_counter,
            "name": f"effect_seg_{segment_id_counter}",
            "startLed": start_led,
            "endLed": end_led,
            "brightness": random.randint(100, 255) # Random brightness
        }

        chosen_effect = dynamic_effects[effect_index]
        segment["effect"] = chosen_effect

        # Add parameters for the chosen effect
        if chosen_effect in effect_param_info:
            print(f"DEBUG: Parameters for {chosen_effect} before assignment: {effect_param_info[chosen_effect]}") # ADDED DEBUG LINE
            for param in effect_param_info[chosen_effect]:
                param_name = param["name"]
                param_type = param["type"]
                min_val = param.get("min_val")
                max_val = param.get("max_val")

                if param_type == "integer":
                    segment[param_name] = random.randint(int(min_val if min_val is not None else 0), int(max_val if max_val is not None else 255))
                elif param_type == "float":
                    segment[param_name] = round(random.uniform(min_val if min_val is not None else 0.0, max_val if max_val is not None else 1.0), 2)
                elif param_type == "color":
                    segment[param_name] = random.randint(0, 0xFFFFFF)
                elif param_type == "boolean":
                    segment[param_name] = random.choice([True, False])
        else:
            print(f"Warning: No parameter info found for effect: {chosen_effect}. Skipping parameter assignment.")
        
        segments.append(segment)
        segment_id_counter += 1
        current_led_start = end_led + 1
        effect_index += 1

    print(f"\nGenerated {len(segments)} test segments.")
    return segments


# --- Main Script Execution ---

def main():
    parser = argparse.ArgumentParser(description="Arduino Segment Config Serial Tester")
    parser.add_argument('--port', type=str, required=True,
                        help='The serial port connected to the Arduino (e.g., COM3, /dev/ttyACM0)')
    parser.add_argument('--baud', type=int, default=115200,
                        help='The baud rate for the serial connection (default: 115200)')
    parser.add_argument('--mode', type=str, choices=['get', 'set', 'both'], default='get',
                        help='Operation mode: "get" (read configs), "set" (send sample configs), or "both"')
    parser.add_argument('--led_count', type=int, default=45, # Changed default to 45
                        help='The total number of LEDs on the strip. Used for generating test segment ranges.')
    args = parser.parse_args()

    print(f"Connecting to {args.port} at {args.baud} baud...")
    try:
        ser = serial.Serial(args.port, args.baud, timeout=5)
        time.sleep(2)  # Give some time for the serial connection to establish
        print("Connection established.")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        print("Please check if the Arduino is connected and the port is correct.")
        sys.exit(1)

    try:
        if args.mode == 'get' or args.mode == 'both':
            print("\n--- Running GET Test ---")
            get_all_segment_configs(ser)
            time.sleep(1) # Give some time before the next operation

        if args.mode == 'set' or args.mode == 'both':
            print("\n--- Running SET Test ---")
            # Generate sample segment configurations automatically
            test_segments = generate_test_segments(ser, args.led_count)
            if test_segments: # Only proceed if segments were successfully generated
                set_all_segment_configs(ser, test_segments)
                time.sleep(1) # Give some time after sending

                # Optional: After setting, you might want to get the configs again to verify
                if args.mode == 'set': # Only if 'set' was the primary mode
                    print("\n--- Verifying SET by running GET again ---")
                    get_all_segment_configs(ser)
            else:
                print("Skipping SET test as no segments were generated.")

    except KeyboardInterrupt:
        print("\nTesting interrupted by user.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        ser.close()
        print("Serial port closed.")

if __name__ == "__main__":
    main()
