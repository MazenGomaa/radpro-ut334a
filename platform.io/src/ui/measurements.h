/*
 * Rad Pro
 * UI measurements
 *
 * (C) 2022-2026 Gissio
 *
 * License: MIT
 */

#if !defined(UI_MEASUREMENTS_H)
#define UI_MEASUREMENTS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MEASUREMENTSTYLE_NORMAL,
    MEASUREMENTSTYLE_WARNING,
    MEASUREMENTSTYLE_ALARM,
    MEASUREMENTSTYLE_DONE,
} MeasurementStyle;

void drawMeasurementValue(const char *valueString, const char *unitString, float confidence, MeasurementStyle style);
void drawMeasurementAlert(const char *alertString);
void drawMeasurementInfo(const char *keyString, const char *valueString, const char *unitString, MeasurementStyle style);
void drawMeasurementBar(float value, int32_t minExponent, float warningValue, float alarmValue);
#if defined(MEASUREMENT_OVERVIEW)
void drawOverviewValue(const char *valueString, const char *unitString, float confidence, MeasurementStyle style);
void drawOverviewRow(uint32_t index, const char *keyString, const char *timeString, const char *valueString, const char *unitString, bool isHold);
void drawOverviewBar(float value, int32_t minExponent, float warningValue, float alarmValue);
#endif

#endif
