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
  * There are two ways in. Cycle_Start() enters at SETTLE, so the first press
  * is START and the machine runs at once: that is switching on.
  * Cycle_StartWithStop() enters one step earlier:
  *
  *   SETTLE_STOP  both relays off, dead time            CYC_SETTLE_MS
  *   -> STOP_PULSE -> PAUSE -> START_PULSE ...
  *
  * so the first press is STOP, the machine rests the whole pause, and only
  * then the loop goes on with START: that is how a mode is taken up, from
  * OFF as well as on the run. A dead time opens both entries. It gives the
  * relay that may still be closed from before time to open, so the machine
  * never sees both buttons at once even for a few milliseconds.
  *
  * Stopping is a phase too, not a switch. The machine holds itself running
  * once START has been pressed, so merely letting go of the relays would
  * leave a conveyor switched off mid-run going. Cycle_Stop() therefore ends
  * with one more STOP press, from whatever phase the cycle was in:
  *
  *   END_SETTLE   both relays off, dead time            CYC_SETTLE_MS
  *   END_STOP     STOP_RELAY on                         CYC_RELAY_PULSE_MS
  *   -> IDLE
  *
  * A pause is no exception: STOP on a machine that already stands still does
  * nothing, and one path is simpler than two. The whole ending takes 600 ms,
  * less than the 700 ms the button needs to close a click series, so the
  * next command can never cut the last STOP short.
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
  CYC_PAUSE       = 5,
  CYC_END_SETTLE  = 6,   /* stopping: dead time before the last STOP press   */
  CYC_END_STOP    = 7,   /* stopping: the last STOP press                    */
  CYC_SETTLE_STOP = 8    /* starting with STOP: dead time before the press   */
} cyc_state_t;


/**
  * Live snapshot of the cycle for the IAR Live Watch window.
  */
typedef struct
{
  uint32_t starts;       /* Cycle_Start() calls                              */
  uint32_t stops;        /* Cycle_Stop() calls: last STOP presses played     */
  uint32_t rounds;       /* pauses completed; START follows each one         */

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
  * @brief  Start, or restart, the cycle with new timings, START first: after
  *         the dead time the machine runs at once. Safe in the middle of a
  *         pulse, both relays are let go first.
  * @param  run_ms    time the machine runs after the START pulse
  * @param  pause_ms  time the machine rests after the STOP pulse
  */
void Cycle_Start(uint32_t run_ms, uint32_t pause_ms);


/**
  * @brief  Start, or restart, the cycle with new timings, STOP first: after
  *         the dead time the machine is stopped, rests pause_ms, and only
  *         then gets its first START. Safe in the middle of a pulse.
  * @param  run_ms    time the machine runs after the START pulse
  * @param  pause_ms  time the machine rests after the STOP pulse
  */
void Cycle_StartWithStop(uint32_t run_ms, uint32_t pause_ms);


/**
  * @brief  Stop: after CYC_SETTLE_MS of dead time the machine gets a last
  *         STOP press, then both relays rest and the cycle is IDLE. That
  *         takes CYC_SETTLE_MS + CYC_RELAY_PULSE_MS, during which Cycle_Task()
  *         must keep being called. Nothing happens when the cycle is idle or
  *         already stopping.
  */
void Cycle_Stop(void);


/**
  * @brief  Is the cycle playing? The ending counts: the relays are not at
  *         rest until the last STOP has been released.
  * @retval 1 playing, 0 idle
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
