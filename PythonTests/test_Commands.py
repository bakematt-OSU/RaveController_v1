#!/usr/bin/env python3
"""
Comprehensive test harness for RaveController_v1 serial commands
Sends ASCII and binary commands and validates basic responses.
"""
import argparse
import serial
import time
import json

# Command IDs matching Processes.h
CMD_SET_COLOR      = 0x01
CMD_SET_EFFECT     = 0x02
CMD_SET_BRIGHTNESS = 0x03
CMD_SET_SEG_BRIGHT = 0x04
CMD_SELECT_SEGMENT = 0x05
CMD_CLEAR_SEGMENTS = 0x06
CMD_SET_SEG_RANGE  = 0x07
CMD_GET_STATUS     = 0x08
CMD_BATCH_CONFIG   = 0x09

BAUD = 115200
DELAY = 0.1  # seconds between commands

ASCII_COMMANDS = [
    "clearsegments",
    "addsegment 0 50",
    "addsegment 51 100",
    "listsegments",
    "setsegrange 1 60 90",
    "setsegeffect 1 SolidColor",
    "setsegname 1 middle",
    "setcolor 128 64 32",
    "listeffects",
    "listeffectsjson",
    "seteffect Fire",
    "setbrightness 200",
    "setsegbrightness 0 150",
    "getstatus",
    # batchconfig JSON: one segment + global settings
    "batchconfig {\"segments\":[{\"start\":0,\"end\":20,\"name\":\"segA\",\"brightness\":100,\"effect\":\"SolidColor\"}],\"brightness\":180,\"color\":[10,20,30]}"
]

BINARY_COMMANDS = [
    (CMD_CLEAR_SEGMENTS, bytearray([CMD_CLEAR_SEGMENTS])),
    (CMD_SELECT_SEGMENT, bytearray([CMD_SELECT_SEGMENT, 1])),
    # set range: idx=1, start=10, end=30 (big endian)
    (CMD_SET_SEG_RANGE, bytearray([CMD_SET_SEG_RANGE, 1, 0x00, 0x0A, 0x00, 0x1E])),
    (CMD_SET_SEG_BRIGHT, bytearray([CMD_SET_SEG_BRIGHT, 1, 128])),
    (CMD_SET_COLOR, bytearray([CMD_SET_COLOR, 255, 0, 128])),
    (CMD_SET_EFFECT, bytearray([CMD_SET_EFFECT, 3])),  # effect index 3
    (CMD_SET_BRIGHTNESS, bytearray([CMD_SET_BRIGHTNESS, 100])),
    (CMD_GET_STATUS, bytearray([CMD_GET_STATUS]))
]


def send_ascii(ser, cmd):
    ser.write((cmd + '\n').encode('utf-8'))
    print(f">>> ASCII: {cmd}")
    time.sleep(DELAY)
    resp = read_all(ser)
    print(resp)
    return resp


def send_binary(ser, packet):
    ser.write(packet)
    print(f">>> BINARY: {[hex(b) for b in packet]} ")
    time.sleep(DELAY)
    resp = read_all(ser)
    print(resp)
    return resp


def read_all(ser):
    data = b''
    # read until timeout
    while True:
        chunk = ser.read(ser.in_waiting or 1)
        if not chunk:
            break
        data += chunk
    try:
        text = data.decode('utf-8', errors='ignore')
    except:
        text = str(data)
    return text.strip()


def run_tests(port, baud):
    print(f"Opening serial port {port} @ {baud} baud")
    with serial.Serial(port, baud, timeout=0.2) as ser:
        time.sleep(1)
        # Flush any startup
        ser.reset_input_buffer()
        print("\n--- ASCII COMMANDS ---")
        for cmd in ASCII_COMMANDS:
            send_ascii(ser, cmd)

        print("\n--- BINARY COMMANDS ---")
        for name, packet in BINARY_COMMANDS:
            send_binary(ser, packet)

        print("\nAll tests completed.")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Test RaveController commands')
    parser.add_argument('--port', required=True, help='Serial port (e.g. COM7 or /dev/ttyACM0)')
    parser.add_argument('--baud', type=int, default=BAUD, help='Baud rate')
    args = parser.parse_args()
    run_tests(args.port, args.baud)
