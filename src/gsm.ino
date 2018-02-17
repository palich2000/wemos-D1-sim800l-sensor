#include "support.h"

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

static bool _is_gsm_ready = false;

bool is_gsm_ready() {
    return(_is_gsm_ready);
}

void command_to_modem(const String& command, String& res) {
    modem.sendAT(command);
    if (modem.waitResponse(1000L, res) != 1) {
        res = "";
    }
    res.replace(GSM_NL "OK" GSM_NL, "");
    res.replace(GSM_NL, " ");
    res.trim();

}

int gsm_sync_time() {

    if (!is_gsm_ready()) {
        return(-1);
    }

    if (!modem.waitForNetwork()) {
        SYSLOG(LOG_ERR, " waitForNetwork fail");
        return (-1);
    }

    if (!modem.gprsConnect(apn, user, pass)) {
        SYSLOG(LOG_ERR, "gprsConnect fail");
        return (-1);
    }

    String gsmLoc = modem.getGsmLocation();
    SYSLOG(LOG_INFO, "loc:%s", gsmLoc.c_str());
    modem.gprsDisconnect();
    if (!gsmLoc.length()) {
        return (-1);
    }
    gsmLoc = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());
    gsmLoc = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());
    gsmLoc = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());

    String dt = gsmLoc.substring(0, gsmLoc.indexOf(","));
    String tm = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());

    SYSLOG(LOG_INFO, "DT:%s TM:%s", dt.c_str(), tm.c_str());

    if ((dt.length() != 10) || (tm.length() != 8)) {
        return (-1);
    }
    // DT:2018/02/16 TM:15:02:42
    int year = dt.substring(0, 4).toInt();
    int month = dt.substring(5, 7).toInt();
    int day_of_month = dt.substring(8, 10).toInt();
    int hour = tm.substring(0, 3).toInt();
    int minute = tm.substring(3, 5).toInt();
    int second = tm.substring(6, 8).toInt();

    //SYSLOG(LOG_INFO,"%d %d %d %d %d %d",year, month, day_of_month, hour, minute,second);
    TIME_T tmpTime;
    tmpTime.year = year - 1970;
    tmpTime.month = month;
    tmpTime.day_of_month = day_of_month;
    tmpTime.hour = hour;
    tmpTime.minute = minute;
    tmpTime.second = second;
    SetUTCTime(MakeTime(tmpTime));
    SYSLOG(LOG_INFO, "Sync date and time: %s", GetUtcDateAndTime().c_str());
    return (0);
}

int post_data_via_gsm_http(const char * server, int port, const char * url, const char * data) {

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
        SYSLOG(LOG_ERR, " fail");
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


int gsm_init() {
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
        _is_gsm_ready = modem.restart();
        String modemInfo = modem.getModemInfo();
        SYSLOG(LOG_INFO, "Modem: ");
        SYSLOG(LOG_INFO, "%s", modemInfo.c_str());
        //gsm_sync_time();
    }
    return 0;
}

void gsmLoop() {
    if (is_need_sync_time()) {
        if (!gsm_sync_time()) {
            sync_time_done();
            SYSLOG(LOG_INFO, "UTC: %s", GetUtcDateAndTime().c_str());
        }
    }
}
