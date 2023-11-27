#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <MQTT.h>
#include <OneWire.h>
#include <DallasTemperature.h>            

#include "credentials.h"

#define ANALOG_PIN A0
#define DIGITAL_PIN 5
#define KY001_Signal_PIN D2
float COUNTER_MULTIPLIER = 0.01;
int LOOP_WAIT = 50;       // Time to wait in ms in each loop()
int MQTT_INTERVAL = 1000; // Interval in ms to send mqtt messages

ESP8266WebServer server(80);
WiFiClient wifiClient;
MQTTClient mqttClient;
OneWire oneWire(KY001_Signal_PIN);
DallasTemperature sensors(&oneWire);

float voltage = 0;
int digitalVal = 0;
int counter = 0;

int lastDigitalVal = 0;
unsigned long lastMillis = millis();

float temperature = 0;
float lastTemp = 0;
int wifiFails = -1;
int mqttFails = -1;

void assertWifiIsConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiFails++;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi connected");
    delay(2000);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setupWifi() {
  delay(100);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifiSsid);

  WiFi.begin(wifiSsid, wifiPassword);
  assertWifiIsConnected();
}

void assertMqttIsConnected() {
  if (!mqttClient.connected()) {
    mqttFails++;
    while (!mqttClient.connected()) {
      if (mqttClient.connect(mqttServer, mqttUser, mqttPassword)) {
        Serial.println("\nMQTT connected.");
      } else {
        Serial.println("MQTT failed, code=" + String(mqttClient.lastError()) + ". Try again in 3sec.");
        delay(3000);
      }
    }
  }
}

void setupMqtt() {
  Serial.print("Connecting to MQTT Server " + String(mqttServer));
  mqttClient.begin(mqttServer, 1883, wifiClient);
  assertMqttIsConnected();
}

void setupTemperatureSensor() {
  sensors.begin();
}

bool isSensorConnected() {
  DeviceAddress deviceAddress;
  if (!sensors.getAddress(deviceAddress, 0)) {
    return false;
  }
  return sensors.isConnected(deviceAddress);
}

String getMetrics() {
  String str = "";
  str += "gas_meter_voltage=" + String(voltage) + "\n";
  str += "gas_meter_value=" + String(digitalVal) + "\n";
  str += "gas_meter_counter=" + String(counter * COUNTER_MULTIPLIER) + "\n";
  str += "temperature_value=" + String(temperature) + "\n";
  str += "wifi_fails=" + String(wifiFails) + "\n";
  str += "mqtt_fails=" + String(mqttFails) + "\n";

  return str;
}

String getHTML() {
  String str = "<!DOCTYPE html>\n<html>\n<head>\n  <title>Gas Meter</title>\n</head>\n<body>\n";
  str += "<h1>Gas Meter</h1>\n";
  str += "<pre>" + getMetrics() + "\n";
  str += "Temp. sensor connected: " + String(isSensorConnected()) + "\n";
  str += "MQTT connected: " + String(mqttClient.connected()) + "\n";
  str += "MQTT last error: " + String(mqttClient.lastError()) + "\n";
  str += "</pre>\n";
  str += "<script>setTimeout(() => location.reload(), 1000)</script>\n";
  str += "</body>\n</html>\n";
  return str;
}

void setup() {
  Serial.begin(9600);
  pinMode(ANALOG_PIN, INPUT);
  pinMode(DIGITAL_PIN, INPUT);
  pinMode(KY001_Signal_PIN, INPUT);

  setupWifi();
  setupMqtt();
  setupTemperatureSensor();

  server.on("/metrics", []() { server.send(200, "text/plain", getMetrics()); });
  server.on("/", []() { server.send(200, "text/html", getHTML()); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void loop() {
  server.handleClient();

  voltage = analogRead(ANALOG_PIN) * (5.0 / 1023.0);
  digitalVal = digitalRead(DIGITAL_PIN);

  if (digitalVal != lastDigitalVal) {
    lastDigitalVal = digitalVal;

    if (digitalVal == 1) {
      Serial.print("analog: " + String(voltage, 4) + "V, ");
      Serial.println("counter: " + String(++counter * COUNTER_MULTIPLIER));
    }
  }

  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);

  if (abs(temperature - lastTemp) > 0.1) {
    lastTemp = temperature;
    Serial.println("temperature: " + String(temperature, 1) + "°C");
  }

  if (millis() - lastMillis > 1000) {
    lastMillis = millis();
    assertWifiIsConnected();
    assertMqttIsConnected();
    String payload = "{\"voltage\":" + String(voltage, 4)
      + ",\"counter\":" + String(counter)
      + ",\"temperature\":" + String(temperature, 1)
      + ",\"wifiFails\":" + String(wifiFails)
      + ",\"mqttFails\":" + String(mqttFails)
      + "}";
    mqttClient.publish("tele/gas_meter/SENSOR", payload);
  }

  delay(LOOP_WAIT);
}
