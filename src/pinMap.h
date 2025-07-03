
/**
 * @file pinmap.h
 * @brief Pin mapping definitions for the Nano RP2040 Connect board.
 *
 * This header file defines the mapping between Arduino digital pins
 * (e.g., D0, D1, ...) and the RP2040 GPIO pin numbers for the Nano RP2040 Connect.
 * It also defines the onboard LED pin constant.
 * 
 * These definitions allow for easier and clearer pin usage in source code.
 * 
 * @author Your Name
 * @version 0.1
 * @date 2025-06-05
 * 
 * @copy
*/

#ifndef PINMAP_H
#define PINMAP_H

/*****************************************************************
 *  PIN MAP FOR NANO RP2040 CONNECT  –  ARDUINO-D → GPIO
 *****************************************************************/
#define D0_GPIO   1
#define D1_GPIO   0
#define D2_GPIO   2
#define D3_GPIO   3
#define D4_GPIO   6
#define D5_GPIO   7
#define D6_GPIO   8
#define D7_GPIO   9
#define D8_GPIO   10
#define D9_GPIO   11
#define D10_GPIO  12
#define D11_GPIO  13
#define D12_GPIO  4
#define D13_GPIO  5
#define LED_PIN LED_BUILTIN

#endif // PINMAP_H
