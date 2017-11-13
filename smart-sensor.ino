/*
   Articles for next update
   http://www.esp8266.com/viewtopic.php?f=29&t=4209
*/
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

//#define DEBUG

#define CS   D4
#define CLK  D5
#define MISO D6
#define MOSI D7

//Adafruit_BMP280 bme();
Adafruit_BMP280 bme(CS, MOSI, MISO, SCK);

DHT dht(D3, DHT11);

const char* ssid = "}RooT{";
const char* password = "";
const char* serialNumber = "1";
const char* firmwareVersion = "201711140730";
const char* host = "smart-sensor";
const char* bmp280ErrorInfo = "Could not find a valid BMP280 sensor, check wiring!";
boolean bmp280Initialized;

MDNSResponder mdns;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void handleRoot() {
  server.send(200, "text/plain", getSensorDataXML());
}

String getSensorDataXML() {
  if (bmp280Initialized) {
    return "<sensor>\n<measurements>\n<temperatureBMP280>" + String(bme.readTemperature(), 1) + "</temperatureBMP280>" + "\n<pressureBMP280>" + String(bme.readPressure(), 1) + "</pressureBMP280>" + "\n<altitudeBMP280>" + String(bme.readAltitude(1013.25), 1) + "</altitudeBMP280>" + "\n<humidityDTH11>" + String(dht.readHumidity(), 1) + "</humidityDTH11>" + "\n<temperatureDTH11>" + String(dht.readTemperature(), 1) + "</temperatureDTH11>\n</measurements>\n<systemInfo>\n<rssi>" + String(WiFi.RSSI(), DEC) + "</rssi>\n<vcc>" + String(ESP.getVcc() / 1024.00f, DEC) + "</vcc>\n<serialNumber>" + serialNumber + "</serialNumber>\n<firmwareVersion>" + firmwareVersion + "</firmwareVersion>\n<chipId>" + ESP.getChipId() + "</chipId>\n<flashChipId>" + ESP.getFlashChipId() + "</flashChipId>\n<macAddress>" + WiFi.macAddress() + "</macAddress></systemInfo></sensor>";
  } else {
    return bmp280ErrorInfo;
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

void setup() {
  //  ADC_MODE(ADC_VCC);
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("Booting");
#endif
  WiFi.mode(WIFI_STA);
  WiFi.hostname(host);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
#ifdef DEBUG
    Serial.println("Retrying connection...");
#endif
  }

  if (mdns.begin(host, WiFi.localIP())) {
#ifdef DEBUG
    Serial.println("MDNS responder started");
#endif
  }
  server.on("/", handleRoot);
  server.on("/reboot", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", "reboot ok");
    ESP.restart();
  });

  server.onNotFound(handleNotFound);
  ArduinoOTA.setHostname(host);
  httpUpdater.setup(&server);
  server.begin();
  if (bme.begin()) {//0x76
    bmp280Initialized = true;
  } else {
#ifdef DEBUG
    Serial.println(bmp280ErrorInfo);
#endif
  }
#ifdef DEBUG
  Serial.println("Smart sensor Ready");
#endif

}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}
