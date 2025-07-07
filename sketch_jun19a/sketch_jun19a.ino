#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include "DHT.h"

const char *ssid = "wifi name";
const char *password = "wifi password";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  dht.begin();
}

void sendFormData(float temperature, int moisture)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to get time");
    return;
  }

  char datetime[25];
  strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", &timeinfo);

  int binID = 1;

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String payload = "";
  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"temperature\"\r\n\r\n";
  payload += String(temperature) + "\r\n";
  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"moisture\"\r\n\r\n";
  payload += String(moisture) + "\r\n";
  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"binID\"\r\n\r\n";
  payload += String(binID) + "\r\n";
  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"dateRecorded\"\r\n\r\n";
  payload += String(datetime) + "\r\n";
  payload += "--" + boundary + "--\r\n";

  HTTPClient http;
  WiFiClientSecure client;

  client.setInsecure();
  http.begin(client, "https://team-21.vercel.app/api");
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  int httpCode = http.POST((uint8_t *)payload.c_str(), payload.length());

  if (httpCode > 0)
  {
    String response = http.getString();
    Serial.println("Data sent successfully:");
    Serial.println(response);
  }
  else
  {
    Serial.print("POST failed. HTTP code: ");
    Serial.println(httpCode);
  }

  http.end();
}

void loop()
{
  float temp = dht.readTemperature();
  float moisture = dht.readHumidity();

  if (isnan(temp) || isnan(moisture))
  {
    Serial.println("Failed to read from DHT sensor!");
    delay(10000);
    return;
  }

  Serial.printf("Temp: %.2f °C | Humidity: %.2f%%\n", temp, moisture);
  sendFormData(temp, (int)moisture);

  delay(60000);
}