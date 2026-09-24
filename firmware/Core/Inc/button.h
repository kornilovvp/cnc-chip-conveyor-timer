/**
  ******************************************************************************
  * @file           : button.h
  * @brief          : Command button: debounce + click series detector
  ******************************************************************************
  * The command button (X1) is a single illuminated push-button. A command is
  * encoded as a series of clicks: the operator clicks 1..4 times, then stops.
  * After BTN_SERIES_GAP_MS of inactivity the series is closed and the click
  * count is reported once through Button_GetSeries().
  *
  * Electrical: BUTTON_IN = PB0, pulled up by R3 10k to 3V3, the button shorts
  * the line to GND through R9 1k -> the input is ACTIVE LOW.
  *
  * This module owns nothing but that input pin. Indication lives elsewhere:
  * indicator.c drives the button lamp, debug_led.c drives the on-board HL1.
  ******************************************************************************
  */

#ifndef __BUTTON_H
#define __BUTTON_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Timing --------------------------------------------------------------------*/

/* Button_Tick() must be called at this rate (SysTick, 1 ms). */
#define BTN_TICK_MS                 1u


/* Contact bounce filter. Release bounces longer than press on this type of
   switch, so the two thresholds are separate. Both stay well below the
   shortest human press (~30 ms), so fast clicking is not lost. */
#define BTN_DEBOUNCE_PRESS_MS      15u
#define BTN_DEBOUNCE_RELEASE_MS    20u


/* Idle time (button released) that closes the click series.

   Sized for a hesitant operator: inside a series a confident user pauses
   100..250 ms, but an elderly one, or anyone wearing gloves on the stiff
   19 mm vandal-proof button, goes up to ~400 ms. 700 ms keeps ~75 % margin
   over that, so a slow series is not split into two commands.

   Cost: the command fires 700 ms after the last release, and the worst case
   (4 slow clicks) takes ~3.0 s end to end. */
#define BTN_SERIES_GAP_MS         700u


/* A press longer than this is treated as a stuck contact or a latched-up
   input: the whole series is discarded and no command is reported. */
#define BTN_MAX_PRESS_MS         2000u


/* Longest series the protocol knows. Extra clicks are still counted, but the
   reported value saturates here: 6 clicks come out as 4. */
#define BTN_CLICKS_MAX              4u


/* Debug ---------------------------------------------------------------------*/

/* Series state machine. Exposed so g_btn.state is readable in the debugger. */
typedef enum
{
  BTN_ST_IDLE     = 0,   /* released, no series in progress                   */
  BTN_ST_COUNTING = 1,   /* series in progress (pressed or inside the gap)    */
  BTN_ST_DISCARD  = 2    /* series dropped, waiting for the button to go up   */
} btn_state_t;


/**
  * Live snapshot of the detector for the IAR Live Watch window.
  *
  * Every field is written from Button_Tick() in SysTick context, so this is a
  * set of independent fields, not an atomic snapshot.
  *
  * The measured timings carry the filter delay with them: last_press_ms reads
  * about 5 ms longer and last_gap_ms about 5 ms shorter than reality.
  */
typedef struct
{
  /* Counters. If these stop moving, something upstream is dead. */
  uint32_t ticks;          /* Button_Tick() calls; frozen => SysTick is dead  */
  uint32_t presses;        /* debounced press edges                           */
  uint32_t releases;       /* debounced release edges                         */
  uint32_t series;         /* series handed over to the application           */
  uint32_t capped;         /* series longer than BTN_CLICKS_MAX               */
  uint32_t discards;       /* series dropped because the button was held      */
  uint32_t glitches;       /* level changes rejected by the debounce filter   */

  /* Live values. */
  uint16_t press_ms;       /* current press duration                          */
  uint16_t gap_ms;         /* idle time since the last release                */

  /* Measured on this hardware: how bad is the bounce really? */
  uint16_t last_press_ms;  /* duration of the last completed press            */
  uint16_t last_gap_ms;    /* pause between the last two clicks               */
  uint16_t max_glitch_ms;  /* longest rejected glitch = worst bounce seen     */

  uint8_t  raw;            /* pin right now, 1 = pressed (pin low)            */
  uint8_t  stable;         /* debounced level, 1 = pressed                    */
  uint8_t  listen;         /* 1 = series are being collected                  */
  uint8_t  state;          /* btn_state_t                                     */
  uint8_t  clicks;         /* clicks counted in the series so far (uncapped)  */
  uint8_t  last_series;    /* last value handed to the application (capped)   */
} btn_debug_t;


extern volatile btn_debug_t g_btn;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Initialise the detector. Call once after MX_GPIO_Init().
  *         Starts deaf: nothing is reported until Button_Listen(1).
  */
void Button_Init(void);


/**
  * @brief  Sampling and state machine. Call every BTN_TICK_MS from SysTick.
  */
void Button_Tick(void);


/**
  * @brief  Start or stop collecting click series.
  *
  *         While deaf the pin is still sampled (the debug snapshot stays live)
  *         but no series is collected and any unread result is dropped. A press
  *         that began while deaf is ignored even if it ends after listening
  *         resumed, so nothing leaks across the boundary.
  *
  * @param  enable  1 listen, 0 deaf
  */
void Button_Listen(uint8_t enable);


/**
  * @brief  Fetch a completed click series. Consuming read.
  * @retval 0                   no series completed since the last call
  * @retval 1..BTN_CLICKS_MAX   number of clicks, saturated at the maximum
  */
uint8_t Button_GetSeries(void);


/**
  * @brief  Debounced button state.
  * @retval 1 pressed, 0 released
  */
uint8_t Button_IsPressed(void);


#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_H */
