#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "credentials.h"

int analogPin = A0;
int digitalPin = 5;
float counterMultiplier = 0.01;


ESP8266WebServer server(80);
WiFiClient espClient;

float voltage = 0; 
int digitalVal = 0;
int counter = 0;
String str = "";

int lastDigitalVal = 0;

void setup_wifi() {
  delay(100);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  delay(2000);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

String getMetrics() {
  String str = "";
  str += "gas_meter_voltage=" + String(voltage) + "\n";
  str += "gas_meter_value=" + String(digitalVal) + "\n";
  str += "gas_meter_sum=" + String(counter * counterMultiplier) + "\n";
  
  return str;
}

String getHTML() {
  String str = "<!DOCTYPE html>\n<html>\n<head>\n  <title>Gas Meter</title>\n</head>\n<body>\n";
  str += "<h1>Gas Meter</h1>\n"; 
  str += "<pre>" + getMetrics() + "</pre>\n";
  str += "<script>setTimeout(() => location.reload(), 1000)</script>\n";
  str += "</body>\n</html>\n";
  return str;
}

void setup() {
  Serial.begin(9600);
  pinMode(analogPin, INPUT);
  pinMode(digitalPin, INPUT);

  setup_wifi();

  server.on("/metrics", []() {
    server.send(200, "text/plain", getMetrics()); 
  });
  server.on("/", []() {
    server.send(200, "text/html", getHTML());
  });
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
}

void loop() {
  server.handleClient();

  float voltage = analogRead (analogPin) * (5.0 / 1023.0); 
  int digitalVal = digitalRead (digitalPin);

  if (digitalVal != lastDigitalVal) {
    lastDigitalVal = digitalVal;

    if (digitalVal == 1) {
      Serial.print("analog: " + String(voltage, 4) + "V, ");
      Serial.println("counter: " + String(++counter * counterMultiplier));
    }
  }

  delay(50);
}
