/**
  ******************************************************************************
  * @file           : relay.h
  * @brief          : START / STOP relays with mutual exclusion
  ******************************************************************************
  * Two relays stand in for the START and STOP buttons of the machine:
  *   START_RELAY = PA5 -> R5 1k -> solid-state relay -> K1, normally open
  *   STOP_RELAY  = PA7 -> R6 1k -> solid-state relay -> K2, normally closed
  *
  * Contacts. The START relay's NO contact sits across the machine's START
  * button: energise the coil and START is pressed. The STOP relay's NC contact
  * sits in series with the machine's STOP circuit: energise the coil, the
  * circuit opens, STOP is pressed. At rest both coils are off and the timer is
  * invisible -- the operator's own panel buttons work as if it were not there.
  *
  * Vocabulary: "on" means the button is PRESSED, i.e. the coil energised,
  * i.e. the pin HIGH. The RELAY_*_ACTIVE_LOW flags exist for a board where a
  * relay has to be driven the other way round; on rev 1 both are 0.
  *
  * The machine must never see both buttons pressed at once. That rule lives
  * here, in the driver, not in the callers: pressing one button lets go of
  * the other first, whatever the caller thinks the state is.
  *
  * Reset state: CubeMX drives every output LOW the moment it becomes an
  * output, which is exactly "at rest". Relay_Init() still runs from inside
  * MX_GPIO_Init(), so the driver's own state matches the pins from the first
  * microsecond.
  ******************************************************************************
  */

#ifndef __RELAY_H
#define __RELAY_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Polarity ------------------------------------------------------------------*/

/* 0 = "pressed" is a HIGH pin (coil energised), 1 = "pressed" is a LOW pin
   (coil released). Rev 1: both relays are energised to press. */
#define RELAY_START_ACTIVE_LOW    0u
#define RELAY_STOP_ACTIVE_LOW     0u


/* Types ---------------------------------------------------------------------*/

typedef enum
{
  RELAY_START = 0,
  RELAY_STOP  = 1
} relay_t;


/* Debug ---------------------------------------------------------------------*/

/**
  * Live snapshot of the relays for the IAR Live Watch window.
  */
typedef struct
{
  uint32_t start_pulses;   /* times START was pressed                        */
  uint32_t stop_pulses;    /* times STOP was pressed                         */
  uint32_t forced_off;     /* times the other button had to be let go first  */

  uint8_t  start_on;       /* 1 = START pressed right now (coil energised)   */
  uint8_t  stop_on;        /* 1 = STOP pressed right now (coil energised)    */
} rel_debug_t;


extern volatile rel_debug_t g_rel;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Rest: neither button pressed, both coils off. Call from the
  *         MX_GPIO_Init_2 user section, right after the pins have become
  *         outputs.
  */
void Relay_Init(void);


/**
  * @brief  Press a button. The other one is let go first.
  */
void Relay_On(relay_t relay);


/**
  * @brief  Let go of a button.
  */
void Relay_Off(relay_t relay);


/**
  * @brief  Neither button pressed: the resting state.
  */
void Relay_AllOff(void);


/**
  * @brief  Is the button pressed?
  * @retval 1 pressed, 0 not
  */
uint8_t Relay_IsOn(relay_t relay);


#ifdef __cplusplus
}
#endif

#endif /* __RELAY_H */
