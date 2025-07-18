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

def generate_rainbow_chase_config(num_segments=13, leds_per_segment=45):
    """
    Generates a list of segment configurations, all set to RainbowChase.
    """
    segments = []
    total_leds = num_segments * leds_per_segment

    for i in range(num_segments):
        start_led = i * leds_per_segment
        end_led = start_led + leds_per_segment - 1
        
        segment = {
            "id": i + 1,
            "name": f"rc_segment_{i+1}",
            "startLed": start_led,
            "endLed": end_led,
            "brightness": 200,
            "effect": "RainbowChase",
            "speed": 30, # A good default speed for RainbowChase
        }
        segments.append(segment)

    print(f"\nGenerated {len(segments)} segments, all set to 'RainbowChase'.")
    return segments

def upload_and_save_configuration(ser, segments_to_send):
    """Uploads the segment configuration and then saves it to the device's memory."""
    print("\n--- Uploading and Saving Configuration ---")
    
    # 1. Send the command to initiate the upload process
    ser.flushInput()
    send_command(ser, "setallsegmentconfigs\n")
    if not wait_for_ack(ser):
        print("Error: Did not receive initial ACK. Aborting.")
        return False

    # 2. Send the number of segments
    num_segments = len(segments_to_send)
    count_bytes = struct.pack(">H", num_segments)
    print(f"[SEND] Segment Count: {num_segments} | Bytes: {count_bytes.hex()}")
    ser.write(count_bytes)
    if not wait_for_ack(ser):
        print("Error: Did not receive ACK for segment count. Aborting.")
        return False

    # 3. Send each segment's JSON data
    print(f"\nSending {num_segments} segment configurations...")
    for i, segment_data in enumerate(segments_to_send):
        json_payload = json.dumps(segment_data) + "\n"
        print(f"[SEND] Segment {i+1}/{num_segments}: {json_payload.strip()}")
        ser.write(json_payload.encode("utf-8"))
        if not wait_for_ack(ser):
            print(f"Error: Failed to receive ACK for segment {i+1}. Aborting.")
            return False
        print(f"Successfully sent segment {i+1}.")
        time.sleep(0.1)

    print("\n--- Configuration successfully uploaded! ---")
    
    # 4. Send the save command
    send_command(ser, "saveconfig\n")
    if wait_for_ack(ser):
        print("Configuration saved successfully to device memory.")
    else:
        print("Warning: Did not receive confirmation for save command.")
        return False
        
    return True

# --- Main Script Execution ---

def main():
    parser = argparse.ArgumentParser(description="Quick RainbowChase Configuration Loader")
    parser.add_argument("--port", type=str, required=True, help="Serial port of the Arduino")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate for serial connection")
    parser.add_argument("--num_segments", type=int, default=13, help="Number of segments to create.")
    parser.add_argument("--leds_per_segment", type=int, default=45, help="Number of LEDs in each segment.")
    args = parser.parse_args()

    ser = None
    success = False
    try:
        print(f"Connecting to {args.port} at {args.baud} baud...")
        ser = serial.Serial(args.port, args.baud, timeout=5)
        time.sleep(3)
        ser.flushInput()
        print("Connection established.")

        # Generate the configuration
        rainbow_chase_config = generate_rainbow_chase_config(args.num_segments, args.leds_per_segment)
        
        # Upload and save
        if upload_and_save_configuration(ser, rainbow_chase_config):
            success = True

    except serial.SerialException as e:
        print(f"Error: Could not open serial port {args.port}. {e}")
    except KeyboardInterrupt:
        print("\nOperation cancelled by user.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    finally:
        if ser and ser.is_open:
            ser.close()
            print("\nSerial port closed.")
        
        print("\n" + "="*30)
        if success:
            print("✅ RAINBOWCHASE CONFIGURATION COMPLETE ✅")
        else:
            print("❌ CONFIGURATION FAILED ❌")
        print("="*30)

if __name__ == "__main__":
    main()
