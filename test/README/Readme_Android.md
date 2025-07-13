# Android App Development Summary - LED Cape Controller

## Device Information
- **Device Name**: "RP2040-CAPE-LED"
- **Platform**: Arduino RP2040 Connect
- **Communication**: Bluetooth Low Energy (BLE)

## BLE Service Configuration

### Service UUID
```
Service: "0000180A-0000-1000-8000-00805F9B34FB"
```

### Characteristic UUID
```
Command Characteristic: "00002A57-0000-1000-8000-00805F9B34FB"
Properties: BLERead | BLEWrite | BLENotify
Max Size: 256 bytes
```

## Binary Command Protocol

### Command IDs (Send to Device)
```kotlin
const val CMD_SET_COLOR = 0x01
const val CMD_SET_EFFECT = 0x02
const val CMD_SET_BRIGHTNESS = 0x03
const val CMD_SET_SEG_BRIGHT = 0x04
const val CMD_SELECT_SEGMENT = 0x05
const val CMD_CLEAR_SEGMENTS = 0x06
const val CMD_SET_SEG_RANGE = 0x07
const val CMD_GET_STATUS = 0x08
const val CMD_BATCH_CONFIG = 0x09
const val CMD_NUM_PIXELS = 0x0A
const val CMD_GET_EFFECT_INFO = 0x0B
const val CMD_SET_LED_COUNT = 0x0C
const val CMD_GET_LED_COUNT = 0x0D
```

### Response ID (From Device)
```kotlin
const val CMD_ACK = 0xA0
```

## Available Effects
```kotlin
val AVAILABLE_EFFECTS = listOf(
    "RainbowChase",
    "SolidColor", 
    "FlashOnTrigger",
    "RainbowCycle",
    "TheaterChase",
    "Fire",
    "Flare",
    "ColoredFire",
    "AccelMeter",
    "KineticRipple"
)
```

## Serial Commands (for Testing/Debug)

### Basic Commands
```
listeffects          - Get all available effects as JSON
getstatus           - Get complete system status as JSON
getconfig           - Get current configuration (same format as save file)
getledcount         - Get current LED count
saveconfig          - Save current configuration to flash
fixsegments         - Fix segments that extend beyond LED bounds
```

### Segment Management
```
listsegments                    - List all segments with details
select <index>                  - Set active segment
clearsegments                   - Clear all user segments, reset to default
addsegment <start> <end>        - Add new segment
```

### Effects & Colors
```
seteffect <segmentIndex> <effectName>  - Set effect on specific segment
setcolor <r> <g> <b>                   - Set color of active segment
geteffectinfo <segmentIndex>           - Get effect parameters for segment
```

### System
```
setledcount <number>            - Set LED count and restart system
```

### JSON Commands
```json
{"get_parameters": "EffectName"}
{"set_parameter": {"segment_id": 0, "effect": "Fire", "name": "paramName", "value": "newValue"}}
```

## Binary Command Formats

### CMD_GET_LED_COUNT (0x0D)
**Send:** `[0x0D]`
**Response:** `[0xA0, count_high_byte, count_low_byte]`

### CMD_SET_LED_COUNT (0x0C)
**Send:** `[0x0C, count_high_byte, count_low_byte]`
**Response:** Device restarts automatically

### CMD_GET_STATUS (0x08)
**Send:** `[0x08]`
**Response:** JSON string via serial/notification

### CMD_BATCH_CONFIG (0x09)
**Send:** Multi-part JSON configuration
**Format:** `[0x09, json_data...]` (may span multiple packets)

## JSON Response Formats

### Status Response (getstatus/CMD_GET_STATUS)
```json
{
  "segments": [
    {
      "id": 0,
      "name": "all",
      "startLed": 0,
      "endLed": 299,
      "brightness": 128,
      "effect": "SolidColor"
    }
  ]
}
```

### Configuration Response (getconfig)
```json
{
  "led_count": 300,
  "segments": [
    {
      "name": "all",
      "startLed": 0,
      "endLed": 299,
      "brightness": 128,
      "effect": "SolidColor"
    }
  ]
}
```

### Effect Parameters Response
```json
{
  "effect": "Fire",
  "params": [
    {
      "name": "speed",
      "type": "integer",
      "value": 50,
      "min": 1,
      "max": 100
    },
    {
      "name": "color",
      "type": "color", 
      "value": "FF4500",
      "min": 0,
      "max": 16777215
    }
  ]
}
```

### Effects List Response
```json
{
  "effects": [
    "RainbowChase",
    "SolidColor",
    "FlashOnTrigger",
    "RainbowCycle",
    "TheaterChase", 
    "Fire",
    "Flare",
    "ColoredFire",
    "AccelMeter",
    "KineticRipple"
  ]
}
```

## Parameter Types
```kotlin
enum class ParamType(val value: Int) {
    INTEGER(0),
    FLOAT(1), 
    COLOR(2),
    BOOLEAN(3)
}
```

## Key Android App Features to Implement

### 1. Device Connection
- Scan for "RP2040-CAPE-LED" BLE devices
- Connect to service `0000180A-0000-1000-8000-00805F9B34FB`
- Subscribe to characteristic notifications

### 2. LED Strip Configuration
- Get/Set LED count with restart handling
- Validate segment boundaries against LED count

### 3. Segment Management
- Create, delete, and modify segments
- Visual segment editor with LED position mapping
- Segment validation (prevent overlaps, out-of-bounds)

### 4. Effect Control
- Effect selection with real-time preview
- Dynamic parameter adjustment based on effect type
- Color picker for color parameters
- Sliders for numeric parameters

### 5. Configuration Management
- Save/load configurations to device flash
- Export/import configurations as JSON
- Batch configuration updates

### 6. Real-time Control
- Live status updates via BLE notifications
- Real-time effect parameter adjustment
- Audio trigger monitoring (AccelMeter effect)

### 7. Error Handling
- Connection loss recovery
- Invalid command responses
- Segment boundary validation
- JSON parsing error handling

## Implementation Notes

1. **BLE Communication**: Use writeCharacteristic() for commands, enable notifications for responses
2. **JSON Parsing**: Most responses are JSON strings sent via notifications
3. **Multi-packet Support**: CMD_BATCH_CONFIG may require multiple BLE packets
4. **Device Restart**: CMD_SET_LED_COUNT triggers automatic restart
5. **Validation**: Always validate segment boundaries against current LED count
6. **Real-time Updates**: Subscribe to notifications for live status updates

## Example Android BLE Implementation

### Basic Connection Setup
```kotlin
class LEDCapeController {
    companion object {
        const val SERVICE_UUID = "0000180A-0000-1000-8000-00805F9B34FB"
        const val COMMAND_CHAR_UUID = "00002A57-0000-1000-8000-00805F9B34FB"
        const val DEVICE_NAME = "RP2040-CAPE-LED"
    }
    
    private var bluetoothGatt: BluetoothGatt? = null
    private var commandCharacteristic: BluetoothGattCharacteristic? = null
    
    fun sendCommand(commandId: Byte, data: ByteArray = byteArrayOf()) {
        val fullCommand = byteArrayOf(commandId) + data
        commandCharacteristic?.writeValue(fullCommand)
        bluetoothGatt?.writeCharacteristic(commandCharacteristic)
    }
    
    fun getLEDCount() {
        sendCommand(CMD_GET_LED_COUNT)
    }
    
    fun setLEDCount(count: Int) {
        val countBytes = byteArrayOf(
            (count shr 8).toByte(),
            (count and 0xFF).toByte()
        )
        sendCommand(CMD_SET_LED_COUNT, countBytes)
    }
    
    fun getStatus() {
        sendCommand(CMD_GET_STATUS)
    }
}
```

### Handling Notifications
```kotlin
private val gattCallback = object : BluetoothGattCallback() {
    override fun onCharacteristicChanged(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic,
        value: ByteArray
    ) {
        when {
            value[0] == CMD_ACK && value.size == 3 -> {
                // LED count response
                val ledCount = (value[1].toInt() shl 8) or value[2].toInt()
                handleLEDCountResponse(ledCount)
            }
            else -> {
                // JSON response
                val jsonString = String(value)
                handleJSONResponse(jsonString)
            }
        }
    }
}
```

This summary provides everything needed to implement a full-featured Android app for controlling your LED cape system!
