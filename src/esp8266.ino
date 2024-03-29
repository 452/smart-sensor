/*
   Articles for next update
   http://www.esp8266.com/viewtopic.php?f=29&t=4209
   //Adafruit_BME280 sensor(CS, MOSI, MISO, SCK);
   //DHT dht(D3, DHT11);
   //#include <DHT.h>/
   #include <SPI.h>
*/
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <ESP8266NetBIOS.h>

//#include <MQTT.h>

#include <ArduinoJson.h>
#include <CronAlarms.h>
#include <ESP8266HTTPClient.h>

#define DEBUG

#define CS   D4
#define CLK  D5
#define MISO D6
#define MOSI D7
#define SENSOR_ADDRESS 0x76

// const char* ssid = "myssid";
// const char* wifiPassword = "mypass";

//const char* mqttUserName = "5ba8acec";
//const char* mqttPassword = "";
//const char* mqttURL = "broker.shiftr.io";
//const char* topic = "bme280.rec";
const char* serialNumber = "1";
const char* firmwareVersion = "202302191708";
String deviceName = ("smart-sensor-bme280-" + String(ESP.getChipId(), DEC));
String name = ("smart" + String(ESP.getChipId(), DEC));

unsigned int localUdpPort = 6930;
const char* sensorErrorInfo = "Could not find a valid BME280 sensor, check wiring!";
boolean sensorInitialized;
long lastMsg = 0;
//int mqttConnectRetrysAllowed = 5;
//int mqttConnectRetrys;

WiFiUDP ntpUDP;
WiFiUDP wifiUDP;
NTPClient timeClient(ntpUDP);
WiFiClientSecure espClient;
HTTPClient https;
//MQTTClient client;
Adafruit_BME280 sensor;
MDNSResponder mdns;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void handleRoot() {
  server.send(200, "text/plain", getSensorDataXML());
}

void handleJSON() {
  server.send(200, "application/json", getSensorDataJSON());
}

void handleCSV() {
  server.send(200, "text/csv", getSensorDataCSV());
}

String getSensorDataXML() {
  if (sensorInitialized) {
    return "<sensor>\n<measurements>\n<unixtimestamp>" + String(timeClient.getEpochTime(), DEC) + "</unixtimestamp>" + "\n<temperature>" + String(sensor.readTemperature(), 1) + "</temperature>" + "\n<pressure>" + String(sensor.readPressure() / 100, 1) + "</pressure>" + "\n<altitude>" + String(sensor.readAltitude(1013.25), 1) + "</altitude>" + "\n<humidity>" + String(sensor.readHumidity(), 1) + "</humidity>\n</measurements>\n<systemInfo>\n<type>BME280</type>\n<rssi>" + String(WiFi.RSSI(), DEC) + "</rssi>\n<vcc>" + String(ESP.getVcc() / 1024.00f, DEC) + "</vcc>\n<serialNumber>" + serialNumber + "</serialNumber>\n<firmwareVersion>" + firmwareVersion + "</firmwareVersion>\n<chipId>" + ESP.getChipId() + "</chipId>\n<flashChipId>" + ESP.getFlashChipId() + "</flashChipId>\n<macAddress>" + WiFi.macAddress() + "</macAddress></systemInfo></sensor>\n";
  } else {
    return sensorErrorInfo;
  }
}

String getSensorDataJSON() {
  if (sensorInitialized) {
    const size_t capacity = JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(5) + JSON_OBJECT_SIZE(8);
    DynamicJsonDocument doc(capacity);
    
    JsonObject measurements = doc.createNestedObject("measurements");
    measurements["unixtimestamp"] = timeClient.getEpochTime();
    measurements["temperature"] = sensor.readTemperature();
    measurements["pressure"] = sensor.readPressure() / 100;
    measurements["altitude"] = sensor.readAltitude(1013.25);
    measurements["humidity"] = sensor.readHumidity();
    
    JsonObject systemInfo = doc.createNestedObject("systemInfo");
    systemInfo["type"] = "BME280";
    systemInfo["rssi"] = WiFi.RSSI();
    systemInfo["vcc"] = ESP.getVcc() / 1024.00f;
    systemInfo["serialNumber"] = serialNumber;
    systemInfo["firmwareVersion"] = firmwareVersion;
    systemInfo["chipId"] = ESP.getChipId();
    systemInfo["flashChipId"] = ESP.getFlashChipId();
    systemInfo["macAddress"] = WiFi.macAddress();
    String output;
    serializeJson(doc, output);
    return output;
  } else {
    return sensorErrorInfo;
  }
}


String getSensorDataCSV() {
  if (sensorInitialized) {
    return "BME280;" + String(timeClient.getEpochTime(), DEC) + ";" + String(sensor.readTemperature(), 1) + ";" + String(sensor.readPressure() / 100, 1) + ";" + String(sensor.readAltitude(1013.25), 1) + ";" + String(sensor.readHumidity(), 1) + ";" + String(WiFi.RSSI(), DEC) + ";" + String(ESP.getVcc() / 1024.00f, DEC) + ";" + serialNumber + ";" + firmwareVersion + ";" + ESP.getChipId() + ";" + ESP.getFlashChipId() + ";" + WiFi.macAddress() + ";\n";
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

void messageReceived(String &topic, String &payload) {
#ifdef DEBUG
  Serial.println("incoming: " + topic + " - " + payload);
#endif
}

void setup() {
  // ADC_MODE(ADC_VCC);
#ifdef DEBUG
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("Booting");
  Serial.print("WIFI scan start ... ");
  int n = WiFi.scanNetworks();
  Serial.print(n);
  Serial.println(" network(s) found");
  for (int i = 0; i < n; i++) {
    Serial.println(WiFi.SSID(i));
  }
  Serial.println();
#endif
//  WiFi.persistent(false);
//  WiFi.disconnect(true);
//  WiFi.setAutoConnect(false);
//  WiFi.softAPdisconnect(true);
  WiFi.hostname(deviceName);
  WiFi.mode(WIFI_STA);
//  WiFi.setOutputPower(30);
//  WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
#ifdef DEBUG
    Serial.println("Begin connect to WIFI " + String(wifissid));
#endif
    WiFi.begin(wifissid, wifiPassword);
#ifdef DEBUG
    WiFi.printDiag(Serial);
    Serial.println("Retrying connection... ssid: '" + String(wifissid) + "' password: '" + wifiPassword + "'");
#endif
    delay(10000);
  }
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
#ifdef DEBUG
    Serial.println("Connected to WIFI " + String(wifissid));
    //Serial.println("IP address: " + WiFi.localIP());
#endif
  if (mdns.begin(deviceName.c_str(), WiFi.localIP())) {
#ifdef DEBUG
    Serial.println("MDNS responder started");
#endif
  }
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
  ArduinoOTA.setHostname(deviceName.c_str());
  httpUpdater.setup(&server);
  server.begin();
  if (sensor.begin(SENSOR_ADDRESS)) {
    sensorInitialized = true;
  } else {
#ifdef DEBUG
    Serial.println(sensorErrorInfo);
#endif
  }
  waitNTPSync();
  NBNS.begin(deviceName.c_str());
  wifiUDP.begin(localUdpPort);
  espClient.setInsecure();
//  client.begin(mqttURL, 8883, espClient);
//  client.onMessage(messageReceived);
  Cron.create("0 */3 * * * *", sendToBackend, false);
  Cron.create("0 0 */3 * * *", sendToTelegram, false);
  #ifdef DEBUG
    Serial.println("Smart sensor Ready");
  #endif
}

void sendToBackend() {
  #ifdef DEBUG
    Serial.println("sendToBackend");
  #endif
  String url = "";
  const size_t capacity = JSON_OBJECT_SIZE(96);//JSON_OBJECT_SIZE(12);
  DynamicJsonDocument doc(capacity);
  doc["chipId"] = ESP.getChipId();
  doc["flashChipId"] = ESP.getFlashChipId();
  doc["macAddress"] = String(WiFi.macAddress());
  doc["temperature"] = sensor.readTemperature();
  doc["rssi"] = String(WiFi.RSSI(), DEC);
  doc["vcc"] = ESP.getVcc() / 1024.00f;
  doc["type"] = "BME280";
  doc["serialNumber"] = serialNumber;
  doc["firmwareVersion"] = firmwareVersion;
  doc["pressure"] = sensor.readPressure() / 100;
  doc["altitude"] = sensor.readAltitude(1013.25);
  doc["humidity"] = sensor.readHumidity();
  doc["unixtimestamp"] = timeClient.getEpochTime();
//  deviceName
  String output;
  serializeJson(doc, output);
  https.begin(espClient, url);
  https.addHeader("Content-Type", "application/json");
  int httpCode = https.POST(output);
//  String payload = https.getString();
//  Serial.println(httpCode);   //Print HTTP return code
//  Serial.println(payload);
//  https.writeToStream(&Serial);
  https.end();
}

void sendToTelegram() {
  String url = "https://api.telegram.org/&text=Метеорологічні метрики:%0A" +
  String("назва: " + name + "%0A") +
  String("температура: " + String(sensor.readTemperature(), 1) + " °C%0A") +
  String("тиск: " + String(sensor.readPressure() / 100) + " hPa%0A") +
  String("вологість повітря: " + String(sensor.readHumidity()) + "%%0A") +
  String("висота над рівнем моря: " + String(sensor.readAltitude(1013.25)));
  https.begin(espClient, url);
  https.GET();
  https.end();
}

//void connect() {
//#ifdef DEBUG
//  Serial.print("MQTT connecting...");
//#endif
//  int retrys;
//  while (!client.connect(deviceName.c_str(), mqttUserName, mqttPassword)) {
//    delay(1000);
//#ifdef DEBUG
//    Serial.print(".");
//#endif
//    if (retrys++ >= 3) {
//      delay(3000);
//      break;
//    }
//  }
//#ifdef DEBUG
//  Serial.println("\nconnected!");
//#endif
//
//  client.subscribe("bm280S", 1);
//}

void loop() {
//  if (mqttConnectRetrys++ <= mqttConnectRetrysAllowed && !client.connected()) {
//    connect();
//  }
  timeClient.update();
//  client.loop();
  ArduinoOTA.handle();
  server.handleClient();
//  sendToQueue();
  handleUDPServer();
  Cron.delay();
  delay(10);
}

void sendToQueue() {
  long now = millis();
  if (now - lastMsg > 1 * (1000 * 60)) {
    lastMsg = now;
    const size_t capacity = JSON_OBJECT_SIZE(5);
    DynamicJsonDocument doc(capacity);

    doc["chipId"] = ESP.getChipId();
    doc["temperature"] = sensor.readTemperature();
    doc["pressure"] = sensor.readPressure() / 100;
    doc["altitude"] = sensor.readAltitude(1013.25);
    doc["humidity"] = sensor.readHumidity();
    String output;
    serializeJson(doc, output);
//    if (client.publish(topic, output, 2, true)) {
//#ifdef DEBUG
//      Serial.println("\ndelivered mqtt message!");
//#endif
//    } else {
//#ifdef DEBUG
//      Serial.println("\nnot delivered mqtt message!");
//#endif
//    }
  }
}

void handleUDPServer() {
  if (wifiUDP.parsePacket()) {
    udpSend(getSensorDataCSV());
  }
}

void udpSend(String payload) {
  wifiUDP.beginPacket(wifiUDP.remoteIP(), wifiUDP.remotePort());
  wifiUDP.write(payload.c_str());
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
