/*
 * Rad Pro
 * FM33 RTC
 *
 * (C) 2026 Rad Pro UT334A port contributors
 *
 * License: MIT
 *
 * The FM33 RTC is a BCD calendar that runs from power-up on LSCLK (XTLF; LPOSC until the crystal
 * starts). Reads are repeated until two match (datasheet 30.3.6); writes need RTC_WER = 0xACACACAC.
 */

#if defined(FM33)

#include "../fm33/device.h"
#include "../peripherals/rtc.h"

#define RTC_WER_KEY 0xACACACAC

static uint32_t encodeBCD(uint32_t value)
{
    return ((value / 10) << 4) | (value % 10);
}

static uint32_t decodeBCD(uint32_t value)
{
    return 10 * ((value >> 4) & 0xf) + (value & 0xf);
}

void initRTC(void)
{
    rcc_enable_rtc();
}

bool setDeviceTime(uint32_t value)
{
    RTCDateTime dateTime;
    getDateTimeFromTime(value, &dateTime);

    RTC->WER = RTC_WER_KEY;
    RTC->BCDSEC = encodeBCD(dateTime.second);
    RTC->BCDMIN = encodeBCD(dateTime.minute);
    RTC->BCDHOUR = encodeBCD(dateTime.hour);
    RTC->BCDDAY = encodeBCD(dateTime.day);
    RTC->BCDWEEK = (value / 86400 + 4) % 7; // 1970-01-01 was a Thursday; 0 = Sunday
    RTC->BCDMONTH = encodeBCD(dateTime.month);
    RTC->BCDYEAR = encodeBCD(dateTime.year % 100);
    RTC->WER = 0;

    return true;
}

static void readRTC(uint32_t *time, uint32_t *date)
{
    *time = (RTC->BCDHOUR << 16) | (RTC->BCDMIN << 8) | RTC->BCDSEC;
    *date = (RTC->BCDYEAR << 16) | (RTC->BCDMONTH << 8) | RTC->BCDDAY;
}

uint32_t getDeviceTime(void)
{
    uint32_t time, date, time2, date2;
    readRTC(&time, &date);
    while (true)
    {
        readRTC(&time2, &date2);
        if ((time == time2) && (date == date2))
            break;

        time = time2;
        date = date2;
    }

    RTCDateTime dateTime;
    dateTime.year = 2000 + decodeBCD(date >> 16);
    dateTime.month = decodeBCD((date >> 8) & 0x1f);
    dateTime.day = decodeBCD(date & 0x3f);
    dateTime.hour = decodeBCD(time >> 16);
    dateTime.minute = decodeBCD((time >> 8) & 0x7f);
    dateTime.second = decodeBCD(time & 0x7f);

    return getTimeFromDateTime(&dateTime);
}

uint32_t getDeviceTimeFast(void)
{
    return (RTC->BCDMIN << 8) | RTC->BCDSEC;
}

#endif
