/**
  ******************************************************************************
  * @file           : cycle.c
  * @brief          : START / STOP relay cycle
  ******************************************************************************
  */

#include "main.h"
#include "cycle.h"
#include "relay.h"


/* Private variables ---------------------------------------------------------*/

static cyc_state_t   s_state;
static uint32_t      s_run_ms;
static uint32_t      s_pause_ms;
static uint32_t      s_phase_t0;   /* when the current phase began           */

volatile cyc_debug_t g_cyc;        /* live snapshot for the debugger         */


/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Enter a phase. The phase clock restarts from the current tick, not
  *         from the planned end of the previous one: dead time and relay
  *         pulses must last their full length in real time, even if the main
  *         loop was held up (a flash erase, for instance) just before.
  */
static void Cycle_Enter(cyc_state_t state)
{
  s_state    = state;
  s_phase_t0 = HAL_GetTick();

  g_cyc.state = (uint8_t)state;
}


/**
  * @brief  Take new timings and let both relays go: the common opening of
  *         either kind of (re)start, before its dead time.
  */
static void Cycle_Arm(uint32_t run_ms, uint32_t pause_ms)
{
  s_run_ms   = run_ms;
  s_pause_ms = pause_ms;

  g_cyc.starts++;
  g_cyc.run_ms   = run_ms;
  g_cyc.pause_ms = pause_ms;

  Relay_AllOff();
}


/* Exported functions --------------------------------------------------------*/

void Cycle_Init(void)
{
  s_run_ms   = 0u;
  s_pause_ms = 0u;

  g_cyc.starts   = 0u;
  g_cyc.stops    = 0u;
  g_cyc.rounds   = 0u;
  g_cyc.run_ms   = 0u;
  g_cyc.pause_ms = 0u;
  g_cyc.phase_ms = 0u;

  Relay_AllOff();
  Cycle_Enter(CYC_IDLE);
}


void Cycle_Start(uint32_t run_ms, uint32_t pause_ms)
{
  Cycle_Arm(run_ms, pause_ms);
  Cycle_Enter(CYC_SETTLE);
}


void Cycle_StartWithStop(uint32_t run_ms, uint32_t pause_ms)
{
  Cycle_Arm(run_ms, pause_ms);
  Cycle_Enter(CYC_SETTLE_STOP);
}


void Cycle_Stop(void)
{
  /* Idle: nothing to stop. Already stopping: let the last STOP play out
     rather than press it twice. */
  if ((s_state == CYC_IDLE) || (s_state == CYC_END_SETTLE) || (s_state == CYC_END_STOP))
  {
    return;
  }

  g_cyc.stops++;

  /* Whatever is pressed is let go first; the last STOP follows after the
     dead time, so the machine never sees START and STOP together. */
  Relay_AllOff();
  Cycle_Enter(CYC_END_SETTLE);
}


uint8_t Cycle_IsRunning(void)
{
  return (s_state != CYC_IDLE) ? 1u : 0u;
}


cyc_state_t Cycle_GetState(void)
{
  return s_state;
}


void Cycle_Task(void)
{
  uint32_t elapsed;

  if (s_state == CYC_IDLE)
  {
    return;
  }

  elapsed        = HAL_GetTick() - s_phase_t0;
  g_cyc.phase_ms = elapsed;

  switch (s_state)
  {
    case CYC_SETTLE:

      if (elapsed >= CYC_SETTLE_MS)
      {
        Relay_On(RELAY_START);
        Cycle_Enter(CYC_START_PULSE);
      }
      break;


    case CYC_SETTLE_STOP:

      /* Into the loop at its STOP: the pause follows, then the first START. */
      if (elapsed >= CYC_SETTLE_MS)
      {
        Relay_On(RELAY_STOP);
        Cycle_Enter(CYC_STOP_PULSE);
      }
      break;


    case CYC_START_PULSE:

      if (elapsed >= CYC_RELAY_PULSE_MS)
      {
        Relay_Off(RELAY_START);
        Cycle_Enter(CYC_RUN);
      }
      break;


    case CYC_RUN:

      if (elapsed >= s_run_ms)
      {
        Relay_On(RELAY_STOP);
        Cycle_Enter(CYC_STOP_PULSE);
      }
      break;


    case CYC_STOP_PULSE:

      if (elapsed >= CYC_RELAY_PULSE_MS)
      {
        Relay_Off(RELAY_STOP);
        Cycle_Enter(CYC_PAUSE);
      }
      break;


    case CYC_PAUSE:

      if (elapsed >= s_pause_ms)
      {
        g_cyc.rounds++;

        /* Both relays have been off for the whole pause: no SETTLE needed. */
        Relay_On(RELAY_START);
        Cycle_Enter(CYC_START_PULSE);
      }
      break;


    case CYC_END_SETTLE:

      if (elapsed >= CYC_SETTLE_MS)
      {
        Relay_On(RELAY_STOP);
        Cycle_Enter(CYC_END_STOP);
      }
      break;


    case CYC_END_STOP:

      if (elapsed >= CYC_RELAY_PULSE_MS)
      {
        Relay_Off(RELAY_STOP);
        Cycle_Enter(CYC_IDLE);
      }
      break;


    case CYC_IDLE:
    default:
      break;
  }
}
