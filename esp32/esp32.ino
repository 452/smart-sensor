/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" esp32-webupdate.local/update
*/
extern "C" int rom_phy_get_vdd33();
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "DHT.h"
#include <NTPClient.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <NetBIOS.h>
#include <math.h>

#define DHTTYPE DHT11

WiFiUDP ntpUDP;
WiFiUDP wifiUDP;
NTPClient timeClient(ntpUDP);

const char* host = "esp32-webupdate";
const char* ssid = "}RooT{";
const char* password = "";
unsigned int localUdpPort = 6930;
uint64_t chipid = ESP.getEfuseMac();
uint16_t chip = (uint16_t)(chipid >> 32);
char deviceName[23];

const int DHTPin = 22;
DHT dht(DHTPin, DHTTYPE);
String celsiusTemp;
String fahrenheitTemp;
String humidityTemp;

const String deviceType = "HiGrowEsp32";
const String firmwareVersion = "201903301731";
const String sensorErrorInfo = "Could not init sensor, check wiring!";
boolean sensorInitialized;

WebServer server(80);

const char* serverIndex = "<form method='POST' action='/update-firmware' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

void handleRoot() {
  server.send(200, "text/plain", getSensorDataXML());
}

void handleJSON() {
  server.send(200, "application/json", getSensorDataJSON());
}

void handleCSV() {
  server.send(200, "text/csv", getSensorDataCSV());
}

String moisure() {
  return String(map(analogRead(32), 3200, 1440, 0, 100));
}

void dthMeashure() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);
  float hic = dht.computeHeatIndex(t, h, false);
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    celsiusTemp = "Failed";
    fahrenheitTemp = "Failed";
    humidityTemp = "Failed";
  } else {
    float hic = dht.computeHeatIndex(t, h, false);
    celsiusTemp = hic;
    float hif = dht.computeHeatIndex(f, h);
    fahrenheitTemp = hif;
    humidityTemp = h;
  }
}

String getSensorDataXML() {
  if (sensorInitialized) {
    dthMeashure();
    return "<sensor>\n<measurements>\n<unixtimestamp>" + String(timeClient.getEpochTime(), DEC)
           + "</unixtimestamp>" + "\n<temperature>"
           + celsiusTemp + "</temperature>" + "\n<moisture>" +
           moisure() + "</moisture>" +
           "\n<humidity>" + humidityTemp + "</humidity>\n</measurements>\n<systemInfo><serialNumber>" +
           String(chip) + "</serialNumber>\n<type>" + deviceType + "</type>\n<rssi>" +
           String(min(max(2 * (WiFi.RSSI() + 100), 0), 100), DEC) + "</rssi>\n<vcc>" +
           String(rom_phy_get_vdd33() / 1024.00f, DEC) + "</vcc>\n<firmwareVersion>" +
           firmwareVersion + "</firmwareVersion>\n</systemInfo></sensor>\n";
  } else {
    return sensorErrorInfo;
  }
}

String getSensorDataJSON() {
  if (sensorInitialized) {
    dthMeashure();
    return "{\"measurements\": {\"unixtimestamp\": " + String(timeClient.getEpochTime(), DEC) +
           ",\"temperature\": " +
           celsiusTemp +
           ",\"moisture\": " + moisure() +
           + ",\"humidity\": " +
           humidityTemp + "},\"systemInfo\": { \"serialNumber\": \"" +
           String(chip) + "\",\"type\": \"" + deviceType + "\", \"rssi\": " +
           String(min(max(2 * (WiFi.RSSI() + 100), 0), 100), DEC) + ",\"vcc\": " +
           String(rom_phy_get_vdd33() / 1024.00f, DEC) +
           ",\"firmwareVersion\": \"" +
           firmwareVersion + "\"}}";
  } else {
    return sensorErrorInfo;
  }
}


String getSensorDataCSV() {
  if (sensorInitialized) {
    dthMeashure();
    return deviceType + ";" + String(timeClient.getEpochTime(), DEC) + ";" +
           celsiusTemp + ";" +
           moisure() + ";" +
           humidityTemp + ";" +
           String(min(max(2 * (WiFi.RSSI() + 100), 0), 100), DEC) + ";" +
           String(rom_phy_get_vdd33() / 1024.00f, DEC) + ";" +
           firmwareVersion + ";" +
           String(chip) + ";\n";
  } else {
    return sensorErrorInfo;
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void) {

  snprintf(deviceName, 23, "smart-sensor-esp32-%04X%08X", chip, (uint32_t)chipid);
  dht.begin();
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting Sketch...");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    MDNS.begin(deviceName);
    server.on("/", handleRoot);
    server.on("/sensor.csv", handleCSV);
    server.on("/sensor.json", handleJSON);
    server.on("/reboot", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access - Control - Allow - Origin", "*");
      server.send(200, "text/html", "reboot ok");
      ESP.restart();
    });

    server.onNotFound(handleNotFound);
    server.on("/update", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });
    server.on("/update-firmware", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin()) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
    });
    waitNTPSync();
    server.begin();
    MDNS.addService("http", "tcp", 80);
    NBNS.begin(deviceName);
    wifiUDP.begin(localUdpPort);
    Serial.printf("Ready! Open http://%s.local in your browser\n", host);
  } else {
    Serial.println("WiFi Failed");
  }
  sensorInitialized = true;
}

void loop(void) {
  timeClient.update();
  server.handleClient();
  handleUDPServer();
}

void handleUDPServer() {
  if (wifiUDP.parsePacket()) {
    udpSend(getSensorDataCSV());
  }
}

void udpSend(String payload) {
  wifiUDP.beginPacket(wifiUDP.remoteIP(), wifiUDP.remotePort());
  //wifiUDP.write(payload.c_str());
  wifiUDP.printf(payload.c_str());
  wifiUDP.endPacket();
}

bool waitNTPSync() {
  timeClient.end();
  timeClient.setUpdateInterval(60 * 60 * 1000);
  timeClient.begin();
  while (timeClient.getEpochTime() < 10000) {
    timeClient.update();
  }
  return true;
}
