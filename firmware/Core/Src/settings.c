/**
  ******************************************************************************
  * @file           : settings.c
  * @brief          : Persistent settings in the last flash page
  ******************************************************************************
  */

#include "main.h"
#include "settings.h"


/* Flash layout --------------------------------------------------------------*/

/* STM32G071GB: 128 KB = 64 pages x 2 KB. The last page is ours; the linker
   script stops ROM one page early to match. */
#define SET_PAGE_INDEX       63u
#define SET_PAGE_ADDR        0x0801F800u

#define SET_SLOT_SIZE        8u                                /* one double word */
#define SET_SLOT_COUNT       (FLASH_PAGE_SIZE / SET_SLOT_SIZE)   /* 256            */

/* A record is one double word:
     low word  = SET_MAGIC | (~mode & 0xFF) << 8 | mode
     high word = 0
   An erased slot reads 0xFFFFFFFF in both words.

   The magic was 0x5E77 while there were five modes. It changed with the move
   to three, so the old records fail validation: the first power-on after the
   update repairs the page and starts over at SET_MODE_DEFAULT, whatever mode
   the old firmware had stored. */
#define SET_MAGIC            0x5E730000u
#define SET_MAGIC_MASK       0xFFFF0000u
#define SET_ERASED           0xFFFFFFFFu

#define SET_NO_SLOT          0xFFFFFFFFu


/* Private variables ---------------------------------------------------------*/

static uint8_t       s_mode;        /* mode in force                          */
static uint8_t       s_fault;       /* 1 = flash given up, RAM only           */
static uint8_t       s_first_boot;  /* 1 = page was blank at start-up         */
static uint32_t      s_next_slot;   /* first erased slot; SET_SLOT_COUNT = none */

volatile set_debug_t g_set;         /* live snapshot for the debugger         */


/* Private functions ---------------------------------------------------------*/

static uint32_t Settings_SlotAddr(uint32_t slot)
{
  return SET_PAGE_ADDR + (slot * SET_SLOT_SIZE);
}


static uint32_t Settings_Encode(uint8_t mode)
{
  uint32_t inverted = (uint32_t)(~mode) & 0xFFu;

  return SET_MAGIC | (inverted << 8) | (uint32_t)mode;
}


/**
  * @brief  Validate a record.
  * @retval the mode it holds, or 0 when the slot holds no valid record
  */
static uint8_t Settings_Decode(uint32_t lo, uint32_t hi)
{
  uint8_t mode     = (uint8_t)(lo & 0xFFu);
  uint8_t inverted = (uint8_t)((lo >> 8) & 0xFFu);

  if (hi != 0u)
  {
    return 0u;
  }

  if ((lo & SET_MAGIC_MASK) != SET_MAGIC)
  {
    return 0u;
  }

  if ((uint8_t)(~mode) != inverted)
  {
    return 0u;
  }

  if ((mode < SET_MODE_MIN) || (mode > SET_MODE_MAX))
  {
    return 0u;
  }

  return mode;
}


static void Settings_Read(uint32_t slot, uint32_t *lo, uint32_t *hi)
{
  const volatile uint32_t *word = (const volatile uint32_t *)Settings_SlotAddr(slot);

  *lo = word[0];
  *hi = word[1];
}


/**
  * @brief  Erase the page. Flash must be unlocked.
  * @retval 1 ok, 0 failed
  */
static uint8_t Settings_Erase(void)
{
  FLASH_EraseInitTypeDef erase;
  uint32_t               failed_page = 0u;

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.Banks     = FLASH_BANK_1;
  erase.Page      = SET_PAGE_INDEX;
  erase.NbPages   = 1u;

  g_set.erases++;

  if (HAL_FLASHEx_Erase(&erase, &failed_page) != HAL_OK)
  {
    g_set.errors++;

    return 0u;
  }

  s_next_slot = 0u;

  return 1u;
}


/**
  * @brief  Write one record and read it back. Flash must be unlocked.
  * @retval 1 ok, 0 failed
  */
static uint8_t Settings_Program(uint32_t slot, uint8_t mode)
{
  uint64_t record = (uint64_t)Settings_Encode(mode);   /* high word stays 0 */
  uint32_t lo;
  uint32_t hi;

  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, Settings_SlotAddr(slot), record) != HAL_OK)
  {
    g_set.errors++;

    return 0u;
  }

  Settings_Read(slot, &lo, &hi);

  if (Settings_Decode(lo, hi) != mode)
  {
    g_set.errors++;

    return 0u;
  }

  g_set.writes++;
  g_set.slot = slot;

  return 1u;
}


/**
  * @brief  Append one record in the next free slot.
  * @retval 1 ok, 0 failed or page full
  */
static uint8_t Settings_Append(uint8_t mode)
{
  uint8_t ok;

  if (s_next_slot >= SET_SLOT_COUNT)
  {
    return 0u;
  }

  HAL_FLASH_Unlock();

  /* Stale error flags would fail the first operation before it starts. */
  FLASH->SR = FLASH_SR_ERRORS;

  ok = Settings_Program(s_next_slot, mode);

  HAL_FLASH_Lock();

  if (ok != 0u)
  {
    s_next_slot++;
  }

  return ok;
}


/**
  * @brief  Fresh page with a single record in slot 0.
  * @retval 1 ok, 0 failed
  */
static uint8_t Settings_Rewrite(uint8_t mode)
{
  uint8_t ok = 0u;

  HAL_FLASH_Unlock();

  /* Stale error flags would fail the first operation before it starts. */
  FLASH->SR = FLASH_SR_ERRORS;

  if (Settings_Erase() != 0u)
  {
    ok = Settings_Program(0u, mode);
  }

  HAL_FLASH_Lock();

  if (ok != 0u)
  {
    s_next_slot = 1u;
  }

  return ok;
}


static void Settings_Fault(void)
{
  s_fault     = 1u;
  g_set.fault = 1u;
}


/* Exported functions --------------------------------------------------------*/

void Settings_Init(void)
{
  uint32_t slot;
  uint32_t lo;
  uint32_t hi;
  uint8_t  mode;
  uint8_t  last_valid = 0u;
  uint8_t  garbage    = 0u;

  s_mode       = SET_MODE_DEFAULT;
  s_fault      = 0u;
  s_first_boot = 0u;
  s_next_slot  = SET_SLOT_COUNT;

  g_set.page_addr  = SET_PAGE_ADDR;
  g_set.slot       = SET_NO_SLOT;
  g_set.writes     = 0u;
  g_set.erases     = 0u;
  g_set.errors     = 0u;
  g_set.ecc_errors = 0u;
  g_set.found      = 0u;
  g_set.first_boot = 0u;
  g_set.garbage    = 0u;
  g_set.healed     = 0u;
  g_set.fault      = 0u;

  /* Walk the log. The last valid record wins; the first erased slot is where
     the next record goes. Anything that is neither erased nor valid is
     garbage, and so is a read that tripped the ECC. */
  for (slot = 0u; slot < SET_SLOT_COUNT; slot++)
  {
    Settings_Read(slot, &lo, &hi);

    if ((lo == SET_ERASED) && (hi == SET_ERASED))
    {
      s_next_slot = slot;

      break;
    }

    mode = Settings_Decode(lo, hi);

    if (mode != 0u)
    {
      last_valid = mode;

      g_set.slot  = slot;
      g_set.found = 1u;
    }
    else
    {
      garbage = 1u;
    }
  }

  if (g_set.ecc_errors != 0u)
  {
    garbage = 1u;
  }

  if (last_valid != 0u)
  {
    s_mode = last_valid;
  }

  if (garbage != 0u)
  {
    g_set.garbage = 1u;

    /* Repair: fresh page, one record. Keep the operator's last good mode if
       there was one -- the damage is most likely a write that lost power. */
    if (Settings_Rewrite(s_mode) != 0u)
    {
      g_set.healed = 1u;
    }
    else
    {
      s_mode = SET_MODE_DEFAULT;

      Settings_Fault();
    }
  }
  else if (s_next_slot == 0u)
  {
    /* Blank page: the very first power-on, or a freshly erased chip. Write
       the default now, so this happens exactly once -- and so the flash gets
       its first real test on the bench rather than in the field. */
    s_first_boot     = 1u;
    g_set.first_boot = 1u;

    if (Settings_Append(SET_MODE_DEFAULT) == 0u)
    {
      if (Settings_Rewrite(SET_MODE_DEFAULT) == 0u)
      {
        Settings_Fault();
      }
    }
  }

  g_set.mode = s_mode;
}


uint8_t Settings_GetMode(void)
{
  return s_mode;
}


void Settings_SetMode(uint8_t mode)
{
  if ((mode < SET_MODE_MIN) || (mode > SET_MODE_MAX))
  {
    return;
  }

  if (mode == s_mode)
  {
    return;
  }

  /* RAM first: even if flash refuses, the device keeps working this session. */
  s_mode     = mode;
  g_set.mode = mode;

  if (s_fault != 0u)
  {
    return;
  }

  /* Next free slot. When the page is full, or the slot turns out unusable,
     start a fresh page. Neither works: the store is done for. */
  if (Settings_Append(mode) == 0u)
  {
    if (Settings_Rewrite(mode) == 0u)
    {
      Settings_Fault();
    }
  }
}


uint8_t Settings_IsFirstBoot(void)
{
  return s_first_boot;
}


uint8_t Settings_IsFaulty(void)
{
  return s_fault;
}


uint8_t Settings_NmiHook(void)
{
  if ((FLASH->ECCR & FLASH_ECCR_ECCD) == 0u)
  {
    return 0u;
  }

  /* Double ECC error on a flash read: the word we just read is garbage and
     will fail Settings_Decode(). Clear the flag (write 1) and carry on. */
  FLASH->ECCR |= FLASH_ECCR_ECCD;

  g_set.ecc_errors++;

  return 1u;
}
