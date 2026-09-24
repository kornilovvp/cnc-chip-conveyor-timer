/**
  ******************************************************************************
  * @file           : indicator.h
  * @brief          : Command button lamp: blink bursts
  ******************************************************************************
  * Drives the lamp built into the command button:
  * BUTTON_LED_ON = PA6 -> R4 1k -> solid-state relay D2 -> 24 V via FU1 -> X1.1.
  *
  * The lamp speaks in bursts of blinks. A burst is either shown once
  * (power-on signature) or repeated with a pause of darkness in between (the
  * mode number while the device is running: three blinks, three seconds of
  * dark, three blinks... means MODE_3). A fast unbroken flicker is reserved
  * for one message only: the controller has a hardware problem but keeps
  * working.
  ******************************************************************************
  */

#ifndef __INDICATOR_H
#define __INDICATOR_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Timing --------------------------------------------------------------------*/

/* One blink inside a burst. Slow enough to count, short enough that the
   longest burst -- three blinks, MODE_3 -- is over in 1.2 s. */
#define IND_BLINK_ON_MS       200u
#define IND_BLINK_OFF_MS      200u

/* The fault flicker: 5 Hz, clearly faster than a burst. */
#define IND_FLICKER_ON_MS     100u
#define IND_FLICKER_OFF_MS    100u


/* Debug ---------------------------------------------------------------------*/

/**
  * Live snapshot of the lamp for the IAR Live Watch window.
  */
typedef struct
{
  uint32_t bursts;      /* bursts started, one-shot and repeated alike        */
  uint32_t gap_ms;      /* dark time between bursts when repeating            */
  uint16_t on_ms;       /* blink timing of the current pattern                */
  uint16_t off_ms;

  uint8_t  count;       /* blinks per burst                                   */
  uint8_t  left;        /* blinks still to be shown in this burst             */
  uint8_t  on;          /* 1 = lamp is on right now                           */
  uint8_t  repeat;      /* 1 = the burst repeats after gap_ms of dark         */
} ind_debug_t;


extern volatile ind_debug_t g_ind;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Lamp dark, nothing scheduled. Call once at start-up.
  */
void Indicator_Init(void);


/**
  * @brief  Stop whatever is playing and go dark.
  */
void Indicator_Off(void);


/**
  * @brief  Show one burst of `count` blinks, then go dark.
  *         Replaces anything that was playing. 0 is the same as Indicator_Off().
  */
void Indicator_Blink(uint8_t count);


/**
  * @brief  Show a burst of `count` blinks now, then again and again with
  *         `gap_ms` of darkness after each burst, until told otherwise.
  *         Replaces anything that was playing. 0 is the same as Indicator_Off().
  */
void Indicator_Repeat(uint8_t count, uint32_t gap_ms);


/**
  * @brief  Fast unbroken flicker until told otherwise: the fault signal.
  */
void Indicator_Flicker(void);


/**
  * @brief  Is a one-shot burst still playing? A repeating pattern is the
  *         lamp's normal state, not something to wait for, so it is not busy.
  * @retval 1 busy, 0 idle
  */
uint8_t Indicator_IsBusy(void);


/**
  * @brief  Pattern state machine. Call from the main loop; it never blocks.
  */
void Indicator_Task(void);


#ifdef __cplusplus
}
#endif

#endif /* __INDICATOR_H */
