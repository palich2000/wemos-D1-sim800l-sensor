#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <Ticker.h>                         // RTC, HLW8012, OSWatch
#include <ESP8266httpUpdate.h>
#include <ESP8266WebServer.h>
#include "settings.h"
#include "support.h"

const char* ssid = "yt201";
const char* password = "zhopa3600";

ESP8266WebServer _web_server(80);

void setup_web_server() {

    _web_server.on("/gprs", HTTP_POST, [&]() {
        digitalWrite(BUILTIN_LED, HIGH);
        SYSLOG(LOG_INFO, "/gprs %s", _web_server.arg("data").c_str());
        String result;
        post_data_via_gsm_http("sip.trophy.com.ua", 3000, "/mqtt", _web_server.arg("data"), result);
        _web_server.send (200, "text/json", result);
        digitalWrite(BUILTIN_LED, LOW);
    });

    _web_server.on("/time", HTTP_POST, [&]() {
        digitalWrite(BUILTIN_LED, HIGH);
        int res = gsm_sync_time();
        _web_server.send (200, "text/json", "{\"result\":\"" + String(res) + "\"}");
        digitalWrite(BUILTIN_LED, LOW);
    });

    _web_server.on("/modem", HTTP_POST, [&]() {
        digitalWrite(BUILTIN_LED, HIGH);
        SYSLOG(LOG_INFO, "command='%s'", _web_server.arg("cmd").c_str());
        String res;
        command_to_modem(_web_server.arg("cmd"), res);
        _web_server.send (200, "text/json", "{\"result\":\"" + res + "\"}");
        digitalWrite(BUILTIN_LED, LOW);
    });

    _web_server.on("/esp", HTTP_POST, [&]() {
        HTTPUpdateResult ret = ESPhttpUpdate.update(_web_server.arg("firmware"), "1.0.0");
        SYSLOG(LOG_INFO, "%s", _web_server.arg("firmware").c_str());
        switch(ret) {
        case HTTP_UPDATE_FAILED:
            _web_server.send (500, "text/json", "{\"result\":false,\"msg\":\"Update failed.\"}");
            Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            _web_server.send (304, "text/json", "{\"result\":true,\"msg\":\"Update not necessary.\"}");
            Serial.println("Update not necessary.");
            break;
        case HTTP_UPDATE_OK:
            _web_server.send (200, "text/json", "{\"result\":true,\"msg\":\"Update started.\"}");
            Serial.println("Update started");
            ESP.restart();
            break;
        }
    });
    _web_server.begin();
}

char my_hostname[33] = {};

#define PIR_IN D0

void setup() {
    pinMode(BUILTIN_LED, OUTPUT);
    digitalWrite(BUILTIN_LED, LOW);

    Serial.begin(115200);
    Serial.println("");
    Serial.println("");
    Serial.println("Booting");

    SettingsLoad();
    OsWatchInit();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(1000);
        WiFi.forceSleepBegin();
        wdt_reset();
        ESP.restart();
        while(1)wdt_reset();
    }

    RtcInit();

    SyslogInit();
    delay(100);
    SYSLOG(LOG_INFO, "=======Begin setup======");
    SYSLOG(LOG_INFO, "IP address: %s", WiFi.localIP().toString().c_str());
    SYSLOG(LOG_INFO, "Chip ID = %08X", ESP.getChipId());
    SYSLOG(LOG_INFO, "Buil timestamp: %s", __TIMESTAMP__);
    SYSLOG(LOG_INFO, "Sketch size: %d", ESP.getSketchSize());
    SYSLOG(LOG_INFO, "Free space: %d", ESP.getFreeSketchSpace());
    SYSLOG(LOG_INFO, "Chip ID: %d", ESP.getFlashChipId());
    SYSLOG(LOG_INFO, "Chip Real Size: %d", ESP.getFlashChipRealSize());
    SYSLOG(LOG_INFO, "Chip Size: %d", ESP.getFlashChipSize());
    SYSLOG(LOG_INFO, "Chip Speed: %d", ESP.getFlashChipSpeed());
    SYSLOG(LOG_INFO, "Chip Mode: %d", ESP.getFlashChipMode());

    ESPhttpUpdate.rebootOnUpdate(false);
    setup_web_server();
    gsm_init();
    SYSLOG(LOG_INFO, "======End setup======");
    digitalWrite(BUILTIN_LED, HIGH);

    pinMode(PIR_IN, INPUT);
}

Ticker tickerOSWatch;
#define OSWATCH_RESET_TIME 60
static unsigned long oswatch_last_loop_time;
byte oswatch_blocked_loop = 0;

void OsWatchTicker() {
    unsigned long t = millis();
    unsigned long last_run = abs(t - oswatch_last_loop_time);

    if (last_run >= (OSWATCH_RESET_TIME * 1000)) {
        RtcSettings.oswatch_blocked_loop = 1;
        RtcSettingsSave();
        SYSLOG(LOG_INFO, "RESET");
        ESP.reset();  // hard reset
    }
}

void OsWatchInit() {
    oswatch_blocked_loop = RtcSettings.oswatch_blocked_loop;
    RtcSettings.oswatch_blocked_loop = 0;
    oswatch_last_loop_time = millis();
    tickerOSWatch.attach_ms(((OSWATCH_RESET_TIME / 3) * 1000), OsWatchTicker);
}

void OsWatchLoop() {
    oswatch_last_loop_time = millis();
}


#define motionDetected 1
#define motionNotDetected 0
static byte pirLastState = motionNotDetected;

byte getPirState() {
    return pirLastState;
}

void pirLoop() {
    static unsigned long lastStateChanged = 0;

    int pirState = digitalRead(PIR_IN);
    if (pirState != pirLastState) {
        unsigned long now = millis();
        if ((pirState == motionDetected) || ((pirState == motionNotDetected) && (now - lastStateChanged > 30000))) {
            if (pirState != motionDetected) {
                SYSLOG(LOG_INFO, "Nobody here.");
            } else {
                SYSLOG(LOG_INFO, "There's someone here.");
            }
            pirLastState = pirState;
            lastStateChanged = now;
        }
    } else {
        if (pirLastState == motionDetected) {
            lastStateChanged = millis();
        }
    }
}

void send_state() {
    static unsigned long lastStateSended = 0;
    unsigned long now = millis();
    if (((!lastStateSended) || (abs(now - lastStateSended > 300 * 1000))) && (is_gsm_ready())) {
//    tele/wemos1/STATE = {"Time":"2018-02-23T13:10:37","Uptime":75,"Vcc":2.980,"Wifi":{"AP":1,"SSId":"yt201","RSSI":100,"APMac":"1C:AF:F7:2A:22:9E"}}
        lastStateSended = now;
        char buffer[255] = {};
        char stemp1[16];
        dtostrfd((double)ESP.getVcc() / 1000, 3, stemp1);
        snprintf_P(buffer, sizeof(buffer) - 1, PSTR("{\"Time\":\"%s\",\"Uptime\":%d,\"Vcc\":%s%s}"), GetDateAndTime().c_str(), uptime, stemp1, gsmStatusJson().c_str());
        String data("data=");
        data +=  String(buffer) + String("&topic=tele/") + my_hostname + String("/STATE");
        String resp;
        post_data_via_gsm_http("sip.trophy.com.ua", 3000, "/mqtt", data, resp);
        SYSLOG(LOG_INFO, "%s", resp.c_str());
    }
}

void send_sensors() {
    static unsigned long lastSensorSended = 0;
    unsigned long now = millis();
    if (((!lastSensorSended) || abs(now - lastSensorSended > 300 * 1000)) && (is_gsm_ready())) {
//tele/wemos1/SENSOR = {"Time":"2018-02-26T17:22:30","PIR":{"Motion":0}}
        lastSensorSended = now;
        char buffer[255] = {};
        snprintf_P(buffer, sizeof(buffer) - 1, PSTR("{\"Time\":\"%s\",\"PIR\":{\"Motion\":%d}}"), GetDateAndTime().c_str(), getPirState());
        String data("data=");
        data +=  String(buffer) + String("&topic=tele/") + my_hostname + String("/SENSOR");
        String resp;
        post_data_via_gsm_http("sip.trophy.com.ua", 3000, "/mqtt", data, resp);
        SYSLOG(LOG_INFO, "%s", resp.c_str());
    }
}


void loop() {
    pirLoop();
    OsWatchLoop();
    _web_server.handleClient();
    gsmLoop();
    send_state();
    send_sensors();
    yield();
}
