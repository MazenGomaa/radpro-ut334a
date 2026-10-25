/*
 * Rad Pro
 * FM33 flash
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(FM33)

#include "../peripherals/flash.h"
#include "../fm33/device.h"

void initFlash(void)
{
}

bool verifyFlash(void)
{
    return true;
}

const uint8_t *readFlash(uint32_t source, uint32_t count)
{
    return (const uint8_t *)source;
}

bool writeFlash(uint32_t dest, const uint8_t *source, uint32_t count)
{
    return false;
}

bool eraseFlash(uint32_t dest)
{
    return false;
}

#endif
