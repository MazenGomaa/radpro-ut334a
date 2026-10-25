/*
 * Rad Pro
 * FM33 flash
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * Page erase (512 bytes) and 32-bit programming as in the FMSH flash algorithm
 * (FM33LC04X_FLASH256.FLM: Init, EraseSector, ProgramPage) and datasheet 7.4.3. Only the settings and
 * datalog area after the firmware slot can be erased or written.
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../peripherals/flash.h"

#define FIRMWARE_PAYLOAD_SIZE (FIRMWARE_SIZE - 0x4)
#define FIRMWARE_CRC (*(uint32_t *)(FIRMWARE_BASE + FIRMWARE_PAYLOAD_SIZE))

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

__asm__(".section .comment\n"
        ".string \"FLASH_BASE: " TOSTRING(FLASH_BASE_) "\"\n"
        ".string \"FLASH_SIZE: " TOSTRING(FLASH_SIZE_) "\"\n"
        ".string \"FIRMWARE_BASE: " TOSTRING(FIRMWARE_BASE) "\"\n"
        ".string \"FIRMWARE_SIZE: " TOSTRING(FIRMWARE_SIZE) "\"\n"
        ".string \"STATES_BASE: " TOSTRING(STATES_BASE) "\"\n"
        ".string \"STATES_SIZE: " TOSTRING(STATES_SIZE) "\"\n"
        ".string \"DATALOG_BASE: " TOSTRING(DATALOG_BASE) "\"\n"
        ".string \"DATALOG_SIZE: " TOSTRING(DATALOG_SIZE) "\"\n"
        ".section .text\n");

#define FLASH_ERASE_KEY1 0x96969696
#define FLASH_ERASE_KEY2_PAGE 0xEAEAEAEA
#define FLASH_ERASE_REQUEST 0x1234ABCD
#define FLASH_PROGRAM_KEY1 0xA5A5A5A5
#define FLASH_PROGRAM_KEY2 0xF1F1F1F1

#define FLASH_ERASE_TIMEOUT_US 50000
#define FLASH_PROGRAM_TIMEOUT_US 1000
#define FLASH_POLL_US 10

#define FLASH_ISR_ERRORS (FLASH_ISR_OTPERR_Msk | FLASH_ISR_AUTHERR_Msk | FLASH_ISR_KEYERR_Msk | FLASH_ISR_CKERR_Msk)

void initFlash(void)
{
}

// CRC-32/MPEG-2 on 32-bit little-endian words, as the STM32 CRC unit and tools/sign.py compute it
static const uint32_t crcNibbleTable[16] = {
    0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9,
    0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005,
    0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61,
    0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
};

bool verifyFlash(void)
{
    uint32_t crc = 0xffffffff;
    for (uint32_t i = 0; i < FIRMWARE_PAYLOAD_SIZE; i += 4)
    {
        crc ^= *(const uint32_t *)(FIRMWARE_BASE + i);
        for (uint32_t j = 0; j < 8; j++)
            crc = (crc << 4) ^ crcNibbleTable[crc >> 28];
    }

    return (crc == FIRMWARE_CRC);
}

const uint8_t *readFlash(uint32_t address, uint32_t count)
{
    return (const uint8_t *)address;
}

static bool isFlashAddressWritable(uint32_t address, uint32_t count)
{
    return (address >= STATES_BASE) && ((address + count) <= FLASH_END_) && !(address & 0b11);
}

static void unlockFlashClocks(void)
{
    set_bits(RCC->PCLKCR2, RCC_PCLKCR2_FLASH_PCE_Msk);
    set_bits(RCC->OPCCR2, RCC_OPCCR2_FLASHCKE_Msk);
}

static void lockFlash(void)
{
    // No KEY write: each operation already closed with KEY = 0, and KEY = 0 in the protected state is a
    // key error that locks the controller until a power-on reset (bench, 2026-10-25)
    clear_bits(RCC->OPCCR2, RCC_OPCCR2_FLASHCKE_Msk);
}

static bool waitFlash(uint32_t flag, uint32_t timeout)
{
    bool done = false;
    for (uint32_t t = 0; t < timeout; t += FLASH_POLL_US)
    {
        if (get_bits(FLASH->ISR, flag))
        {
            done = true;

            break;
        }

        fm33ExtUsleep(FLASH_POLL_US);
    }

    bool errors = get_bits(FLASH->ISR, FLASH_ISR_ERRORS);

    // Write 1 to clear
    FLASH->ISR = flag | FLASH_ISR_ERRORS;

    return done && !errors;
}

static bool programWord(uint32_t address, uint32_t value)
{
    clear_bits(FLASH->EPCR, FLASH_EPCR_EREQ_Msk);
    set_bits(FLASH->EPCR, FLASH_EPCR_PREQ_Msk);
    FLASH->KEY = FLASH_PROGRAM_KEY1;
    FLASH->KEY = FLASH_PROGRAM_KEY2;

    *(volatile uint32_t *)address = value;

    bool success = waitFlash(FLASH_ISR_PRD_Msk, FLASH_PROGRAM_TIMEOUT_US);

    FLASH->KEY = 0;

    return success;
}

bool writeFlash(uint32_t address, const uint8_t *data, uint32_t count)
{
    if (!isFlashAddressWritable(address, count))
        return false;

    unlockFlashClocks();

    bool errors = false;
    for (uint32_t i = 0; i < count; i += FLASH_WORD_SIZE)
    {
        uint32_t value = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) | (data[i + 3] << 24);

        // Try twice
        bool success = false;
        for (uint32_t j = 0; j < 2; j++)
        {
            if (programWord(address + i, value))
            {
                success = true;

                break;
            }
        }

        if (!success)
        {
            errors = true;

            break;
        }
    }

    lockFlash();

    return !errors;
}

static bool erasePage(uint32_t address)
{
    modify_bits(FLASH->EPCR, FLASH_EPCR_ERTYPE_Msk, 0b00 << FLASH_EPCR_ERTYPE_Pos);
    clear_bits(FLASH->EPCR, FLASH_EPCR_PREQ_Msk);
    set_bits(FLASH->EPCR, FLASH_EPCR_EREQ_Msk);
    FLASH->KEY = FLASH_ERASE_KEY1;
    FLASH->KEY = FLASH_ERASE_KEY2_PAGE;

    *(volatile uint32_t *)address = FLASH_ERASE_REQUEST;

    bool success = waitFlash(FLASH_ISR_ERD_Msk, FLASH_ERASE_TIMEOUT_US);

    FLASH->KEY = 0;

    return success;
}

bool eraseFlash(uint32_t address)
{
    address = getFlashPageBase(address);
    if (!isFlashAddressWritable(address, FLASH_PAGE_SIZE))
        return false;

    unlockFlashClocks();

    // Try twice
    bool success = false;
    for (uint32_t i = 0; i < 2; i++)
    {
        if (erasePage(address))
        {
            success = true;

            break;
        }
    }

    lockFlash();

    return success;
}

#endif
