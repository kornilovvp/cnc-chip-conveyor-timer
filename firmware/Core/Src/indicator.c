/**
  ******************************************************************************
  * @file           : indicator.c
  * @brief          : Command button lamp: blink bursts
  ******************************************************************************
  */

#include "main.h"
#include "indicator.h"


/* Private variables ---------------------------------------------------------*/

static uint8_t       s_count;      /* blinks per burst                        */
static uint8_t       s_left;       /* blinks still to be shown in this burst  */
static uint8_t       s_on;         /* 1 = inside the on phase of a blink      */
static uint8_t       s_repeat;     /* 1 = start a new burst after the gap     */
static uint16_t      s_on_ms;      /* blink timing of the current pattern     */
static uint16_t      s_off_ms;
static uint32_t      s_gap_ms;     /* darkness between bursts                 */
static uint32_t      s_phase_t0;   /* start of the current on/off phase       */
static uint32_t      s_burst_end;  /* when the last burst went dark           */

volatile ind_debug_t g_ind;        /* live snapshot for the debugger          */


/* Private functions ---------------------------------------------------------*/

static void Indicator_Write(uint8_t on)
{
  HAL_GPIO_WritePin(BUTTON_LED_ON_GPIO_Port,
                    BUTTON_LED_ON_Pin,
                    (on != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  g_ind.on = on;
}


static void Indicator_StartBurst(void)
{
  s_left     = s_count;
  s_on       = 1u;
  s_phase_t0 = HAL_GetTick();

  g_ind.bursts++;
  g_ind.left = s_left;

  Indicator_Write(1u);
}


static void Indicator_Program(uint8_t  count,
                              uint8_t  repeat,
                              uint32_t gap_ms,
                              uint16_t on_ms,
                              uint16_t off_ms)
{
  s_count  = count;
  s_repeat = repeat;
  s_gap_ms = gap_ms;
  s_on_ms  = on_ms;
  s_off_ms = off_ms;

  g_ind.count  = count;
  g_ind.repeat = repeat;
  g_ind.gap_ms = gap_ms;
  g_ind.on_ms  = on_ms;
  g_ind.off_ms = off_ms;
}


/* Exported functions --------------------------------------------------------*/

void Indicator_Init(void)
{
  s_left      = 0u;
  s_on        = 0u;
  s_phase_t0  = HAL_GetTick();
  s_burst_end = s_phase_t0;

  g_ind.bursts = 0u;
  g_ind.left   = 0u;

  Indicator_Program(0u, 0u, 0u, 0u, 0u);
  Indicator_Write(0u);
}


void Indicator_Off(void)
{
  s_left      = 0u;
  s_on        = 0u;
  s_burst_end = HAL_GetTick();

  g_ind.left = 0u;

  Indicator_Program(0u, 0u, 0u, 0u, 0u);
  Indicator_Write(0u);
}


void Indicator_Blink(uint8_t count)
{
  if (count == 0u)
  {
    Indicator_Off();

    return;
  }

  Indicator_Program(count, 0u, 0u, IND_BLINK_ON_MS, IND_BLINK_OFF_MS);
  Indicator_StartBurst();
}


void Indicator_Repeat(uint8_t count, uint32_t gap_ms)
{
  if (count == 0u)
  {
    Indicator_Off();

    return;
  }

  Indicator_Program(count, 1u, gap_ms, IND_BLINK_ON_MS, IND_BLINK_OFF_MS);
  Indicator_StartBurst();
}


void Indicator_Flicker(void)
{
  /* One blink, repeated with no gap at all: an unbroken 5 Hz flicker. */
  Indicator_Program(1u, 1u, 0u, IND_FLICKER_ON_MS, IND_FLICKER_OFF_MS);
  Indicator_StartBurst();
}


uint8_t Indicator_IsBusy(void)
{
  return ((s_left != 0u) && (s_repeat == 0u)) ? 1u : 0u;
}


void Indicator_Task(void)
{
  uint32_t now = HAL_GetTick();
  uint32_t phase_ms;

  if (s_left == 0u)
  {
    /* Dark between bursts. Has the gap passed? */
    if ((s_repeat != 0u) && ((now - s_burst_end) >= s_gap_ms))
    {
      Indicator_StartBurst();
    }

    return;
  }

  phase_ms = (s_on != 0u) ? s_on_ms : s_off_ms;

  if ((now - s_phase_t0) < phase_ms)
  {
    return;
  }

  s_phase_t0 += phase_ms;

  if (s_on != 0u)
  {
    /* End of the on phase: go dark and hold. */
    s_on = 0u;

    Indicator_Write(0u);
  }
  else
  {
    /* End of the off phase: one blink is complete. */
    s_left--;
    g_ind.left = s_left;

    if (s_left != 0u)
    {
      s_on = 1u;

      Indicator_Write(1u);
    }
    else
    {
      /* Burst over. The gap is counted from this exact moment. */
      s_burst_end = s_phase_t0;
    }
  }
}
