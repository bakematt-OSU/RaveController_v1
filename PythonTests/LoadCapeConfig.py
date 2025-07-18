import serial
import json
import time
import sys
import argparse
import struct
import random

# --- Serial Communication Helpers ---

def send_command(ser, command_str):
    """Sends a command string to the Arduino and prints debug info."""
    print(f"\n[SEND] Command: '{command_str.strip()}'")
    ser.write(command_str.encode("utf-8"))
    print(
        f"[SENT] ASCII: '{command_str.strip()}' | Bytes: {command_str.encode('utf-8').hex()}"
    )

def read_json_response(ser, timeout_s=15):
    """
    Reads from serial until a complete JSON object is received or timeout.
    This function ignores common debug messages from the Arduino.
    """
    json_str_buffer = ""
    start_time = time.time()
    debug_prefixes = ["CMD:", "->", "OK:", "ERR:"]

    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            print(f"[RECV] Line: '{line}'")

            if any(line.startswith(prefix) for prefix in debug_prefixes):
                continue

            json_str_buffer += line
            try:
                # Attempt to parse the buffer on each new line
                return json.loads(json_str_buffer)
            except json.JSONDecodeError:
                # Incomplete JSON, continue buffering
                pass
        time.sleep(0.01)

    print(f"Timeout: No valid JSON received. Buffer: '{json_str_buffer}'")
    return None

def wait_for_ack(ser, timeout_s=5):
    """Waits for an ACK or relevant 'OK' message from the Arduino."""
    print("[RECV] Waiting for ACK...")
    start_time = time.time()
    while time.time() - start_time < timeout_s:
        if ser.in_waiting > 0:
            line = ser.readline().decode("utf-8", errors="ignore").strip()
            print(f"[RECV] Line: '{line}'")
            if "-> Sent ACK" in line or "OK:" in line:
                print("[RECV] ACK received.")
                return True
            elif "ERR:" in line:
                print(f"[RECV] Error detected: {line}")
                return False
        time.sleep(0.01)
    print("[RECV] Timeout: No ACK received.")
    return False

# --- Configuration Generation and Upload ---

def get_available_effects(ser):
    """Fetches the list of available effects from the Arduino."""
    ser.flushInput()
    send_command(ser, "listeffects\n")
    print("Waiting for available effects list...")
    response_json = read_json_response(ser)
    if response_json and "effects" in response_json:
        effects = response_json["effects"]
        print(f"Available effects: {effects}")
        return effects
    print("Failed to get effects list.")
    return []

def generate_custom_config(available_effects, num_segments=13, leds_per_segment=45):
    """
    Generates a list of segment configurations.
    """
    if not available_effects:
        print("Cannot generate config without a list of effects.")
        return []

    segments = []
    total_leds = num_segments * leds_per_segment

    # Segment 0: The 'all' segment, covering the entire strip
    segments.append({
        "id": 0,
        "name": "all",
        "startLed": 0,
        "endLed": total_leds - 1,
        "brightness": 200,
        "effect": "SolidColor",
        "color": 0x100030,  # A dim blue/purple
    })

    # Generate the 13 main segments
    for i in range(num_segments):
        start_led = i * leds_per_segment
        end_led = start_led + leds_per_segment - 1
        
        # Cycle through available effects
        effect_name = available_effects[(i + 1) % len(available_effects)]

        segment = {
            "id": i + 1,
            "name": f"cape_seg_{i+1}",
            "startLed": start_led,
            "endLed": end_led,
            "brightness": random.randint(150, 255),
            "effect": effect_name,
            # Add some default parameters that might be used
            "speed": random.randint(20, 100),
            "color": random.randint(0, 0xFFFFFF),
            "cooling": random.randint(20, 85),
            "sparking": random.randint(50, 200)
        }
        segments.append(segment)

    print(f"\nGenerated {len(segments)} total segments for {total_leds} LEDs.")
    return segments

def upload_configuration(ser, segments_to_send):
    """Uploads the full segment configuration to the device."""
    print("\n--- Initiating Configuration Upload ---")
    
    # 1. Send the command to initiate the process
    ser.flushInput()
    send_command(ser, "setallsegmentconfigs\n")
    if not wait_for_ack(ser):
        print("Error: Did not receive initial ACK. Aborting.")
        return False

    # 2. Send the number of segments (2-byte, big-endian)
    num_segments = len(segments_to_send)
    count_bytes = struct.pack(">H", num_segments)
    print(f"[SEND] Segment Count: {num_segments} | Bytes: {count_bytes.hex()}")
    ser.write(count_bytes)
    if not wait_for_ack(ser):
        print("Error: Did not receive ACK for segment count. Aborting.")
        return False

    # 3. Send each segment's JSON data one by one
    print(f"\nSending {num_segments} segment configurations...")
    for i, segment_data in enumerate(segments_to_send):
        json_payload = json.dumps(segment_data) + "\n"
        print(f"[SEND] Segment {i+1}/{num_segments}: {json_payload.strip()}")
        ser.write(json_payload.encode("utf-8"))
        if not wait_for_ack(ser):
            print(f"Error: Failed to receive ACK for segment {i+1}. Aborting.")
            return False
        print(f"Successfully sent segment {i+1}.")
        time.sleep(0.1) # Small delay for stability

    print("\n--- Configuration successfully uploaded! ---")
    return True

# --- Main Script Execution ---

def main():
    parser = argparse.ArgumentParser(description="Arduino Segment Configuration Loader")
    parser.add_argument(
        "--port",
        type=str,
        required=True,
        help="Serial port of the Arduino (e.g., COM3, /dev/ttyACM0)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Baud rate for serial connection (default: 115200)",
    )
    args = parser.parse_args()

    ser = None
    try:
        print(f"Connecting to {args.port} at {args.baud} baud...")
        ser = serial.Serial(args.port, args.baud, timeout=2)
        time.sleep(3)  # Wait for Arduino to reset
        ser.flushInput()
        print("Connection established.")

        # Step 1: Get available effects from the device
        effects = get_available_effects(ser)
        if not effects:
            print("Could not retrieve effects. Cannot proceed.")
            sys.exit(1)

        # Step 2: Generate the configuration
        custom_config = generate_custom_config(effects)

        # Step 3: Upload the configuration
        upload_configuration(ser, custom_config)

    except serial.SerialException as e:
        print(f"Error: Could not open serial port {args.port}. {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
            print("\nSerial port closed.")

if __name__ == "__main__":
    main()
