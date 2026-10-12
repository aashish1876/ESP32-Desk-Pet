
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <time.h>

// Configure these privately before building.
const char* WIFI_SSID = "Aashish_5G";
const char* WIFI_PASSWORD = "Aashish@1318";
const char* WEATHER_KEY = "bd5e378503939ddaee76f12ad7a97608";

const char* CITY = "Kunchanapalli";
const char* CITY_QUERY = "Kunchanapalli,IN";

// ESP32 DEVKIT V1 GPIO numbers
#define SDA_PIN 21
#define SCL_PIN 22
#define PIR_PIN 27
#define SERVO_PIN 18
#define GREEN_LED 25
#define RED_LED 26
#define BUZZER_PIN 23
#define BUTTON_PIN 14

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo;

byte eyeL[8] = {
  7, 15, 31, 27, 27, 31, 15, 7
};
byte eyeR[8] = {
  28, 30, 31, 27, 27, 31, 30, 28
};
byte closedEye[8] = {
  0, 0, 31, 31, 0, 0, 0, 0
};

bool infoMode = false;
bool blink = false;
bool weatherOK = false;
bool lastButton = HIGH;

unsigned long infoStarted = 0;
unsigned long lastBlink = 0;
unsigned long lastMotion = 0;
unsigned long lastWeather = 0;
unsigned long lastScroll = 0;
unsigned long lastButtonChange = 0;

String conditionText = "Weather unavailable";
String weatherText = "Add API key";
int tempC = 0;
int humidity = 0;
int scrollPos = 0;

String pad(String s, int n) {
  if (s.length() > n) s = s.substring(0, n);
  while (s.length() < n) s += ' ';
  return s;
}

String scroll(String s, int offset, int width) {
  s = " " + s + "   ";
  String out = "";
  for (int i = 0; i < width; i++)
    out += s[(offset + i) % s.length()];
  return out;
}

void eyes() {
  lcd.clear();

  if (blink) {
    lcd.setCursor(3, 0);
    lcd.write(byte(2));
    lcd.write(byte(2));
    lcd.write(byte(2));
    lcd.write(byte(2));
    lcd.setCursor(10, 0);
    lcd.write(byte(2));
    lcd.write(byte(2));
    lcd.write(byte(2));
    lcd.write(byte(2));
    return;
  }

  lcd.setCursor(3, 0);
  lcd.write(byte(0));
  lcd.write(byte(1));
  lcd.setCursor(3, 1);
  lcd.write(byte(0));
  lcd.write(byte(1));

  lcd.setCursor(10, 0);
  lcd.write(byte(0));
  lcd.write(byte(1));
  lcd.setCursor(10, 1);
  lcd.write(byte(0));
  lcd.write(byte(1));
}

String clockText() {
  struct tm t;
  if (!getLocalTime(&t, 20)) return "--:--";
  char b[12];
  strftime(b, sizeof(b), "%H:%M %a", &t);
  return String(b);
}

void showInfo() {
  lcd.setCursor(0, 0);
  lcd.print(pad(clockText(), 7));
  lcd.print(scroll(CITY, scrollPos, 9));

  String details = weatherOK
      ? conditionText + " " + String(tempC) + "C H" +
        String(humidity) + "%"
      : weatherText;

  lcd.setCursor(0, 1);
  lcd.print("WEATHER");
  lcd.print(scroll(details, scrollPos, 9));
}

void getWeather() {
  lastWeather = millis();

  if (WiFi.status() != WL_CONNECTED) {
    weatherOK = false;
    weatherText = "WiFi disconnected";
    return;
  }

  if (String(WEATHER_KEY).length() < 10 ||
      String(WEATHER_KEY) == "YOUR_OPENWEATHERMAP_API_KEY") {
    weatherOK = false;
    weatherText = "Add weather API key";
    return;
  }

  WiFiClientSecure client;
  // Prototype only; this skips HTTPS certificate validation.
  client.setInsecure();

  HTTPClient http;
  String url =
    "https://api.openweathermap.org/data/2.5/weather?q=" +
    String(CITY_QUERY) + "&appid=" + String(WEATHER_KEY) +
    "&units=metric";

  http.setTimeout(5000);

  if (!http.begin(client, url)) {
    weatherOK = false;
    weatherText = "Connection error";
    return;
  }

  int status = http.GET();

  if (status == 200) {
    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, http.getString());

    if (!err) {
      tempC = doc["main"]["temp"] | 0;
      humidity = doc["main"]["humidity"] | 0;
      conditionText = doc["weather"][0]["main"] | "Unknown";
      weatherOK = true;
      weatherText = conditionText + " " + String(tempC) + "C";
    } else {
      weatherOK = false;
      weatherText = "JSON error";
    }
  } else {
    weatherOK = false;
    weatherText = "HTTP " + String(status);
  }

  http.end();
}

void happy() {
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  servo.write(65);
  tone(BUZZER_PIN, 1400, 100);
  delay(150);

  servo.write(115);
  tone(BUZZER_PIN, 1800, 100);
  delay(150);

  servo.write(90);
  digitalWrite(GREEN_LED, LOW);
}

void enterInfo() {
  infoMode = true;
  infoStarted = millis();
  scrollPos = 0;
  showInfo();
  getWeather();
  showInfo();
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, eyeL);
  lcd.createChar(1, eyeR);
  lcd.createChar(2, closedEye);

  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(90);

  lcd.setCursor(0, 0);
  lcd.print("DESK PET");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(800);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 12000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    configTime(19800, 0, "pool.ntp.org", "time.google.com");
  }

  eyes();
  lastBlink = millis();
  lastMotion = millis();
}

void loop() {
  unsigned long now = millis();

  // Debounce button.
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButton) {
    lastButton = reading;
    lastButtonChange = now;
  }

  static bool stableButton = HIGH;
  if (now - lastButtonChange > 40 &&
      stableButton != reading) {
    stableButton = reading;
    if (stableButton == LOW) {
      if (!infoMode) enterInfo();
      else {
        infoMode = false;
        eyes();
      }
    }
  }

  if (infoMode) {
    if (now - infoStarted >= 600000UL) {
      infoMode = false;
      eyes();
    } else {
      if (now - lastWeather >= 600000UL) {
        getWeather();
        showInfo();
      }
      if (now - lastScroll >= 450) {
        lastScroll = now;
        scrollPos++;
        showInfo();
      }
      delay(5);
      return;
    }
  }

  if (digitalRead(PIR_PIN) == HIGH &&
      now - lastMotion > 2000) {
    lastMotion = now;
    happy();
    eyes();
  }

  if (!blink && now - lastBlink > 3500) {
    blink = true;
    eyes();
    lastBlink = now;
  } else if (blink && now - lastBlink > 180) {
    blink = false;
    eyes();
    lastBlink = now;
  }

  delay(10);
}
