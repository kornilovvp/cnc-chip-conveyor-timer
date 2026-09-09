/**
  ******************************************************************************
  * @file           : buzzer.c
  * @brief          : Buzzer SP1: timed beeps and Morse SOS
  ******************************************************************************
  */

#include "main.h"
#include "buzzer.h"


/* Sequences -----------------------------------------------------------------*/

/* Alternating on / off durations, starting with "on". An odd count ends on a
   sound; Buzzer_Task() switches off after the last step. */
static const uint16_t k_sos[] =
{
  BUZ_DOT_MS,  BUZ_GAP_MS,  BUZ_DOT_MS,  BUZ_GAP_MS,  BUZ_DOT_MS,  BUZ_LETTER_GAP_MS,   /* S */
  BUZ_DASH_MS, BUZ_GAP_MS,  BUZ_DASH_MS, BUZ_GAP_MS,  BUZ_DASH_MS, BUZ_LETTER_GAP_MS,   /* O */
  BUZ_DOT_MS,  BUZ_GAP_MS,  BUZ_DOT_MS,  BUZ_GAP_MS,  BUZ_DOT_MS                        /* S */
};

#define BUZ_NO_STEP   0xFFu


/* Private variables ---------------------------------------------------------*/

static uint16_t       s_length_ms;   /* current step or beep length, 0 = off  */
static uint32_t       s_start_t0;    /* when it started                       */

static const uint16_t *s_seq;        /* sequence being played, or NULL        */
static uint8_t         s_seq_len;
static uint8_t         s_seq_step;   /* index of the step being played        */

volatile buz_debug_t   g_buz;        /* live snapshot for the debugger        */


/* Private functions ---------------------------------------------------------*/

static void Buzzer_Write(uint8_t on)
{
  HAL_GPIO_WritePin(BUZZER_GPIO_Port,
                    BUZZER_Pin,
                    (on != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  g_buz.on = on;
}


/**
  * @brief  Sound (or stay silent) for a while, from now.
  */
static void Buzzer_Run(uint16_t ms, uint8_t on)
{
  s_length_ms = ms;
  s_start_t0  = HAL_GetTick();

  g_buz.length_ms = ms;

  Buzzer_Write(on);
}


static void Buzzer_Silence(void)
{
  s_length_ms = 0u;
  s_seq       = 0;
  s_seq_len   = 0u;
  s_seq_step  = BUZ_NO_STEP;

  g_buz.length_ms = 0u;
  g_buz.step      = BUZ_NO_STEP;

  Buzzer_Write(0u);
}


/**
  * @brief  Start step `index` of the running sequence. Even steps sound,
  *         odd ones are gaps.
  */
static void Buzzer_Step(uint8_t index)
{
  s_seq_step = index;
  g_buz.step = index;

  Buzzer_Run(s_seq[index], ((index & 1u) == 0u) ? 1u : 0u);
}


/* Exported functions --------------------------------------------------------*/

void Buzzer_Init(void)
{
  g_buz.beeps     = 0u;
  g_buz.sequences = 0u;

  Buzzer_Silence();
}


void Buzzer_Beep(uint16_t ms)
{
  if (ms == 0u)
  {
    return;
  }

  if (ms > BUZ_MAX_BEEP_MS)
  {
    ms = BUZ_MAX_BEEP_MS;
  }

  Buzzer_Silence();

  g_buz.beeps++;

  Buzzer_Run(ms, 1u);
}


void Buzzer_Sos(void)
{
  Buzzer_Silence();

  s_seq     = k_sos;
  s_seq_len = (uint8_t)(sizeof(k_sos) / sizeof(k_sos[0]));

  g_buz.sequences++;

  Buzzer_Step(0u);
}


uint8_t Buzzer_IsBusy(void)
{
  return (s_length_ms != 0u) ? 1u : 0u;
}


void Buzzer_Task(void)
{
  if (s_length_ms == 0u)
  {
    return;
  }

  if ((HAL_GetTick() - s_start_t0) < (uint32_t)s_length_ms)
  {
    return;
  }

  /* Inside a sequence: on to the next step, if there is one. */
  if ((s_seq != 0) && ((uint8_t)(s_seq_step + 1u) < s_seq_len))
  {
    Buzzer_Step((uint8_t)(s_seq_step + 1u));

    return;
  }

  Buzzer_Silence();
}
