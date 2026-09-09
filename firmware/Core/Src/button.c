/**
  ******************************************************************************
  * @file           : button.c
  * @brief          : Command button: debounce + click series detector
  ******************************************************************************
  */

#include "main.h"
#include "button.h"


/* Private variables ---------------------------------------------------------*/

/* Owned by Button_Tick() (SysTick context). */
static btn_state_t      s_state;      /* series state machine                 */
static uint16_t         s_deb_ms;     /* how long the raw level disagrees     */
static uint16_t         s_press_ms;   /* current press duration               */
static uint16_t         s_gap_ms;     /* idle time since the last release     */
static uint8_t          s_clicks;     /* clicks counted in this series        */

/* Cross the ISR / main boundary, hence volatile. */
static volatile uint8_t s_ready;      /* 1 = Button_Init() has run            */
static volatile uint8_t s_listen;     /* 1 = collect series, 0 = deaf         */
static volatile uint8_t s_stable;     /* debounced level, 1 = pressed         */
static volatile uint8_t s_result;     /* completed series, 0 = nothing        */

volatile btn_debug_t    g_btn;        /* live snapshot for the debugger       */


/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Raw pin level, converted to logic level.
  * @retval 1 pressed (pin pulled low), 0 released (pin high)
  */
static uint8_t Button_ReadRaw(void)
{
  GPIO_PinState pin = HAL_GPIO_ReadPin(BUTTON_IN_GPIO_Port, BUTTON_IN_Pin);

  return (pin == GPIO_PIN_RESET) ? 1u : 0u;
}


/**
  * @brief  Put the series machine into its rest state for the current level.
  *         A button that is held right now must not turn into a click later,
  *         so it parks in DISCARD until it is released.
  */
static void Button_Rest(void)
{
  s_state    = (s_stable != 0u) ? BTN_ST_DISCARD : BTN_ST_IDLE;
  s_clicks   = 0u;
  s_press_ms = 0u;
  s_gap_ms   = 0u;
  s_result   = 0u;
}


/**
  * @brief  Mirror the internal state into the debug snapshot.
  */
static void Button_Publish(uint8_t raw)
{
  g_btn.raw      = raw;
  g_btn.stable   = s_stable;
  g_btn.listen   = s_listen;
  g_btn.state    = (uint8_t)s_state;
  g_btn.clicks   = s_clicks;
  g_btn.press_ms = s_press_ms;
  g_btn.gap_ms   = s_gap_ms;
}


/**
  * @brief  Debounce one sample.
  *
  *         The new level has to hold for the whole threshold, tick after tick.
  *         A run that ends early was bounce or noise; its length is the real
  *         measured glitch and goes into the debug snapshot.
  *
  * @param  raw  current pin level, 1 = pressed
  * @retval +1 press edge, -1 release edge, 0 no change
  */
static int8_t Button_Debounce(uint8_t raw)
{
  uint16_t threshold = (s_stable != 0u) ? BTN_DEBOUNCE_RELEASE_MS
                                        : BTN_DEBOUNCE_PRESS_MS;

  if (raw == s_stable)
  {
    if (s_deb_ms != 0u)
    {
      g_btn.glitches++;

      if (s_deb_ms > g_btn.max_glitch_ms)
      {
        g_btn.max_glitch_ms = s_deb_ms;
      }
    }

    s_deb_ms = 0u;

    return 0;
  }

  s_deb_ms += BTN_TICK_MS;

  if (s_deb_ms < threshold)
  {
    return 0;
  }

  s_deb_ms = 0u;
  s_stable = raw;

  if (s_stable != 0u)
  {
    g_btn.presses++;

    return 1;
  }

  g_btn.releases++;

  return -1;
}


/**
  * @brief  One step of the series state machine.
  * @param  edge  result of Button_Debounce() for this tick
  */
static void Button_Series(int8_t edge)
{
  switch (s_state)
  {
    case BTN_ST_IDLE:

      if (edge > 0)
      {
        s_clicks   = 0u;
        s_press_ms = 0u;
        s_state    = BTN_ST_COUNTING;
      }
      break;


    case BTN_ST_COUNTING:

      if (edge < 0)
      {
        /* One complete press-release = one click. Keep counting past the
           protocol maximum; the value is capped when it is reported. */
        g_btn.last_press_ms = s_press_ms;

        if (s_clicks < UINT8_MAX)
        {
          s_clicks++;
        }

        s_gap_ms = 0u;
      }
      else if (edge > 0)
      {
        g_btn.last_gap_ms = s_gap_ms;

        s_press_ms = 0u;
      }
      else if (s_stable != 0u)
      {
        /* Still held: guard against a stuck contact or a latched-up input. */
        s_press_ms += BTN_TICK_MS;

        if (s_press_ms >= BTN_MAX_PRESS_MS)
        {
          g_btn.discards++;

          s_clicks = 0u;
          s_state  = BTN_ST_DISCARD;
        }
      }
      else
      {
        /* Released, waiting for the next click of the series. */
        s_gap_ms += BTN_TICK_MS;

        if (s_gap_ms >= BTN_SERIES_GAP_MS)
        {
          uint8_t value = s_clicks;

          if (value > BTN_CLICKS_MAX)
          {
            value = BTN_CLICKS_MAX;

            g_btn.capped++;
          }

          s_result = value;                  /* publish the result once */

          g_btn.series++;
          g_btn.last_series = value;

          s_clicks = 0u;
          s_state  = BTN_ST_IDLE;
        }
      }
      break;


    case BTN_ST_DISCARD:
    default:

      if (edge < 0)
      {
        s_clicks = 0u;
        s_state  = BTN_ST_IDLE;
      }
      break;
  }
}


/* Exported functions --------------------------------------------------------*/

void Button_Init(void)
{
  uint8_t raw;

  s_ready  = 0u;
  s_listen = 0u;

  raw = Button_ReadRaw();

  s_stable = raw;
  s_deb_ms = 0u;

  Button_Rest();

  g_btn.ticks         = 0u;
  g_btn.presses       = 0u;
  g_btn.releases      = 0u;
  g_btn.series        = 0u;
  g_btn.capped        = 0u;
  g_btn.discards      = 0u;
  g_btn.glitches      = 0u;

  g_btn.last_press_ms = 0u;
  g_btn.last_gap_ms   = 0u;
  g_btn.max_glitch_ms = 0u;
  g_btn.last_series   = 0u;

  Button_Publish(raw);

  s_ready = 1u;
}


void Button_Tick(void)
{
  uint8_t raw;
  int8_t  edge;

  /* SysTick starts inside HAL_Init(), long before MX_GPIO_Init() has switched
     the GPIO clocks on. Reading a clock-gated port is not something to rely
     on, so stay out of the way until Button_Init() has run. */
  if (s_ready == 0u)
  {
    return;
  }

  g_btn.ticks++;

  raw  = Button_ReadRaw();
  edge = Button_Debounce(raw);

  if (s_listen != 0u)
  {
    Button_Series(edge);
  }
  else
  {
    /* Deaf: keep the machine parked for whatever the level is right now, so
       listening always resumes from a clean state. */
    Button_Rest();
  }

  Button_Publish(raw);
}


void Button_Listen(uint8_t enable)
{
  /* Everything else happens inside Button_Tick(), in one context. */
  s_listen = (enable != 0u) ? 1u : 0u;
}


uint8_t Button_GetSeries(void)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t  result;

  /* Button_Tick() runs in SysTick context, so read-and-clear must be atomic. */
  __disable_irq();

  result   = s_result;
  s_result = 0u;

  __set_PRIMASK(primask);

  return result;
}


uint8_t Button_IsPressed(void)
{
  return s_stable;
}
