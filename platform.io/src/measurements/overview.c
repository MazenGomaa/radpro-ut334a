/*
 * Rad Pro
 * Overview: instantaneous rate, average rate and cumulative dose on one screen
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 */

#if defined(MEASUREMENT_OVERVIEW)

#include "../measurements/average.h"
#include "../measurements/cumulative.h"
#include "../measurements/instantaneous.h"
#include "../measurements/measurements.h"
#include "../measurements/overview.h"
#include "../system/cstring.h"
#include "../system/events.h"
#include "../system/settings.h"
#include "../ui/measurements.h"
#include "../ui/system.h"

typedef enum
{
    OVERVIEW_TAB_AVERAGE_CUMULATIVE,
    OVERVIEW_TAB_MAX_RATE,
    OVERVIEW_TAB_BAR,

    OVERVIEW_TAB_NUM,

    OVERVIEW_TAB_ALERT = OVERVIEW_TAB_NUM,
} OverviewTab;

static OverviewTab overviewTab;

static void advanceOverviewTab(void)
{
    uint32_t tabNum = getTubeFaultAlertLevel() ? (OVERVIEW_TAB_NUM + 1) : OVERVIEW_TAB_NUM;

    overviewTab++;
    if (overviewTab >= tabNum)
        overviewTab = OVERVIEW_TAB_AVERAGE_CUMULATIVE;
}

static MeasurementStyle getAlertLevelMeasurementStyle(AlertLevel alertLevel)
{
    if (alertLevel == ALERTLEVEL_ALARM)
        return MEASUREMENTSTYLE_ALARM;
    else if (alertLevel == ALERTLEVEL_WARNING)
        return MEASUREMENTSTYLE_WARNING;
    else
        return MEASUREMENTSTYLE_NORMAL;
}

static void drawOverviewAverageCumulative(void)
{
    char valueString[32] = "";
    char unitString[32] = " ";
    char timeString[16] = "";

    // Average rate
    buildValueString(valueString, unitString, getAverageRate(), &pulseUnits[settings.doseUnits].rate, doseUnitsMinMetricPrefix[settings.doseUnits]);
    strcatTime(timeString, getAverageRateTime());
    drawOverviewRow(0, getString(STRING_AVERAGE), timeString, valueString, unitString, isAverageRateDone());

    // Cumulative dose
    strclr(valueString);
    strcpy(unitString, " ");
    strclr(timeString);
    buildValueString(valueString, unitString, getCumulativeDosePulseCount(), &pulseUnits[settings.doseUnits].dose, doseUnitsMinMetricPrefix[settings.doseUnits]);
    strcatTime(timeString, getCumulativeDoseTime());
    drawOverviewRow(1, getString(STRING_CUMULATIVE), timeString, valueString, unitString, false);
}

static void drawOverviewMaxRate(void)
{
    char valueString[32] = "";
    char unitString[32] = " ";

    // Maximum instantaneous rate
    buildValueString(valueString, unitString, getInstantaneousRateMax(), &pulseUnits[settings.doseUnits].rate, doseUnitsMinMetricPrefix[settings.doseUnits]);
    drawOverviewRow(0, getString(STRING_MAX), "", valueString, unitString, false);

    // Instantaneous rate in the secondary units
    strclr(valueString);
    strcpy(unitString, " ");
    buildValueString(valueString, unitString, getInstantaneousRate(), &pulseUnits[settings.secondaryDoseUnits].rate, doseUnitsMinMetricPrefix[settings.secondaryDoseUnits]);
    drawOverviewRow(1, getString(STRING_RATE), "", valueString, unitString, false);
}

static void drawOverviewView(void)
{
    char valueString[32] = "";
    char unitString[32] = "";

    drawTitleBar(getString(STRING_MEASUREMENTS));

    // Instantaneous rate
    buildValueString(valueString, unitString, getInstantaneousRate(), &pulseUnits[settings.doseUnits].rate, doseUnitsMinMetricPrefix[settings.doseUnits]);
    drawOverviewValue(valueString, unitString, getInstantaneousRateConfidence(), getAlertLevelMeasurementStyle(getInstantaneousRateAlertLevel()));

    switch (overviewTab)
    {
    case OVERVIEW_TAB_AVERAGE_CUMULATIVE:
        drawOverviewAverageCumulative();

        break;

    case OVERVIEW_TAB_MAX_RATE:
        drawOverviewMaxRate();

        break;

    case OVERVIEW_TAB_BAR:
    {
        float scale = pulseUnits[settings.doseUnits].rate.scale;
        float microSievertPerHourScale = scale / pulseUnits[DOSE_UNITS_SIEVERTS].rate.scale;

        drawOverviewBar(getInstantaneousRate() * scale,
                        doseUnitsBarMinExponent[settings.doseUnits],
                        rateAlerts[settings.rateWarning] * microSievertPerHourScale,
                        rateAlerts[settings.rateAlarm] * microSievertPerHourScale);

        break;
    }

    default:
        drawOverviewRow(0, getString(STRING_ALERT_FAULT), "", "", "", false);
        drawOverviewRow(1, "", "", "", "", false);

        break;
    }
}

void onOverviewViewEvent(ViewEvent event)
{
    if (onMeasurementViewEvent(event))
        return;

    switch (event)
    {
    case EVENT_KEY_BACK:
        advanceOverviewTab();

        requestViewUpdate();

        break;

    case EVENT_KEY_RESET:
        triggerVibration();

        // As the stock "Delete Current Data": rate and maximum, average rate, cumulative dose
        clearInstantaneousRate();
        clearAverageRate();
        clearCumulativeDose();

        if (overviewTab == OVERVIEW_TAB_ALERT)
            overviewTab = OVERVIEW_TAB_AVERAGE_CUMULATIVE;

        requestViewUpdate();

        break;

    case EVENT_DRAW:
        drawOverviewView();

        break;

    default:
        break;
    }
}

#endif
