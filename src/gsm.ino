#include "support.h"

#define TINY_GSM_MODEM_SIM800
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
/*
int qs() {
    modem.sendAT(GF("+CSQ"));
    if (modem.waitResponse(GF(GSM_NL "+CSQ:")) != 1) {
	return 99;
    }

    int res = modem.stream.readStringUntil(',').toInt();
    return(res);
}
*/

#define GSM_OK 0
#define GSM_NOT_INITIALIZED -1
#define GSM_NET_NOT_READY -2
#define GSM_CONNECT_TO_APN_FAILED -3
#define GSM_POST_FAIL -4
#define GSM_NO_HTTP_STATUS -5
#define GSM_NO_GSM_LOCATION -6
#define GSM_GSM_LOCATION_PARSE_ERROR -7

int gsm_sync_time() {

    if (!is_gsm_ready()) {
        return(GSM_NOT_INITIALIZED);
    }

    //SYSLOG(LOG_INFO, "%s", __FUNCTION__);
    if (!modem.waitForNetwork()) {
        SYSLOG(LOG_ERR, "waitForNetwork fail.");
        return (GSM_NET_NOT_READY);
    }

    if (!modem.gprsConnect(apn, user, pass)) {
        SYSLOG(LOG_ERR, "gprsConnect fail.");
        return (GSM_CONNECT_TO_APN_FAILED);
    }

    String gsmLoc = modem.getGsmLocation();
    SYSLOG(LOG_INFO, "loc:%s", gsmLoc.c_str());

    modem.gprsDisconnect();

    if (!gsmLoc.length()) {
        SYSLOG(LOG_ERR, "gsmLoc.length is zero.");
        return (GSM_NO_GSM_LOCATION);
    }

    gsmLoc = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());
    gsmLoc = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());
    gsmLoc = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());

    String dt = gsmLoc.substring(0, gsmLoc.indexOf(","));
    String tm = gsmLoc.substring(gsmLoc.indexOf(",") + 1, gsmLoc.length());

    //SYSLOG(LOG_INFO, "DT:%s TM:%s", dt.c_str(), tm.c_str());

    if ((dt.length() != 10) || (tm.length() != 8)) {
        SYSLOG(LOG_ERR, "Gsm locarion parse error.");
        return (GSM_GSM_LOCATION_PARSE_ERROR);
    }
    //DT:2018/02/16 TM:15:02:42

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

    //SYSLOG(LOG_INFO, "Sync date and time: %s", GetUtcDateAndTime().c_str());
    return (GSM_OK);
}


int post_data_via_gsm_http(const char * server, int port, const char * url, const String &data, String &resp) {

    if (!is_gsm_ready()) {
        return(GSM_NOT_INITIALIZED);
    }

    TinyGsmClient client(modem);
    HttpClient http(client, server, port);

    if (!modem.waitForNetwork()) {
        return (GSM_NET_NOT_READY);
    }

    if (!modem.gprsConnect(apn, user, pass)) {
        return (GSM_CONNECT_TO_APN_FAILED);
    }

    int err = http.post(url, "application/x-www-form-urlencoded", data);
    if (err != 0) {
        modem.gprsDisconnect();
        return(GSM_POST_FAIL);
    }

    int status = http.responseStatusCode();
    if (!status) {
        modem.gprsDisconnect();
        return(GSM_NO_HTTP_STATUS);
    }

    /*
        while (http.headerAvailable()) {
            String headerName = http.readHeaderName();
            String headerValue = http.readHeaderValue();
            SYSLOG(LOG_INFO, "%s:%s", headerName.c_str(), headerValue.c_str());
        }
    */
    int length = http.contentLength();
    if (length >= 0) {
//        SYSLOG(LOG_INFO, "Content length is: %d", length);
    }
    if (http.isResponseChunked()) {
//        SYSLOG(LOG_INFO, "This response is chunked");
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

    //SYSLOG(LOG_INFO, "Response: %s", body.c_str());
    //SYSLOG(LOG_INFO, "Body length is: %d", body.length());

    http.stop();
    modem.gprsDisconnect();

    resp = String("{\"result\":") + String(status) +
           error +
           String("}");

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
        if (modemInfo.length()) {
            SYSLOG(LOG_INFO, "Modem: %s", modemInfo.c_str());
        } else {
            SYSLOG(LOG_INFO, "Modem: not found");
        }
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

String gsmStatusJson() {
    String gsm_info;
    if (is_gsm_ready()) {
        char buf[20];
        dtostrfd(modem.getBattVoltage() / 1000.0, 4, buf);
        gsm_info = String(",\"gsm\":{\"SignalQuality\":") + modem.getSignalQuality() + String(", \"Operator\": \"") + modem.getOperator() + String("\", \"Vcc\":" + String(buf) + "}");
    }
    return gsm_info;
}