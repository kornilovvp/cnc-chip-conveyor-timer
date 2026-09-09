/**
  ******************************************************************************
  * @file           : relay.c
  * @brief          : START / STOP relays with mutual exclusion
  ******************************************************************************
  */

#include "main.h"
#include "relay.h"


/* Private variables ---------------------------------------------------------*/

static uint8_t       s_on[2];    /* indexed by relay_t, 1 = on               */

volatile rel_debug_t g_rel;      /* live snapshot for the debugger           */


/* Private functions ---------------------------------------------------------*/

/**
  * @brief  "on" becomes a voltage here and nowhere else.
  */
static GPIO_PinState Relay_Level(uint8_t on, uint8_t active_low)
{
  uint8_t high = ((on != 0u) != (active_low != 0u)) ? 1u : 0u;

  return (high != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}


static void Relay_Write(relay_t relay, uint8_t on)
{
  if (relay == RELAY_START)
  {
    HAL_GPIO_WritePin(START_RELAY_GPIO_Port, START_RELAY_Pin,
                      Relay_Level(on, RELAY_START_ACTIVE_LOW));

    g_rel.start_on = on;
  }
  else
  {
    HAL_GPIO_WritePin(STOP_RELAY_GPIO_Port, STOP_RELAY_Pin,
                      Relay_Level(on, RELAY_STOP_ACTIVE_LOW));

    g_rel.stop_on = on;
  }

  s_on[relay] = on;
}


static relay_t Relay_Other(relay_t relay)
{
  return (relay == RELAY_START) ? RELAY_STOP : RELAY_START;
}


/* Exported functions --------------------------------------------------------*/

void Relay_Init(void)
{
  g_rel.start_pulses = 0u;
  g_rel.stop_pulses  = 0u;
  g_rel.forced_off   = 0u;

  Relay_Write(RELAY_START, 0u);
  Relay_Write(RELAY_STOP,  0u);
}


void Relay_On(relay_t relay)
{
  relay_t other = Relay_Other(relay);

  /* The rule of the house: never both at once. Drop the other one first. */
  if (s_on[other] != 0u)
  {
    g_rel.forced_off++;

    Relay_Write(other, 0u);
  }

  if (s_on[relay] == 0u)
  {
    if (relay == RELAY_START)
    {
      g_rel.start_pulses++;
    }
    else
    {
      g_rel.stop_pulses++;
    }
  }

  Relay_Write(relay, 1u);
}


void Relay_Off(relay_t relay)
{
  Relay_Write(relay, 0u);
}


void Relay_AllOff(void)
{
  Relay_Write(RELAY_START, 0u);
  Relay_Write(RELAY_STOP,  0u);
}


uint8_t Relay_IsOn(relay_t relay)
{
  return s_on[relay];
}
