#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <SoftwareSerial.h>
#include <Syslog.h>
#include <ArduinoJson.h>
#include <Ticker.h>                         // RTC, HLW8012, OSWatch
#include <ESP8266httpUpdate.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "settings.h"

const char* ssid = "yt201";
const char* password = "zhopa3600";

#define SYSLOG_SERVER "192.168.0.106"
#define SYSLOG_PORT 514

#define DEVICE_HOSTNAME "wemosD1"
#define APP_NAME "sim800"

WiFiUDP udpClient;
Syslog * syslog = NULL;


ESP8266WebServer _web_server(80);


#define TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_ESP8266
#define TINY_GSM_RX_BUFFER 600

SoftwareSerial SerialAT(D8, D7); // RX, TX

const char apn[]  = "internet";
const char user[] = "";
const char pass[] = "";

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

TinyGsm modem(SerialAT);


const char * gsm_command_run(const String& cmd) {
    return "";
}

#define SYSLOG(ident, format, args...) \
{ \
    if (syslog) { \
	syslog->logf(ident, format, ##args); \
    } \
}

int post_data_via_gsm_http(TinyGsm &modem, const char * server, int port, const char * url, const char * data) {

    TinyGsmClient client(modem);
    HttpClient http(client, server, port);

    SYSLOG(LOG_ERR, " Waiting for network...");
    if (!modem.waitForNetwork()) {
        SYSLOG(LOG_ERR, " failed");
        _web_server.send (500, "text/json", "{\"result\":\"network not ready\"}");
        return (-1);
    }
    SYSLOG(LOG_INFO, " ok, network ready");

    SYSLOG(LOG_INFO, "Connecting to %s", apn);
    if (!modem.gprsConnect(apn, user, pass)) {
        SYSLOG(LOG_INFO, " fail");
        _web_server.send (500, "text/json", "{\"result\":\"Unable connect to apn\"}");
        return (-1);
    }
    SYSLOG(LOG_INFO, " apn connected");

    SYSLOG(LOG_INFO, "Performing HTTP POST request... ");
    int err = http.post(url, "application/x-www-form-urlencoded", data);
    if (err != 0) {
        SYSLOG(LOG_INFO, " fail");
        _web_server.send (500, "text/json", "{\"result\":\"Fail get request\"}");
        return(-1);
    }

    int status = http.responseStatusCode();
    SYSLOG(LOG_INFO, "HTTP status: %d", status);
    if (!status) {
        _web_server.send (500, "text/json", "{\"result\":\"Http status 0\"}");
        return(-1);
    }

    while (http.headerAvailable()) {
        String headerName = http.readHeaderName();
        String headerValue = http.readHeaderValue();
        SYSLOG(LOG_INFO, "%s:%s", headerName.c_str(), headerValue.c_str());
    }

    int length = http.contentLength();
    if (length >= 0) {
        SYSLOG(LOG_INFO, "Content length is: %d", length);
    }
    if (http.isResponseChunked()) {
        SYSLOG(LOG_INFO, "This response is chunked");
    }

    String error("");
    String body = http.responseBody();
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(body);


    if (!root.success()) {
        SYSLOG(LOG_ERR, "response parseObject() failed");
    } else {
        if (root.containsKey("status")) {
            status = root.get<int>("status");
        }
        if (root.containsKey("error")) {
            error = ",\"error\":\"" + root.get<String>("error") + "\"";
        }
    }

    SYSLOG(LOG_INFO, "Response: %s", body.c_str());
    SYSLOG(LOG_INFO, "Body length is: %d", body.length());

    http.stop();
    modem.gprsDisconnect();

    SYSLOG(LOG_INFO, "GPRS disconnected");
    String resp = String("{\"result\":") + String(status) +
                  error +
                  String("}");
    _web_server.send (200, "text/json", resp);

    return(status);
}

void setup_web_server() {

    _web_server.on("/gprs", HTTP_POST, [&]() {
        digitalWrite(BUILTIN_LED, HIGH);
        SYSLOG(LOG_INFO, "/gprs %s", _web_server.arg("data").c_str());
        post_data_via_gsm_http(modem, "sip.trophy.com.ua", 3000, "/mqtt", _web_server.arg("data").c_str());
        digitalWrite(BUILTIN_LED, LOW);
    });

    _web_server.on("/gsm", HTTP_POST, [&]() {
        digitalWrite(BUILTIN_LED, HIGH);
        _web_server.send (200, "text/json", "{\"result\":true}");
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

    snprintf_P(my_hostname, sizeof(my_hostname) - 1, "wemos-%d", ESP.getChipId() & 0x1FFF);

    syslog = new Syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, my_hostname, APP_NAME);

    SYSLOG(LOG_INFO, "Begin setup");

    ESPhttpUpdate.rebootOnUpdate(false);

    SYSLOG(LOG_INFO, "Ready");
    SYSLOG(LOG_INFO, "IP address: %s", WiFi.localIP().toString().c_str());
    SYSLOG(LOG_INFO, "Check ID in: https://www.wemos.cc/verify_products");
    SYSLOG(LOG_INFO, "Chip ID = %08X", ESP.getChipId());
    SYSLOG(LOG_INFO, "Buil timestamp: %s", __TIMESTAMP__);
    SYSLOG(LOG_INFO, "Sketch size: %d", ESP.getSketchSize());
    SYSLOG(LOG_INFO, "Free space: %d", ESP.getFreeSketchSpace());
    SYSLOG(LOG_INFO, "Chip ID: %d", ESP.getFlashChipId());
    SYSLOG(LOG_INFO, "Chip Real Size: %d", ESP.getFlashChipRealSize());
    SYSLOG(LOG_INFO, "Chip Size: %d", ESP.getFlashChipSize());
    SYSLOG(LOG_INFO, "Chip Speed: %d", ESP.getFlashChipSpeed());
    SYSLOG(LOG_INFO, "Chip Mode: %d", ESP.getFlashChipMode());

    setup_web_server();


    int rate = 19200;
    if (!rate) {
        SYSLOG(LOG_INFO, "Gsm baud rate detecting.....");
        rate = TinyGsmAutoBaud(SerialAT);
    }

    if (!rate) {
        SYSLOG(LOG_INFO, "***********************************************************");
        SYSLOG(LOG_INFO, " Module does not respond!");
        SYSLOG(LOG_INFO, "   Check your Serial wiring");
        SYSLOG(LOG_INFO, "   Check the module is correctly powered and turned on");
        SYSLOG(LOG_INFO, "***********************************************************");
    } else {
        SerialAT.begin(rate);
        SYSLOG(LOG_INFO, "Initializing modem...");
        modem.restart();

        String modemInfo = modem.getModemInfo();
        SYSLOG(LOG_INFO, "Modem: ");
        SYSLOG(LOG_INFO, "%s", modemInfo.c_str());
    }
    SYSLOG(LOG_INFO, "End setup");
    digitalWrite(BUILTIN_LED, HIGH);
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


void loop() {
    OsWatchLoop();
    _web_server.handleClient();
    yield();
}
