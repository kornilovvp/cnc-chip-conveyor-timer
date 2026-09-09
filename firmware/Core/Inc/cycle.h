/**
  ******************************************************************************
  * @file           : cycle.h
  * @brief          : START / STOP relay cycle
  ******************************************************************************
  * Plays the machine's START and STOP buttons in a loop:
  *
  *   SETTLE       both relays off, dead time            CYC_SETTLE_MS
  *   START_PULSE  START_RELAY on                        CYC_RELAY_PULSE_MS
  *   RUN          machine running                       run_ms   (per mode)
  *   STOP_PULSE   STOP_RELAY on                         CYC_RELAY_PULSE_MS
  *   PAUSE        machine stopped                       pause_ms (per mode)
  *   -> START_PULSE ...
  *
  * SETTLE is only passed on (re)start. It gives the relay that may still be
  * closed from the previous cycle time to open before START is pressed, so
  * the machine never sees both buttons at once even for a few milliseconds.
  ******************************************************************************
  */

#ifndef __CYCLE_H
#define __CYCLE_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Timing --------------------------------------------------------------------*/

#define CYC_SETTLE_MS          100u   /* both off before the first START      */
#define CYC_RELAY_PULSE_MS     500u   /* how long a "button" is held          */


/* Debug ---------------------------------------------------------------------*/

typedef enum
{
  CYC_IDLE        = 0,
  CYC_SETTLE      = 1,
  CYC_START_PULSE = 2,
  CYC_RUN         = 3,
  CYC_STOP_PULSE  = 4,
  CYC_PAUSE       = 5
} cyc_state_t;


/**
  * Live snapshot of the cycle for the IAR Live Watch window.
  */
typedef struct
{
  uint32_t starts;       /* Cycle_Start() calls                              */
  uint32_t rounds;       /* completed START..PAUSE rounds                    */

  uint32_t run_ms;       /* parameters of the current cycle                  */
  uint32_t pause_ms;
  uint32_t phase_ms;     /* time spent in the current phase so far           */

  uint8_t  state;        /* cyc_state_t                                      */
} cyc_debug_t;


extern volatile cyc_debug_t g_cyc;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Idle, both relays off. Call once at start-up.
  */
void Cycle_Init(void);


/**
  * @brief  Start, or restart, the cycle with new timings. Always begins with
  *         SETTLE, so a restart in the middle of a pulse is safe.
  * @param  run_ms    time the machine runs after the START pulse
  * @param  pause_ms  time the machine rests after the STOP pulse
  */
void Cycle_Start(uint32_t run_ms, uint32_t pause_ms);


/**
  * @brief  Stop immediately: both relays off, back to IDLE.
  */
void Cycle_Stop(void);


/**
  * @brief  Is the cycle running?
  * @retval 1 running, 0 idle
  */
uint8_t Cycle_IsRunning(void);


/**
  * @brief  Which phase is the cycle in?
  */
cyc_state_t Cycle_GetState(void);


/**
  * @brief  Cycle state machine. Call from the main loop; it never blocks.
  */
void Cycle_Task(void);


#ifdef __cplusplus
}
#endif

#endif /* __CYCLE_H */
