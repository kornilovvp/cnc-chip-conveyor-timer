/**
  ******************************************************************************
  * @file           : relay.h
  * @brief          : START / STOP relays with mutual exclusion
  ******************************************************************************
  * Two relays stand in for the START and STOP buttons of the machine:
  *   START_RELAY = PA7 -> R6 1k -> solid-state relay -> K2
  *   STOP_RELAY  = PA5 -> R5 1k -> solid-state relay -> K1
  *
  * The machine must never see both buttons pressed at once. That rule lives
  * here, in the driver, not in the callers: switching one relay on forces the
  * other one off first, whatever the caller thinks the state is.
  *
  * Vocabulary: "on" means the button is PRESSED. Physically that is the relay
  * RELEASED. Both relays are held energised whenever the timer is idle: the
  * STOP relay's contact passes the machine's STOP circuit, the START relay's
  * contact keeps START open, and the operator's own panel buttons keep working
  * through them -- the timer is invisible until it is switched on. To press a
  * button the firmware lets go of its relay for the pulse. The RELAY_*_ACTIVE_LOW
  * flags express that at the pin: HIGH drives the solid-state relay and holds
  * the coil, LOW lets it go.
  *
  * That makes start-up delicate: CubeMX drives every output LOW the moment it
  * becomes an output -- both relays released, both buttons "pressed" at once.
  * Relay_Init() is therefore called from inside MX_GPIO_Init(), microseconds
  * later, well inside the 1.5 ms the solid-state relays need to react.
  ******************************************************************************
  */

#ifndef __RELAY_H
#define __RELAY_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Polarity ------------------------------------------------------------------*/

/* 1 = "pressed" is a LOW pin (coil released), 0 = "pressed" is a HIGH pin
   (coil energised). Both buttons on this board are pressed by releasing. */
#define RELAY_START_ACTIVE_LOW    1u
#define RELAY_STOP_ACTIVE_LOW     1u


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

  uint8_t  start_on;       /* 1 = START pressed right now (coil released)    */
  uint8_t  stop_on;        /* 1 = STOP pressed right now (coil released)     */
} rel_debug_t;


extern volatile rel_debug_t g_rel;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Rest: neither button pressed, both coils held. Call from the
  *         MX_GPIO_Init_2 user section, the moment the pins have become
  *         outputs -- see the note above.
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
