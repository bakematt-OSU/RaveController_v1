import serial
import time
import json
import struct
import random
import argparse # Added for command-line arguments

# --- Configuration ---
# The default port can be overridden by the --port command-line argument
DEFAULT_SERIAL_PORT = 'COM3'
BAUD_RATE = 115200

# --- Binary Command Opcodes (from BinaryCommandHandler.h) ---
CMD = {
    "SET_COLOR": 0x01,
    "SET_EFFECT": 0x02,
    "SET_BRIGHTNESS": 0x03,
    "SET_SEG_BRIGHT": 0x04,
    "SELECT_SEGMENT": 0x05,
    "CLEAR_SEGMENTS": 0x06,
    "SET_SEG_RANGE": 0x07,
    "GET_STATUS": 0x08,
    "BATCH_CONFIG": 0x09,
    "SET_EFFECT_PARAMETER": 0x0A,
    "GET_EFFECT_INFO": 0x0B,
    "SET_LED_COUNT": 0x0C,
    "GET_LED_COUNT": 0x0D,
    "GET_ALL_SEGMENT_CONFIGS": 0x0E,
    "SET_ALL_SEGMENT_CONFIGS": 0x0F,
    "GET_ALL_EFFECTS": 0x10,
    "SET_SINGLE_SEGMENT_JSON": 0x11,
    "SAVE_CONFIG": 0x12,
    "ACK_GENERIC": 0xA0,
    "ACK_EFFECT_SET": 0xA1,
    "ACK_PARAM_SET": 0xA2,
    "ACK_CONFIG_SAVED": 0xA3,
    "ACK_RESTARTING": 0xA4,
    "NACK_UNKNOWN_CMD": 0xE0,
    "NACK_INVALID_PAYLOAD": 0xE1,
    "NACK_INVALID_SEGMENT": 0xE2,
    "NACK_NO_EFFECT": 0xE3,
    "NACK_UNKNOWN_EFFECT": 0xE4,
    "NACK_UNKNOWN_PARAMETER": 0xE5,
    "NACK_JSON_ERROR": 0xE6,
    "NACK_FS_ERROR": 0xE7,
    "NACK_BUFFER_OVERFLOW": 0xE8,
}

# Reverse mapping for easy lookup of response names
RESP_NAMES = {v: k for k, v in CMD.items()}

# --- Parameter Type Enum (from EffectParameter.h) ---
PARAM_TYPE = {
    "INTEGER": 0,
    "FLOAT": 1,
    "COLOR": 2,
    "BOOLEAN": 3,
}


class DeviceTester:
    """
    Handles communication and testing for the Rave Controller.
    """

    def __init__(self, port, baudrate):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.effects_map = {}  # To store discovered effects and their parameters

    def connect(self):
        """Establishes the serial connection."""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=2)
            time.sleep(2)  # Wait for the device to initialize
            print(f"✅ Connected to {self.port} at {self.baudrate} bps.")
            return True
        except serial.SerialException as e:
            print(f"❌ ERROR: Could not open port {self.port}: {e}")
            return False

    def disconnect(self):
        """Closes the serial connection."""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("✅ Disconnected.")

    def send_command(self, name, payload=None):
        """
        Constructs and sends a binary command to the device.
        `payload` should be a list of integers.
        """
        if not self.ser or not self.ser.is_open:
            print("❌ Cannot send command, not connected.")
            return

        opcode = CMD.get(name)
        if opcode is None:
            print(f"❌ Unknown command name: {name}")
            return

        command_bytes = bytearray([opcode])
        if payload:
            command_bytes.extend(payload)

        print(f"🚀 Sending CMD '{name}' (0x{opcode:02X}) with payload: {list(command_bytes)}")
        self.ser.write(command_bytes)
        time.sleep(0.1) # Give the device a moment to process

    def wait_for_response(self, timeout=2.0):
        """
        Waits for and returns a response from the device.
        This can handle single-byte ACKs or multi-byte JSON responses.
        """
        start_time = time.time()
        response_data = bytearray()
        while time.time() - start_time < timeout:
            if self.ser.in_waiting > 0:
                response_data.extend(self.ser.read(self.ser.in_waiting))
                # If it's a potential JSON, wait a bit longer for more data
                if response_data.startswith(b'{') or response_data.startswith(b'['):
                    time.sleep(0.2)
                else: # Likely a single-byte ACK/NACK
                    break
        if not response_data:
            print("⏳ Timeout: No response from device.")
            return None
        return response_data

    def check_ack(self, expected_ack_name="ACK_GENERIC"):
        """Waits for a response and checks if it's the expected ACK."""
        expected_ack_code = CMD.get(expected_ack_name)
        response = self.wait_for_response()
        if response and len(response) == 1 and response[0] == expected_ack_code:
            print(f"👍 OK: Received {expected_ack_name} (0x{response[0]:02X})")
            return True
        elif response:
            resp_name = RESP_NAMES.get(response[0], "UNKNOWN")
            print(f"👎 FAIL: Expected {expected_ack_name}, but got {resp_name} (0x{response[0]:02X})")
            return False
        else:
            return False # Timeout already printed

    def discover_effects(self):
        """
        Uses the binary protocol to discover all effects and their parameters.
        This is the key to making the script dynamic.
        """
        print("\n" + "="*50)
        print("🔬 Test Case: Discovering All Effects")
        print("="*50)

        # Tell the C++ code to use the binary protocol over serial for this command
        self.ser.write(b"getalleffects\n")
        time.sleep(0.1)

        # First response should be the count of effects
        count_response = self.wait_for_response()
        if not count_response or len(count_response) != 3 or count_response[0] != CMD["GET_ALL_EFFECTS"]:
            print("❌ FAIL: Did not receive the correct effect count header.")
            return False

        effect_count = (count_response[1] << 8) | count_response[2]
        print(f"ℹ️ Device reports {effect_count} effects. Now receiving details...")

        for i in range(effect_count):
            print(f"\n- Requesting effect {i+1}/{effect_count}...")
            # Send an ACK to get the next effect details
            self.send_command("ACK_GENERIC")
            
            # Receive the JSON data for the effect
            json_response = self.wait_for_response(timeout=3.0)
            if not json_response:
                print(f"❌ FAIL: Timed out waiting for effect {i} JSON.")
                return False
            
            try:
                # The response might have serial debug output, find the JSON part
                json_str = json_response.decode('utf-8', errors='ignore')
                json_start = json_str.find('{')
                json_end = json_str.rfind('}') + 1
                if json_start == -1 or json_end == 0:
                     raise json.JSONDecodeError("No JSON object found", json_str, 0)
                
                effect_data = json.loads(json_str[json_start:json_end])
                effect_name = effect_data['effect']
                self.effects_map[effect_name] = effect_data['params']
                self.effects_map[effect_name].insert(0, {'name': 'effect_id', 'value': i}) # Add id for later
                print(f"✅ Discovered Effect: '{effect_name}' with {len(effect_data['params'])} parameters.")
            except (json.JSONDecodeError, KeyError) as e:
                print(f"❌ FAIL: Could not parse JSON for effect {i}. Error: {e}")
                print(f"Raw response was: {json_response}")
                return False
        
        # Final ACK to complete the process
        self.send_command("ACK_GENERIC")
        print("\n✅ Successfully discovered all effects and their parameters.")
        return True

    def run_all_tests(self):
        """Runs all test cases in sequence."""
        if not self.discover_effects():
            print("❌ Aborting tests due to failure in effect discovery.")
            return

        self.test_simple_commands()
        self.test_all_effects_and_params()
        self.test_config_management()

    # --- Individual Test Cases ---

    def test_simple_commands(self):
        print("\n" + "="*50)
        print("🔬 Test Case: Simple Commands")
        print("="*50)

        # Test SET_COLOR
        print("\n--- Testing SET_COLOR (Blue) ---")
        self.send_command("SET_COLOR", payload=[0, 0, 0, 255]) # Seg 0, R, G, B
        self.check_ack()
        time.sleep(1)

        # Test SET_BRIGHTNESS
        print("\n--- Testing SET_BRIGHTNESS (50%) ---")
        self.send_command("SET_BRIGHTNESS", payload=[128])
        self.check_ack()
        time.sleep(1)

        # Test SET_SEG_BRIGHT
        print("\n--- Testing SET_SEG_BRIGHT (100%) ---")
        self.send_command("SET_SEG_BRIGHT", payload=[0, 255]) # Seg 0, Brightness
        self.check_ack()
        time.sleep(1)

        # Test SET_SEG_RANGE
        print("\n--- Testing SET_SEG_RANGE ---")
        self.send_command("SET_SEG_RANGE", payload=[0, 0, 10, 0, 20]) # Seg 0, Start (2 bytes), End (2 bytes)
        self.check_ack()

    def test_all_effects_and_params(self):
        """Dynamically tests every discovered effect and its parameters."""
        print("\n" + "="*50)
        print("🔬 Test Case: All Effects and Parameters")
        print("="*50)
        
        if not self.effects_map:
            print("❌ No effects discovered, skipping test.")
            return

        for effect_name, params in self.effects_map.items():
            effect_id = params[0]['value']
            print(f"\n--- Testing Effect: '{effect_name}' (ID: {effect_id}) ---")

            # 1. Set the effect
            self.send_command("SET_EFFECT", payload=[0, effect_id]) # Seg 0, Effect ID
            if not self.check_ack("ACK_EFFECT_SET"):
                print(f"SKIPPING PARAMETER TESTS FOR '{effect_name}' DUE TO SET_EFFECT FAILURE.")
                continue
            time.sleep(2) # Show the effect with default params

            # 2. Test each parameter for the effect
            for param in params[1:]: # Skip the 'effect_id' we added
                param_name = param['name']
                param_type = param['type'].upper()
                payload = [0, PARAM_TYPE[param_type], len(param_name)] + list(param_name.encode())
                
                print(f"  -> Testing Param: '{param_name}' (Type: {param_type})")

                # Construct a test value and pack it into bytes
                if param_type == "INTEGER":
                    test_val = int((param['min_val'] + param['max_val']) / 2)
                    payload.extend(struct.pack('>i', test_val)) # Pack as big-endian signed int
                    print(f"     Value: {test_val}")
                elif param_type == "FLOAT":
                    test_val = (param['min_val'] + param['max_val']) / 2.0
                    payload.extend(struct.pack('>f', test_val)) # Pack as big-endian float
                    print(f"     Value: {test_val:.2f}")
                elif param_type == "BOOLEAN":
                    test_val = True
                    payload.extend(struct.pack('>?', test_val)) # Pack as bool
                    print(f"     Value: {test_val}")
                elif param_type == "COLOR":
                    test_val = random.randint(0x000000, 0xFFFFFF) # Random color
                    # Payload for color is [alpha, r, g, b] but we only need r,g,b
                    r = (test_val >> 16) & 0xFF
                    g = (test_val >> 8) & 0xFF
                    b = test_val & 0xFF
                    payload.extend([0, r, g, b]) # Alpha is ignored but helps pack
                    print(f"     Value: #{test_val:06X}")

                self.send_command("SET_EFFECT_PARAMETER", payload)
                self.check_ack("ACK_PARAM_SET")
                time.sleep(2) # Show the effect with the new param

    def test_config_management(self):
        print("\n" + "="*50)
        print("🔬 Test Case: Configuration Management")
        print("="*50)

        # Test SAVE_CONFIG
        print("\n--- Testing SAVE_CONFIG ---")
        self.send_command("SAVE_CONFIG")
        self.check_ack("ACK_CONFIG_SAVED")

        # Test GET_ALL_SEGMENT_CONFIGS
        print("\n--- Testing GET_ALL_SEGMENT_CONFIGS ---")
        # Use serial command to trigger binary handler
        self.ser.write(b"getallsegmentconfigs\n")
        response = self.wait_for_response(timeout=3.0)
        if response and response.startswith(b'{'):
            try:
                config_data = json.loads(response.decode())
                print("✅ Received valid segment config JSON.")
                # print(json.dumps(config_data, indent=2))
            except json.JSONDecodeError:
                print("❌ FAIL: Received invalid JSON for segment configs.")
        else:
            print("❌ FAIL: Did not receive segment config JSON.")


if __name__ == "__main__":
    # --- Command-Line Argument Parsing ---
    parser = argparse.ArgumentParser(description="Dynamic test script for the Rave Controller.")
    parser.add_argument('--port', type=str, default=DEFAULT_SERIAL_PORT,
                        help=f"The serial port for the device (e.g., COM3, /dev/ttyACM0). Defaults to {DEFAULT_SERIAL_PORT}.")
    args = parser.parse_args()

    # --- Test Execution ---
    tester = DeviceTester(args.port, BAUD_RATE)
    try:
        if tester.connect():
            tester.run_all_tests()
    except Exception as e:
        print(f"\nAn unexpected error occurred: {e}")
    finally:
        tester.disconnect()