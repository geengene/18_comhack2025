#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

const char *ssid = "connec";
const char *password = "manydevices";

// NTP time config for SG time
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

// ds18b20 temp sensor
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// LCD pins
#define TFT_CS 16    // GPIO16 (D0)
#define TFT_RST 12   // GPIO12 (D6)
#define TFT_DC 15    // GPIO15 (D8)
#define TFT_MOSI 13  // GPIO13 (D7)
#define TFT_SCLK 14  // GPIO14 (D5)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);
  delay(1000);
  sensors.begin();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  tft.init(240, 240);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Initializing...");
}

void sendFormData(float temperature, int moisture)  // this is function for post request to api
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
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

  http.begin(client, "api route");
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  int httpCode = http.POST((uint8_t *)payload.c_str(), payload.length());

  if (httpCode > 0) {
    String response = http.getString();
    Serial.println("Data sent successfully:");
    Serial.println(response);
  } else {
    Serial.print("POST failed. HTTP code: ");
    Serial.println(httpCode);
  }

  http.end();
}

void loop() {
  int moistureRaw = analogRead(A0);
  // convert to percentage (adjust based on calibration)
  const int moisture_dry = 1024;
  const int moisture_wet = 0;
  int moisture = map(moistureRaw, moisture_dry, moisture_wet, 0, 100);
  moisture = constrain(moisture, 0, 100);

  // read temperature
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);

  Serial.print("Soil Moisture: ");
  Serial.print(moisture);
  Serial.print("% | Temperature: ");
  if (temp == DEVICE_DISCONNECTED_C) {
    Serial.println("Read error");
  } else {
    Serial.print(temp);
    Serial.println(" °C");
  }

  // lcd display
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextSize(3);
  tft.setCursor(10, 20);
  tft.print("Humidity:");
  tft.setCursor(10, 60);
  tft.setTextSize(3);
  tft.print(moisture);
  tft.print(" %");

  // progress bar background
  int barX = 10;
  int barY = 100;
  int barWidth = 220;
  int barHeight = 20;
  tft.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);

  // fill width based on percentage
  int fillWidth = map(moisture, 0, 100, 0, barWidth);
  tft.fillRect(barX + 1, barY + 1, fillWidth - 2, barHeight - 2, ST77XX_GREEN);

  tft.setTextSize(3);
  tft.setCursor(10, 140);
  tft.print("Temperature:");
  tft.setTextSize(3);
  tft.setCursor(10, 180);
  if (temp == DEVICE_DISCONNECTED_C) {
    tft.print("Read Error");
    delay(5000);
    return;
  } else {
    tft.print(temp);
    tft.print(" C");
  }

  if (isnan(temp) || isnan(moisture)) {
    Serial.println("Failed to read from sensors!");
    delay(10000);
    return;
  }

  sendFormData(temp, moisture);


  delay(5000);  // refreshes and sends form data every 5 seconds
}
