/**
  ******************************************************************************
  * @file           : settings.h
  * @brief          : Persistent settings in the last flash page
  ******************************************************************************
  * The STM32G0 has no EEPROM, so the selected mode lives in the last 2 KB
  * page of program flash. To spare the page (10 000 erase cycles rated) the
  * value is not rewritten in place: records are appended one after another,
  * the last valid one wins, and the page is erased only when all 256 slots
  * are used. That is one erase per 256 mode changes.
  *
  * The page is reserved in the linker script (stm32g071xx_flash.icf: ROM ends
  * at 0x0801F7FF), so no code can ever land there.
  *
  * Damage. A write cut short by a power loss leaves a slot that fails
  * validation, or whose ECC is broken -- reading that raises an NMI on this
  * MCU, which Settings_NmiHook() absorbs. Either way Settings_Init() sees
  * "garbage" and repairs the page: erase, rewrite the last good mode (or the
  * default when there was none), read it back. If even that fails the store
  * declares itself faulty: the mode still works from RAM, but nothing is
  * written any more, and the application raises the alarm.
  *
  * A blank page (first power-on) is not damage: the default mode is written
  * at once, so it happens exactly once, and the application announces it.
  ******************************************************************************
  */

#ifndef __SETTINGS_H
#define __SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>


/* Limits --------------------------------------------------------------------*/

#define SET_MODE_MIN          1u
#define SET_MODE_MAX          5u
#define SET_MODE_DEFAULT      1u   /* first power-on, or nothing valid found */


/* Debug ---------------------------------------------------------------------*/

/**
  * Live snapshot of the settings store for the IAR Live Watch window.
  */
typedef struct
{
  uint32_t page_addr;    /* flash page in use                                */
  uint32_t slot;         /* slot of the record in force; 0xFFFFFFFF = none   */
  uint32_t writes;       /* records written since reset                      */
  uint32_t erases;       /* page erases since reset                          */
  uint32_t errors;       /* flash operations that failed                     */
  uint32_t ecc_errors;   /* NMIs absorbed while reading a damaged slot       */

  uint8_t  mode;         /* mode currently in force                          */
  uint8_t  found;        /* 1 = a valid record was found at start-up         */
  uint8_t  first_boot;   /* 1 = page was blank at start-up, default written  */
  uint8_t  garbage;      /* 1 = damaged data was found at start-up           */
  uint8_t  healed;       /* 1 = the page was repaired at start-up            */
  uint8_t  fault;        /* 1 = flash unusable, running from RAM only        */
} set_debug_t;


extern volatile set_debug_t g_set;


/* API -----------------------------------------------------------------------*/

/**
  * @brief  Read the stored mode, repairing the page if it holds damaged data.
  *         Call once at start-up, before App_Init(). Falls back to
  *         SET_MODE_DEFAULT when the page is empty or beyond repair.
  */
void Settings_Init(void);


/**
  * @brief  Mode in force.
  * @retval SET_MODE_MIN..SET_MODE_MAX
  */
uint8_t Settings_GetMode(void);


/**
  * @brief  Store a new mode. Nothing is written when the value is unchanged,
  *         or when the store is faulty (the value is still kept in RAM).
  *         A page erase, when one is due, stalls the CPU for a few tens of
  *         milliseconds: the SysTick catches up afterwards.
  * @param  mode  SET_MODE_MIN..SET_MODE_MAX; anything else is ignored
  */
void Settings_SetMode(uint8_t mode);


/**
  * @brief  Was the page blank at start-up -- the very first power-on, or a
  *         freshly erased chip? The default mode has been written by now.
  * @retval 1 first boot, 0 not
  */
uint8_t Settings_IsFirstBoot(void);


/**
  * @brief  Has the flash store given up? Set at start-up when the repair
  *         fails, or later when a write fails even after a fresh erase.
  * @retval 1 faulty, 0 healthy
  */
uint8_t Settings_IsFaulty(void);


/**
  * @brief  Call first thing from NMI_Handler().
  * @retval 1  the NMI came from a flash ECC error and has been dealt with;
  *            the handler may return
  * @retval 0  not ours
  */
uint8_t Settings_NmiHook(void);


#ifdef __cplusplus
}
#endif

#endif /* __SETTINGS_H */
