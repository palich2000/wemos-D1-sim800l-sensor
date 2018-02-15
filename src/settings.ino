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
