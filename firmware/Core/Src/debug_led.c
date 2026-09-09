/**
  ******************************************************************************
  * @file           : debug_led.c
  * @brief          : On-board debug LED HL1: state patterns
  ******************************************************************************
  */

#include "main.h"
#include "debug_led.h"


/* Private variables ---------------------------------------------------------*/

static uint8_t       s_steady;     /* 1 = lit and holding, no pattern         */
static uint16_t      s_on_ms;      /* current pattern                         */
static uint16_t      s_off_ms;
static uint8_t       s_on;         /* 1 = inside the lit phase                */
static uint32_t      s_phase_t0;   /* start of the current phase              */

volatile dbg_debug_t g_dbg;        /* live snapshot for the debugger          */


/* Private functions ---------------------------------------------------------*/

static void DebugLed_Write(uint8_t on)
{
  HAL_GPIO_WritePin(DEBUG_LED_GPIO_Port,
                    DEBUG_LED_Pin,
                    (on != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  s_on     = on;
  g_dbg.on = on;
}


/* Exported functions --------------------------------------------------------*/

void DebugLed_Init(void)
{
  s_steady   = 1u;
  s_on_ms    = 0u;
  s_off_ms   = 0u;
  s_phase_t0 = HAL_GetTick();

  g_dbg.on_ms  = 0u;
  g_dbg.off_ms = 0u;

  DebugLed_Write(1u);
}


void DebugLed_Blink(uint16_t on_ms, uint16_t off_ms)
{
  /* Already showing exactly this: leave the phase alone. */
  if ((s_steady == 0u) && (on_ms == s_on_ms) && (off_ms == s_off_ms))
  {
    return;
  }

  s_steady   = 0u;
  s_on_ms    = on_ms;
  s_off_ms   = off_ms;
  s_phase_t0 = HAL_GetTick();

  g_dbg.on_ms  = on_ms;
  g_dbg.off_ms = off_ms;

  DebugLed_Write(1u);
}


void DebugLed_Task(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t phase_ms;

  if (s_steady != 0u)
  {
    return;
  }

  phase_ms = (s_on != 0u) ? s_on_ms : s_off_ms;

  if ((now - s_phase_t0) < phase_ms)
  {
    return;
  }

  /* Next phase starts from now, not from the planned end of the last one: a
     stalled main loop is followed by one clean phase, not a burst of
     catch-up flickers. This LED has no cadence worth keeping. */
  s_phase_t0 = now;

  DebugLed_Write((s_on != 0u) ? 0u : 1u);
}
