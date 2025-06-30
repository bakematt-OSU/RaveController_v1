# LED Segment Control – Test Command Reference

This document provides serial commands for testing LED segments, color settings, and dynamic effects using your PixelStrip-based controller.

---

## 🧪 Core Segment Commands

| Command             | Description                                  |
|---------------------|----------------------------------------------|
| `clearsegments`     | Reset to the default full-strip segment      |
| `addsegment 0 99`   | Create a segment from LED 0 to 99            |
| `addsegment 100 199`| Add another segment (custom range)           |
| `select 1`          | Select segment index `1` as the active one   |

---

## 🎨 Color Control

| Command              | Description                |
|----------------------|----------------------------|
| `setcolor 255 0 0`   | Set the effect color to red|
| `setcolor 0 255 0`   | Set to green               |
| `setcolor 0 0 255`   | Set to blue                |
| `setcolor 255 255 255` | Set to white             |

---

## ✨ Apply Effects

> Be sure to select a segment first (`select <index>`) before applying an effect.

| Command                   | Description                                |
|---------------------------|--------------------------------------------|
| `seteffect solid`         | Turn on solid color                        |
| `seteffect rainbow`       | Rainbow chase animation                    |
| `seteffect rainbow_cycle` | Full-strip rainbow cycle                   |
| `seteffect theater_chase` | Classic theater marquee animation          |
| `seteffect flash_trigger` | Flash when mic detects loud sound          |
| `seteffect fire`          | Classic fire-style glow                    |
| `seteffect flare`         | Bright intense fire burst                  |
| `seteffect colored_fire`  | Fire with color transitions                |
| `seteffect accel_meter`   | Bar graph tied to board orientation        |
| `seteffect kinetic_ripple`| Ripple triggered by motion/step            |

---

## 🧠 Debugging and Audio Testing

| Command               | Description                                  |
|------------------------|----------------------------------------------|
| `DEBUG HELP`          | Show debug command help                      |
| `DEBUG all 2`         | Enable all debug sections (level 2)          |
| `DBGLEVEL 3`          | Set global debug verbosity level to 3        |
| `DEBUG Microphone 2`  | Enable microphone debug at level 2           |

---

## 🔬 Test Scenarios

### Sound Trigger Test
```text
1. seteffect flash_trigger
2. Clap or make a loud sound
3. LEDs should flash briefly
```

### Accelerometer Test
```text
1. seteffect accel_meter
2. Tilt or shake the RP2040
3. LEDs react to motion
```

### Step Ripple Test
```text
1. seteffect kinetic_ripple
2. Tap or bump the board
3. A ripple expands from center LEDs
```

---

## 🔧 Notes

- Always `select` a segment before applying effects.
- Use `setcolor` to define the base color for effects that require it.
- Adjust `DEBUG` verbosity to help with tuning thresholds or viewing real-time data.
