
#include <Arduino.h>
#include <WiFi.h>
#include "wifi_secrets.h"


void setup()
{

    Serial.begin(115200);
    delay(2000);

    Serial.println("Starting ESP32...");
    int networks = WiFi.scanNetworks();

Serial.println(networks);
for (int i = 0; i < networks; i++) {
  Serial.println(WiFi.SSID(i));
}
    WiFi.begin(ssid, password);

    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startTime < 10000)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("WiFi connection timeout!");
    }
}
void loop()
{

}