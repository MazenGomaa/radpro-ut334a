/*
 * Rad Pro
 * FM33 device definitions
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#include <stdbool.h>
#include <stdint.h>

#if defined(UT334A)
#include "ut334a.h"
#endif

// fm33_system.c
void initSystemClock(void);
void startSystemPLL(void);

// fm33_tube.c
void onTubeHVTick(void);
