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
  * Polarity lives here too. On PCB1_main_rev1 both drivers are wired the
  * wrong way round, so a LOW pin energises the relay. Everything above this
  * file speaks in "on" and "off"; the RELAY_*_ACTIVE_LOW flags translate.
  *
  * That makes start-up delicate: CubeMX drives every output LOW the moment it
  * becomes an output, which for an active-low relay means ON -- both at once.
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

/* 1 = the relay is energised by a LOW pin, 0 = by a HIGH pin. */
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
  uint32_t start_pulses;   /* times START_RELAY was switched on              */
  uint32_t stop_pulses;    /* times STOP_RELAY was switched on               */
  uint32_t forced_off;     /* times the other relay had to be dropped first  */

  uint8_t  start_on;       /* 1 = START_RELAY is on right now                */
  uint8_t  stop_on;        /* 1 = STOP_RELAY is on right now                 */
} rel_debug_t;


extern volatile rel_debug_t g_rel;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Both relays off. Call from the MX_GPIO_Init_2 user section, the
  *         moment the pins have become outputs -- see the note above.
  */
void Relay_Init(void);


/**
  * @brief  Switch a relay on. The other relay is switched off first.
  */
void Relay_On(relay_t relay);


/**
  * @brief  Switch a relay off.
  */
void Relay_Off(relay_t relay);


/**
  * @brief  Both relays off.
  */
void Relay_AllOff(void);


/**
  * @brief  Current state of a relay.
  * @retval 1 on, 0 off
  */
uint8_t Relay_IsOn(relay_t relay);


#ifdef __cplusplus
}
#endif

#endif /* __RELAY_H */
