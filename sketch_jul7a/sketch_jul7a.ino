#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <ArduinoJson.h>


const char *ssid = "";
const char *password = "";

// NTP SG time
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;
const int sensorID = 125;
// int binID = -1;

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

  tft.init(240, 240);
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println("Sensor ID: " + String(sensorID));

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // checkAndRegisterSensor();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void sendFormData(float temperature, int moisture) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    return;
  }

  char datetime[25];
  strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", &timeinfo);

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String payload = "";

  // Construct multipart/form-data payload
  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"temperature\"\r\n\r\n";
  payload += String(temperature) + "\r\n";

  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"moisture\"\r\n\r\n";
  payload += String(moisture) + "\r\n";

  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"dateRecorded\"\r\n\r\n";
  payload += String(datetime) + "\r\n";

  payload += "--" + boundary + "\r\n";
  payload += "Content-Disposition: form-data; name=\"sensorID\"\r\n\r\n";
  payload += String(sensorID) + "\r\n";

  payload += "--" + boundary + "--\r\n";

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  http.begin(client, "https://team-21.vercel.app/api/monitor");
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

  int httpCode = http.POST((uint8_t *)payload.c_str(), payload.length());
  String response = http.getString();

  if (httpCode == 200) {
    Serial.println("Data sent successfully:");
    Serial.println(response);
    Serial.printf("HTTP code: %d\n", httpCode);
  } else if (httpCode == 404) {
    Serial.println("Sensor is not registered");
    Serial.println(response);
    Serial.printf("HTTP code: %d\n", httpCode);
  } else if (httpCode == 400) {
    Serial.println("Invalid values submitted.");
    Serial.println(response);
    Serial.printf("HTTP code: %d\n", httpCode);
  } else if (httpCode == 409) {
    Serial.println("Sensor is not paired with a bin.");
    Serial.println(response);
    Serial.printf("HTTP code: %d\n", httpCode);
  } else {
    Serial.println("POST failed.");
    Serial.printf("HTTP code: %d\n", httpCode);
  }

  http.end();
}





void loop() {
  int moistureRaw = analogRead(A0);
  // convert to percentage
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
    delay(5000);
    return;
  }

  sendFormData(temp, moisture);


  delay(5000);  // refreshes and sends form data every 5 seconds
}


// bool registerSensorID() {
//   String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
//   String payload = "";
//   payload += "--" + boundary + "\r\n";
//   payload += "Content-Disposition: form-data; name=\"sensorID\"\r\n\r\n";
//   payload += String(sensorID) + "\r\n";
//   payload += "--" + boundary + "--\r\n";

//   HTTPClient http;
//   WiFiClientSecure client;
//   client.setInsecure();


//   http.begin(client, "https://team-21.vercel.app/api/pair");
//   http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

//   int httpCode = http.POST((uint8_t *)payload.c_str(), payload.length());

//   if (httpCode > 0 && httpCode < 400) {
//     String response = http.getString();
//     Serial.println("Sensor registration successful:");
//     Serial.println(response);
//     http.end();
//     return true;
//   } else {
//     Serial.print("Sensor registration failed. HTTP code: ");
//     Serial.println(httpCode);
//     http.end();
//     return false;
//   }
// }

// JsonVariant getSensorStatus(int sensorID) {
//   HTTPClient http;
//   WiFiClientSecure client;
//   client.setInsecure();


//   const int retryDelay = 10000;

//   while (true) {
//     String url = "https://team-21.vercel.app/api/pair?sensorID=" + String(sensorID);
//     http.begin(client, url);
//     int httpCode = http.GET();

//     if (httpCode == 200) {
//       String response = http.getString();
//       Serial.print("GET response: ");
//       Serial.println(response);

//       DynamicJsonDocument doc(256);
//       DeserializationError error = deserializeJson(doc, response);
//       http.end();

//       if (error) {
//         Serial.print("JSON parse error: ");
//         Serial.println(error.c_str());
//         delay(retryDelay);
//         continue;
//       }

//       return doc["binID"];
//     }

//     else if (httpCode == 404) {
//       Serial.println("Sensor not registered (404).");
//       http.end();
//       return JsonVariant();
//     }

//     else {
//       Serial.print("GET request failed. HTTP code: ");
//       Serial.println(httpCode);
//       http.end();
//       Serial.println("Retrying GET request in 10s...");
//       delay(retryDelay);
//     }
//   }
// }

// check if its paired to a bin(should have binID ) or if its even registered or bin id is null
// if paired to a bin, skip registration, go straight to monitor route
// if bin id is null, then print "not paired"
// if not registered(404), then registerSensorID, then monitor

// bool checkAndRegisterSensor() {
//   JsonVariant binIDValue = getSensorStatus(sensorID);

//   if (binIDValue.isNull()) {
//     Serial.println("Sensor is either unpaired or unregistered.");

//     bool registered = false;

//     while (!registered) {
//       registered = registerSensorID();

//       if (!registered) {
//         Serial.println("Registration failed. Retrying in 5s...");
//         delay(5000);
//       }
//     }
//     Serial.println("Sensor registered successfully. Please pair it to a bin.");
//     return true;
//   } else {
//     binID = binIDValue.as<int>();
//     Serial.print("Sensor is already paired. binID: ");
//     Serial.println(binID);
//     return true;
//   }
// }
