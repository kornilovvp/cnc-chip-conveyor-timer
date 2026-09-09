/**
  ******************************************************************************
  * @file           : buzzer.h
  * @brief          : Buzzer SP1: timed beeps and Morse SOS
  ******************************************************************************
  * SP1 is an MLT-9650, an ACTIVE buzzer with its own 2.7 kHz oscillator, so it
  * only knows on and off -- there is no tone to generate. It hangs on 3V3
  * through R7 10R, and its low side is switched by the solid-state relay D6:
  * BUZZER = PB3 -> R8 1k -> D6 -> SP1.2.
  *
  * D6 needs ~1.5 ms to turn on, so beeps shorter than about 10 ms are pointless.
  ******************************************************************************
  */

#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Limits --------------------------------------------------------------------*/

/* The longest beep in use is the 15 s auto-off alarm. The clamp keeps a bad
   argument from leaving the buzzer screaming until the next reset. */
#define BUZ_MAX_BEEP_MS  20000u


/* Morse timing for Buzzer_Sos(): dot, dash = 3 dots, gap inside a letter =
   1 dot, gap between letters = 3 dots. The whole call lasts ~3.2 s. */
#define BUZ_DOT_MS         120u
#define BUZ_DASH_MS        360u
#define BUZ_GAP_MS         120u
#define BUZ_LETTER_GAP_MS  360u


/* Debug ---------------------------------------------------------------------*/

/**
  * Live snapshot of the buzzer for the IAR Live Watch window.
  */
typedef struct
{
  uint32_t beeps;       /* single beeps started since reset                  */
  uint32_t sequences;   /* SOS calls started since reset                     */
  uint16_t length_ms;   /* length of the beep, or of the current SOS step    */
  uint8_t  step;        /* SOS step being played; 0xFF = not in a sequence   */
  uint8_t  on;          /* 1 = sounding right now                            */
} buz_debug_t;


extern volatile buz_debug_t g_buz;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Silence the buzzer. Call once at start-up.
  */
void Buzzer_Init(void);


/**
  * @brief  Start a single beep. Returns immediately; Buzzer_Task() ends it.
  *         Replaces whatever was playing, an SOS included.
  * @param  ms  length in milliseconds, clamped to BUZ_MAX_BEEP_MS; 0 is ignored
  */
void Buzzer_Beep(uint16_t ms);


/**
  * @brief  Play "... --- ..." once. Replaces whatever was playing.
  */
void Buzzer_Sos(void);


/**
  * @brief  Is anything sounding or scheduled?
  * @retval 1 busy, 0 silent
  */
uint8_t Buzzer_IsBusy(void);


/**
  * @brief  Buzzer state machine. Call from the main loop; it never blocks.
  */
void Buzzer_Task(void);


#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
