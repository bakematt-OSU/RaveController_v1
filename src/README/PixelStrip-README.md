Of course. Here is the command reference for your project in Markdown format.

# Project Command Reference

This document lists all the serial commands available to control the LED effects on your cape.

## General Commands

These commands are used for general setup and control of the LED strip and segments.

| Command | Parameters | Description |
| :--- | :--- | :--- |
| `select` | **`<index>`** | Selects which segment of LEDs the following commands will apply to. The default segment is `0` (the entire strip). |
| `setcolor` | **`<r> <g> <b>`** | Sets the primary active color for many effects like `solid`, `kineticripple`, and `bassflash`. |
| `addsegment` | **`<start> <end>`** | Creates a new addressable segment from a starting pixel to an ending pixel. The new segment will be assigned the next available index. |
| `clearsegments`| *(none)* | Deletes all custom segments and resets the strip to a single segment (`0`) that covers all LEDs. |
| `next` | *(none)* | Cycles to the next available effect in the master list. |
| `stop` | *(none)* | An alias for the `rainbow` effect, which can be used as a default idle state. |

## Effect Commands

These commands start a specific visual effect on the currently selected segment.

### Kinetic Ripple

This effect creates an outward-expanding ripple of light triggered by movement.

  * **`kineticripple`**
      * Starts the Kinetic Ripple effect. The ripple's color is determined by the last `setcolor` command.
  * **`setthreshold <value>`**
      * Sets the motion sensitivity required to trigger a ripple. A lower value (e.g., `1.6`) is more sensitive. A higher value (e.g., `3.0`) requires a harder step or jump. The default is `2.5`.
  * **`setripplewidth <width>`**
      * Sets the visual width of the ripple in pixels. The value must be a positive, odd number (e.g., 1, 3, 5) for a symmetrical look. The default is `3`.
  * **`setripplespeed <speed>`**
      * Sets the travel speed of the ripple, which also controls the fade duration.
          * A *slower speed* (e.g., `0.1`) makes the fade last longer.
          * A *faster speed* (e.g., `0.4`) makes the fade shorter.
          * The default is `0.2`.

### Colored Fire

This effect simulates a flame using a three-color gradient.

  * **`coloredfire`**
      * Starts the Colored Fire effect.
  * **`setfirecolors <r1 g1 b1 r2 g2 b2 r3 g3 b3>`**
      * Sets the three colors for the fire gradient.
          * `<r1 g1 b1>` is the coolest part of the flame (the base).
          * `<r2 g2 b2>` is the middle color.
          * `<r3 g3 b3>` is the hottest part of the flame (the tips).

### Other Effects

These commands start simpler, pre-configured effects.

  * `solid` - A solid color based on `setcolor`.
  * `rainbow` - A classic flowing rainbow.
  * `fire` / `flare` - The original fire effects.
  * `bassflash` - A flash of color triggered by audio.
  * `accelmeter` - The "bubble level" effect.
  * `rainbowcycle` / `theaterchase` - Animated patterns.

## Debugging Commands

  * **`debugaccel`**
      * Toggles a data stream in the Serial Monitor that shows the live accelerometer magnitude reading. This is very useful for finding the right value for the `setthreshold` command. Type it once to turn it on, and again to turn it off.

## Example Workflows

### 1\. Configure a Custom Kinetic Ripple

This example creates a large, slow-fading purple ripple with high sensitivity.

```
// Set the color to a deep purple
setcolor 128 0 128

// Make the ripple wider (5 pixels)
setripplewidth 5

// Make the ripple slower, so the fade lasts longer
setripplespeed 0.15

// Make the effect very sensitive to movement
setthreshold 1.7

// Start the effect
kineticripple
```

### 2\. Create a Fire Effect on the Top of the Cape

This example creates a new segment for just the first 100 pixels and puts a custom-colored fire effect on it.

```
// Create a new segment for pixels 0-99
addsegment 0 99

// Select the new segment (which will be index 1)
select 1

// Set the colors for a spooky green fire
setfirecolors 0 30 10 50 255 50 150 255 150

// Start the effect on the selected segment
coloredfire
```