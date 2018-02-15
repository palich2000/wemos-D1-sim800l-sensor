/*
char my_hostname[33]  = "wemosD1";
char syslog_host[33]  = "192.168.0.106";
int syslog_port = 654;
IPAddress syslog_host_addr;  // Syslog host IP address
unsigned long syslog_host_refresh = 0;
uint16_t syslog_timer = 0;                  // Timer to re-enable syslog_level
*/

/*********************************************************************************************\
 * Syslog
 *
 * Example:
 *   snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_LOG "Any value %d"), value);
 *   AddLog(LOG_LEVEL_DEBUG);
 *
\*********************************************************************************************/
/*
char log_data[TOPSZ + MESSZ];
byte syslog_level = LOG_LEVEL_INFO;
byte seriallog_level;

void Syslog() {
    // Destroys log_data
    char syslog_preamble[64];  // Hostname + Id

    if ((static_cast<uint32_t>(syslog_host_addr) == 0) || ((millis() - syslog_host_refresh) > 60000)) {
        WiFi.hostByName(syslog_host, syslog_host_addr);
        syslog_host_refresh = millis();
    }
    if (PortUdp.beginPacket(syslog_host_addr, syslog_port)) {
        snprintf_P(syslog_preamble, sizeof(syslog_preamble), PSTR("%s ESP-"), my_hostname);
        memmove(log_data + strlen(syslog_preamble), log_data, sizeof(log_data) - strlen(syslog_preamble));
        log_data[sizeof(log_data) - 1] = '\0';
        memcpy(log_data, syslog_preamble, strlen(syslog_preamble));
        PortUdp.write(log_data);
        PortUdp.endPacket();
    } else {
        syslog_level = 0;
        syslog_timer = SYSLOG_TIMER;
        snprintf_P(log_data, sizeof(log_data), PSTR(D_LOG_APPLICATION D_SYSLOG_HOST_NOT_FOUND ". " D_RETRY_IN " %d " D_UNIT_SECOND), SYSLOG_TIMER);
        AddLog(LOG_LEVEL_INFO);
    }
}

void AddLog(byte loglevel) {
    char mxtime[9];  // 13:45:21

    snprintf_P(mxtime, sizeof(mxtime), PSTR("%02d" D_HOUR_MINUTE_SEPARATOR "%02d" D_MINUTE_SECOND_SEPARATOR "%02d"), RtcTime.hour, RtcTime.minute, RtcTime.second);

    if (loglevel <= seriallog_level) {
        Serial.printf("%s %s\n", mxtime, log_data);
    }
    if ((WL_CONNECTED == WiFi.status()) && (loglevel <= syslog_level)) {
        Syslog();
    }
}

void AddLog_P(byte loglevel, const char *formatP) {
    snprintf_P(log_data, sizeof(log_data), formatP);
    AddLog(loglevel);
}

void AddLog_P(byte loglevel, const char *formatP, const char *formatP2) {
    char message[100];
    snprintf_P(log_data, sizeof(log_data), formatP);
    snprintf_P(message, sizeof(message), formatP2);
    strncat(log_data, message, sizeof(log_data));
    AddLog(loglevel);
}
*/
/*********************************************************************************************\
 *
\*********************************************************************************************/
