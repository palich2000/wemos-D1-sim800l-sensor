#include <WiFiUdp.h>
#include "support.h"


WiFiUDP udpClient;
Syslog * syslog = NULL;


#define SYSLOG_SERVER "192.168.0.106"
#define SYSLOG_PORT 514

#define DEVICE_HOSTNAME "wemosD1"
#define APP_NAME "sim800"


void SyslogInit() {
    snprintf_P(my_hostname, sizeof(my_hostname) - 1, "wemos-%d", ESP.getChipId() & 0x1FFF);
    syslog = new Syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, my_hostname, APP_NAME);
    if (syslog) {
        syslog->SetGetStrDateAndTime(GetUtcDateAndTime);
    }
}

