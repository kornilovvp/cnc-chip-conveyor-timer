/**
  ******************************************************************************
  * @file           : debug_led.h
  * @brief          : On-board debug LED HL1: state patterns
  ******************************************************************************
  * HL1 is the green 0805 LED on the board itself (DEBUG_LED = PB7, driven high
  * through R1 1k). It is not the button lamp -- that one belongs to indicator.c.
  *
  * This module knows two things only: "lit" and "blink on / off for ever".
  * What each pattern means is decided in app.c:
  *
  *   lit steady            start-up in progress (or the app never reached OFF)
  *   150 ms every 3 s      OFF
  *   300 on / 100 off      START relay pressed
  *   100 on / 300 off      STOP relay pressed
  *   500 on / 150 off      between the pulses: machine running or resting
  *   frozen in any state   main loop dead
  ******************************************************************************
  */

#ifndef __DEBUG_LED_H
#define __DEBUG_LED_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Debug ---------------------------------------------------------------------*/

/**
  * Live snapshot of HL1 for the IAR Live Watch window.
  */
typedef struct
{
  uint16_t on_ms;      /* pattern being shown; 0 / 0 = lit steady            */
  uint16_t off_ms;
  uint8_t  on;         /* 1 = LED is lit right now                           */
} dbg_debug_t;


extern volatile dbg_debug_t g_dbg;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Light the LED and hold it. Call once at the end of start-up: from
  *         here on a lit LED means initialisation completed normally.
  */
void DebugLed_Init(void);


/**
  * @brief  Blink on_ms / off_ms until told otherwise. Calling again with the
  *         same pattern changes nothing, so it is safe to call every loop.
  * @param  on_ms   lit phase, > 0
  * @param  off_ms  dark phase, > 0
  */
void DebugLed_Blink(uint16_t on_ms, uint16_t off_ms);


/**
  * @brief  Pattern state machine. Call from the main loop; it never blocks.
  */
void DebugLed_Task(void);


#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_LED_H */
