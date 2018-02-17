#include "support.h"

/*********************************************************************************************\
 * RTC memory
\*********************************************************************************************/

#define RTC_MEM_VALID 0xA55A

uint32_t rtc_settings_hash = 0;

uint32_t GetRtcSettingsHash() {
    uint32_t hash = 0;
    uint8_t *bytes = (uint8_t*)&RtcSettings;

    for (uint16_t i = 0; i < sizeof(RTCMEM); i++) {
        hash += bytes[i] * (i + 1);
    }
    return hash;
}

void RtcSettingsSave() {
    if (GetRtcSettingsHash() != rtc_settings_hash) {
        RtcSettings.valid = RTC_MEM_VALID;
        ESP.rtcUserMemoryWrite(100, (uint32_t*)&RtcSettings, sizeof(RTCMEM));
        rtc_settings_hash = GetRtcSettingsHash();
    }
}

void RtcSettingsLoad() {
    ESP.rtcUserMemoryRead(100, (uint32_t*)&RtcSettings, sizeof(RTCMEM));
    if (RtcSettings.valid != RTC_MEM_VALID) {
        memset(&RtcSettings, 0, sizeof(RTCMEM));
        RtcSettings.valid = RTC_MEM_VALID;
        RtcSettingsSave();
    }
    rtc_settings_hash = GetRtcSettingsHash();
}

boolean RtcSettingsValid() {
    return (RTC_MEM_VALID == RtcSettings.valid);
}

/*******************************************************************************************/

void SettingsLoad() {
    RtcSettingsLoad();
}

/*******************************************************************************************/
#define SECS_PER_MIN  ((uint32_t)(60UL))
#define SECS_PER_HOUR ((uint32_t)(3600UL))
#define SECS_PER_DAY  ((uint32_t)(SECS_PER_HOUR * 24UL))
#define LEAP_YEAR(Y)  (((1970+Y)>0) && !((1970+Y)%4) && (((1970+Y)%100) || !((1970+Y)%400)))
#define D_MONTH3LIST "JanFebMarAprMayJunJulAugSepOctNovDec"
static const char kMonthNames[] = D_MONTH3LIST;

static const uint8_t kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }; // API starts months from 1, this array starts from 0

uint32_t utc_time = 0;
uint32_t local_time = 0;
uint32_t daylight_saving_time = 0;
uint32_t standard_time = 0;
uint32_t ntp_time = 0;
uint32_t midnight = 1451602800;
uint8_t  midnight_now = 0;

void SetUTCTime(uint32_t t) {
    utc_time = t;
}

uint32_t MakeTime(TIME_T &tm) {
// assemble time elements into time_t
// note year argument is offset from 1970

    int i;
    uint32_t seconds;

    // seconds from 1970 till 1 jan 00:00:00 of the given year
    seconds = tm.year * (SECS_PER_DAY * 365);
    for (i = 0; i < tm.year; i++) {
        if (LEAP_YEAR(i)) {
            seconds +=  SECS_PER_DAY;   // add extra days for leap years
        }
    }

    // add days for this year, months start from 1
    for (i = 1; i < tm.month; i++) {
        if ((2 == i) && LEAP_YEAR(tm.year)) {
            seconds += SECS_PER_DAY * 29;
        } else {
            seconds += SECS_PER_DAY * kDaysInMonth[i - 1]; // monthDay array starts from 0
        }
    }
    seconds += (tm.day_of_month - 1) * SECS_PER_DAY;
    seconds += tm.hour * SECS_PER_HOUR;
    seconds += tm.minute * SECS_PER_MIN;
    seconds += tm.second;
    return seconds;
}

void BreakTime(uint32_t time_input, TIME_T &tm) {
// break the given time_input into time components
// this is a more compact version of the C library localtime function
// note that year is offset from 1970 !!!

    uint8_t year;
    uint8_t month;
    uint8_t month_length;
    uint32_t time;
    unsigned long days;

    time = time_input;
    tm.second = time % 60;
    time /= 60;                // now it is minutes
    tm.minute = time % 60;
    time /= 60;                // now it is hours
    tm.hour = time % 24;
    time /= 24;                // now it is days
    tm.day_of_week = ((time + 4) % 7) + 1;  // Sunday is day 1

    year = 0;
    days = 0;
    while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
        year++;
    }
    tm.year = year;            // year is offset from 1970

    days -= LEAP_YEAR(year) ? 366 : 365;
    time -= days;              // now it is days in this year, starting at 0
    tm.day_of_year = time;

    days = 0;
    month = 0;
    month_length = 0;
    for (month = 0; month < 12; month++) {
        if (1 == month) { // february
            if (LEAP_YEAR(year)) {
                month_length = 29;
            } else {
                month_length = 28;
            }
        } else {
            month_length = kDaysInMonth[month];
        }

        if (time >= month_length) {
            time -= month_length;
        } else {
            break;
        }
    }
    strlcpy(tm.name_of_month, kMonthNames + (month * 3), 4);
    tm.month = month + 1;      // jan is month 1
    tm.day_of_month = time + 1;         // day of month
    tm.valid = (time_input > 1451602800);  // 2016-01-01
}

Ticker TickerRtc;

static bool need_time_sync = false;

bool is_need_sync_time() {
    return need_time_sync;
}

void sync_time_done() {
    need_time_sync = false;
}

void RtcSecond() {

    if (RtcTime.year < 2016) {
        need_time_sync = true;  // Initial GSM sync
    } else {
        if ((1 == RtcTime.minute) && (1 == RtcTime.second)) {
            need_time_sync = true;  // Hourly GSM sync at xx:01:01
        }
    }

    utc_time++;
    local_time = utc_time;
    BreakTime(local_time, RtcTime);
    RtcTime.year += 1970;
}

void RtcInit() {
    utc_time = 0;
    BreakTime(utc_time, RtcTime);
    TickerRtc.attach(1, RtcSecond);
}

String GetUtcDateAndTime() {
    // "2017-03-07T11:08:02" - ISO8601:2004
    char dt[21];

    TIME_T tmpTime;
    BreakTime(utc_time, tmpTime);
    tmpTime.year += 1970;

    snprintf_P(dt, sizeof(dt), PSTR("%04d/%02d/%02dT%02d:%02d:%02d"),
               tmpTime.year, tmpTime.month, tmpTime.day_of_month, tmpTime.hour, tmpTime.minute, tmpTime.second);
    return String(dt);
}

