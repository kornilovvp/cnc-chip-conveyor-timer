/**
  ******************************************************************************
  * @file           : app.c
  * @brief          : Application: power-on signature, OFF / RUN, modes
  ******************************************************************************
  */

#include "main.h"
#include "app.h"
#include "button.h"
#include "buzzer.h"
#include "cycle.h"
#include "debug_led.h"
#include "indicator.h"
#include "settings.h"


/* Mode table ----------------------------------------------------------------*/

typedef struct
{
  uint32_t run_ms;     /* machine runs this long after the START pulse       */
  uint32_t pause_ms;   /* machine rests this long after the STOP pulse       */
} mode_time_t;


static const mode_time_t k_mode[SET_MODE_MAX + 1u] =
{
  {      0u,       0u },   /* 0: not a mode                                   */
  {  90000u,  360000u },   /* MODE_1:  90 s /  6 min                          */
  {  90000u,  720000u },   /* MODE_2:  90 s / 12 min                          */
  {  90000u, 1080000u }    /* MODE_3:  90 s / 18 min                          */
};


/* Private variables ---------------------------------------------------------*/

static app_state_t   s_state;
static uint8_t       s_mode;       /* mode in force, 1..3                    */
static uint8_t       s_fault;      /* 1 = settings store faulty              */
static uint32_t      s_state_t0;   /* when the current state began           */
static uint32_t      s_run_t0;     /* when RUN began: the 4 h clock          */

volatile app_debug_t g_app;        /* live snapshot for the debugger         */


/* Private functions ---------------------------------------------------------*/

static void App_Enter(app_state_t state)
{
  s_state    = state;
  s_state_t0 = HAL_GetTick();

  g_app.state = (uint8_t)state;
}


/**
  * @brief  Lamp for the OFF state: dark, or the fault flicker.
  */
static void App_ShowOff(void)
{
  if (s_fault != 0u)
  {
    Indicator_Flicker();
  }
  else
  {
    Indicator_Off();
  }
}


/**
  * @brief  HL1 for the current state. Cheap and idempotent, so it simply runs
  *         every loop. During start-up the LED stays lit as DebugLed_Init()
  *         left it. From then on it follows the relays: a pattern per phase
  *         while the cycle plays, the last STOP press on the way to OFF
  *         included, and the OFF flash once the cycle is idle.
  */
static void App_ShowDebugLed(void)
{
  if ((s_state != APP_OFF) && (s_state != APP_RUN))
  {
    return;
  }

  switch (Cycle_GetState())
  {
    case CYC_IDLE:
      /* Only ever seen in OFF: a running cycle is never idle. */
      DebugLed_Blink(APP_HL1_OFF_ON_MS, APP_HL1_OFF_OFF_MS);
      break;

    case CYC_START_PULSE:
      DebugLed_Blink(APP_HL1_START_ON_MS, APP_HL1_START_OFF_MS);
      break;

    case CYC_STOP_PULSE:
    case CYC_END_STOP:
      DebugLed_Blink(APP_HL1_STOP_ON_MS, APP_HL1_STOP_OFF_MS);
      break;

    default:
      /* SETTLE, SETTLE_STOP, RUN, PAUSE, END_SETTLE: both relays released. */
      DebugLed_Blink(APP_HL1_BETWEEN_ON_MS, APP_HL1_BETWEEN_OFF_MS);
      break;
  }
}


/**
  * @brief  Has the settings store failed since we last looked? Raise the
  *         alarm once, when it happens.
  * @retval 1 the fault appeared just now, 0 nothing new
  */
static uint8_t App_PollFault(void)
{
  if ((s_fault != 0u) || (Settings_IsFaulty() == 0u))
  {
    return 0u;
  }

  s_fault     = 1u;
  g_app.fault = 1u;

  Buzzer_Sos();

  return 1u;
}


/**
  * @brief  Start the cycle in the mode in force. From OFF this switches the
  *         device on and starts the 4 h clock; from RUN it only restarts the
  *         cycle and the clock keeps running.
  * @param  with_stop  1 = STOP first, then the mode's pause, then the cycle
  *                    (a mode taken up); 0 = START at once (switching on)
  * @param  announce   1 = the usual 2 s start beep, 0 = keep the buzzer as is
  */
static void App_Start(uint8_t with_stop, uint8_t announce)
{
  if (s_state == APP_RUN)
  {
    g_app.restarts++;
  }
  else
  {
    s_run_t0 = HAL_GetTick();

    g_app.switch_on++;
    g_app.run_ms = 0u;

    App_Enter(APP_RUN);
  }

  if (with_stop != 0u)
  {
    Cycle_StartWithStop(k_mode[s_mode].run_ms, k_mode[s_mode].pause_ms);
  }
  else
  {
    Cycle_Start(k_mode[s_mode].run_ms, k_mode[s_mode].pause_ms);
  }

  Indicator_Repeat(s_mode, APP_MODE_BURST_GAP_MS);

  if (announce != 0u)
  {
    Buzzer_Beep(APP_START_BEEP_MS);
  }
}


/**
  * @brief  Switch off. The cycle presses STOP one last time before the
  *         relays rest (see cycle.h), so the machine really stops; the lamp
  *         takes its OFF face at once. Silent -- the caller decides whether
  *         this deserves a sound.
  */
static void App_Off(void)
{
  Cycle_Stop();
  App_ShowOff();

  App_Enter(APP_OFF);
}


/**
  * @brief  Dispatch a click series.
  *         1 click toggles OFF / RUN. 2..4 clicks select mode 1..3, store it
  *         and take it up -- from OFF as well as from RUN, the same mode
  *         included: STOP, the mode's pause, then the cycle.
  */
static void App_OnClicks(uint8_t clicks)
{
  uint8_t mode;
  uint8_t new_fault = 0u;

  g_app.last_clicks = clicks;

  if (clicks == 1u)
  {
    if (s_state == APP_RUN)
    {
      g_app.switch_off++;

      App_Off();
      Buzzer_Beep(APP_STOP_BEEP_MS);
    }
    else
    {
      App_Start(0u, 1u);
    }

    return;
  }

  mode = clicks - 1u;

  if (mode > SET_MODE_MAX)
  {
    mode = SET_MODE_MAX;
  }

  if (mode != s_mode)
  {
    s_mode = mode;

    g_app.mode = mode;
    g_app.mode_changes++;

    Settings_SetMode(mode);

    /* If flash just died, the SOS is the sound that matters: it must not be
       cut short by the start beep. */
    new_fault = App_PollFault();
  }

  App_Start(1u, (new_fault != 0u) ? 0u : 1u);
}


/* Exported functions --------------------------------------------------------*/

void App_Init(void)
{
  s_mode   = Settings_GetMode();
  s_fault  = Settings_IsFaulty();
  s_run_t0 = 0u;

  g_app.run_ms       = 0u;
  g_app.switch_on    = 0u;
  g_app.switch_off   = 0u;
  g_app.auto_off     = 0u;
  g_app.restarts     = 0u;
  g_app.mode_changes = 0u;
  g_app.mode         = s_mode;
  g_app.last_clicks  = 0u;
  g_app.fault        = s_fault;

  /* Deaf until the signature has played. */
  Button_Listen(0u);

  App_Enter(APP_STARTUP_PAUSE);
}


void App_Task(void)
{
  uint32_t now = HAL_GetTick();
  uint8_t  clicks;

  switch (s_state)
  {
    case APP_STARTUP_PAUSE:

      /* Stay quiet for a second after power-on... */
      if ((now - s_state_t0) >= APP_STARTUP_PAUSE_MS)
      {
        if (s_fault != 0u)
        {
          /* ...then, if the store is broken, say so instead of "ready". */
          Buzzer_Sos();
          Indicator_Flicker();
        }
        else
        {
          /* ...then announce readiness: a few lamp blinks, and either the
             usual beep or -- on a blank chip, the very first power-on -- an
             SOS, so that state is recognised by ear. */
          if (Settings_IsFirstBoot() != 0u)
          {
            Buzzer_Sos();
          }
          else
          {
            Buzzer_Beep(APP_STARTUP_BEEP_MS);
          }

          Indicator_Blink(APP_STARTUP_BLINKS);
        }

        App_Enter(APP_STARTUP_SIGNATURE);
      }
      break;


    case APP_STARTUP_SIGNATURE:

      if ((Buzzer_IsBusy() == 0u) && (Indicator_IsBusy() == 0u))
      {
        App_Enter(APP_OFF);

        /* From here on the operator is heard. */
        Button_Listen(1u);
      }
      break;


    case APP_OFF:
    case APP_RUN:

      clicks = Button_GetSeries();

      if (clicks != 0u)
      {
        App_OnClicks(clicks);
      }

      if (s_state == APP_RUN)
      {
        /* Read the clock afresh: App_Start() may have just set s_run_t0, and
           a flash erase inside a mode change holds the loop for tens of
           milliseconds -- the `now` from the top of this function would then
           be older than s_run_t0, the subtraction would wrap, and the 4 h
           alarm would fire on the spot. */
        g_app.run_ms = HAL_GetTick() - s_run_t0;

        if (g_app.run_ms >= APP_AUTO_OFF_MS)
        {
          g_app.auto_off++;

          App_Off();
          Buzzer_Beep(APP_AUTO_OFF_BEEP_MS);
        }
      }
      break;


    default:
      break;
  }

  App_ShowDebugLed();
}
