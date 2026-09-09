/**
  ******************************************************************************
  * @file           : app.h
  * @brief          : Application: power-on signature, OFF / RUN, modes
  ******************************************************************************
  * Everything the device *does* is decided here; the other modules only know
  * how to drive their piece of hardware.
  *
  *   power-on   HL1 lit -> 1 s quiet -> beep 2 s + 3 lamp blinks -> OFF
  *   OFF        relays off, lamp dark, button listening
  *   RUN        relay cycle in the selected mode, lamp repeats the mode number
  *              with 3 s of dark between bursts, 4 h auto-off clock running
  *
  *   1 click    OFF -> RUN in the stored mode  /  RUN -> OFF
  *   2..6       select mode 1..5, store it, (re)start the cycle
  *   4 h        RUN -> OFF, 15 s alarm beep
  *
  * Every start of the cycle is announced with a 2 s beep, switching off by
  * hand with a 0.5 s one.
  *
  * The on-board LED HL1 mirrors the state for whoever has the lid open: lit
  * during start-up, a flash every 3 s in OFF, and in RUN a pattern per cycle
  * phase (see the APP_HL1_* constants).
  *
  * Fault. When the settings store gives up (see settings.h) the device keeps
  * working, with the mode held in RAM only, and says so: SOS instead of the
  * power-on beep, and a fast lamp flicker whenever it is OFF. In RUN the lamp
  * shows the mode as usual.
  *
  * First boot. A blank chip also plays the SOS instead of the power-on beep,
  * once; the lamp behaves normally, because nothing is wrong.
  ******************************************************************************
  */

#ifndef __APP_H
#define __APP_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Timing --------------------------------------------------------------------*/

/* Power-on signature. */
#define APP_STARTUP_PAUSE_MS       1000u
#define APP_STARTUP_BEEP_MS        2000u
#define APP_STARTUP_BLINKS            3u

/* Announcements. */
#define APP_START_BEEP_MS          2000u   /* every (re)start of the cycle   */
#define APP_STOP_BEEP_MS            500u   /* switched off by the operator   */
#define APP_AUTO_OFF_BEEP_MS      15000u   /* the 4 h clock ran out          */

/* Continuous running time after which the device switches itself off. */
#define APP_AUTO_OFF_MS        14400000u   /* 4 h                            */

/* Darkness between two mode bursts on the lamp while running. */
#define APP_MODE_BURST_GAP_MS      3000u

/* What the on-board LED HL1 says, as lit / dark milliseconds. */
#define APP_HL1_OFF_ON_MS           150u   /* OFF: one flash every 3 s        */
#define APP_HL1_OFF_OFF_MS         2850u
#define APP_HL1_START_ON_MS         300u   /* START relay pressed             */
#define APP_HL1_START_OFF_MS        100u
#define APP_HL1_STOP_ON_MS          100u   /* STOP relay pressed              */
#define APP_HL1_STOP_OFF_MS         300u
#define APP_HL1_BETWEEN_ON_MS       500u   /* between the pulses              */
#define APP_HL1_BETWEEN_OFF_MS      150u


/* Debug ---------------------------------------------------------------------*/

typedef enum
{
  APP_STARTUP_PAUSE     = 0,   /* quiet second after power-on               */
  APP_STARTUP_SIGNATURE = 1,   /* beep and blinks are playing               */
  APP_OFF               = 2,
  APP_RUN               = 3
} app_state_t;


/**
  * Live snapshot of the application for the IAR Live Watch window.
  */
typedef struct
{
  uint32_t run_ms;        /* time since switched on; auto-off at APP_AUTO_OFF_MS */
  uint32_t switch_on;     /* OFF -> RUN by the operator                     */
  uint32_t switch_off;    /* RUN -> OFF by the operator                     */
  uint32_t auto_off;      /* RUN -> OFF by the 4 h clock                    */
  uint32_t restarts;      /* cycle restarted while already running          */
  uint32_t mode_changes;  /* new mode selected                              */

  uint8_t  state;         /* app_state_t                                    */
  uint8_t  mode;          /* mode in force, 1..5                            */
  uint8_t  last_clicks;   /* last click series handled                      */
  uint8_t  fault;         /* 1 = settings store faulty, alarm was raised    */
} app_debug_t;


extern volatile app_debug_t g_app;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Begin the power-on signature. Call after every other module's
  *         Init, Settings_Init() included.
  */
void App_Init(void);


/**
  * @brief  Application state machine. Call from the main loop; never blocks.
  */
void App_Task(void);


#ifdef __cplusplus
}
#endif

#endif /* __APP_H */
