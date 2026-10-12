
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <time.h>

// ===== WIFI & WEATHER =====
// Enter credentials locally. Do not publish real credentials.
const char* WIFI_SSID = "Airtel_matt_5165";
const char* WIFI_PASSWORD = "Air@88073";
const char* WEATHER_KEY = "bd5e378503939ddaee76f12ad7a97608";

const char* CITY = "Kunchanapalli";
const char* CITY_QUERY = "Kunchanapalli,IN";

// ===== ESP32 DEVKIT V1 GPIO =====
#define SDA_PIN     21
#define SCL_PIN     22
#define PIR_PIN     27
#define SERVO_PIN   18
#define GREEN_LED   25
#define RED_LED     26
#define BUZZER_PIN  23
#define BUTTON_PIN  14

// Change to 0x3F only if your LCD uses that address.
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo;

// ===== CUSTOM EYE CHARACTERS =====
byte eyeLeft[8] = {
  B00111, B01111, B11111, B11011,
  B11011, B11111, B01111, B00111
};

byte eyeRight[8] = {
  B11100, B11110, B11111, B11011,
  B11011, B11111, B11110, B11100
};

byte closedEye[8] = {
  B00000, B00000, B11111, B11111,
  B00000, B00000, B00000, B00000
};

// ===== STATE =====
bool infoMode = false;
bool blink = false;
bool weatherOK = false;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long infoStarted = 0;
unsigned long lastBlink = 0;
unsigned long lastMotion = 0;
unsigned long lastWeatherAttempt = 0;
unsigned long lastScroll = 0;
unsigned long lastButtonChange = 0;

const unsigned long INFO_DURATION = 600000UL;
const unsigned long WEATHER_INTERVAL = 600000UL;

String conditionText = "Unknown";
String weatherText = "Weather unavailable";

int tempC = 0;
int humidity = 0;
int scrollPos = 0;

// ===== TEXT HELPERS =====
String padText(String text, int width) {
  if (text.length() > width) {
    text = text.substring(0, width);
  }

  while (text.length() < width) {
    text += ' ';
  }

  return text;
}

String scrollText(String text, int offset, int width) {
  text = " " + text + "   ";

  String result = "";

  for (int i = 0; i < width; i++) {
    result += text[(offset + i) % text.length()];
  }

  return result;
}

// ===== LCD EYES =====
void drawEyes() {
  lcd.clear();

  if (blink) {
    for (int x : {3, 4, 10, 11}) {
      lcd.setCursor(x, 0);
      lcd.write(byte(2));
    }
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

// ===== CLOCK =====
String getClockText() {
  struct tm timeInfo;

  if (!getLocalTime(&timeInfo, 20)) {
    return "--:--";
  }

  char buffer[12];
  strftime(buffer, sizeof(buffer), "%H:%M %a", &timeInfo);

  return String(buffer);
}

// ===== WEATHER =====
void fetchWeather() {
  lastWeatherAttempt = millis();

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

  // Prototype only: disables HTTPS certificate verification.
  // Validate the server certificate for a production device.
  client.setInsecure();

  HTTPClient http;

  String url =
    "https://api.openweathermap.org/data/2.5/weather?q=" +
    String(CITY_QUERY) +
    "&appid=" + String(WEATHER_KEY) +
    "&units=metric";

  http.setTimeout(5000);

  if (!http.begin(client, url)) {
    weatherOK = false;
    weatherText = "HTTP setup failed";
    return;
  }

  int status = http.GET();

  if (status == HTTP_CODE_OK) {
    JsonDocument doc;

    DeserializationError error =
      deserializeJson(doc, http.getString());

    if (!error) {
      tempC = doc["main"]["temp"] | 0;
      humidity = doc["main"]["humidity"] | 0;

      conditionText =
        doc["weather"][0]["main"] | "Unknown";

      weatherText =
        conditionText + " " + String(tempC) + "C";

      weatherOK = true;
    } else {
      weatherOK = false;
      weatherText = "JSON parse error";
    }
  } else {
    weatherOK = false;
    weatherText = "Weather HTTP " + String(status);
  }

  http.end();
}

// ===== INFORMATION SCREEN =====
void drawInfoScreen() {
  String clock = getClockText();

  lcd.setCursor(0, 0);
  lcd.print(padText(clock, 7));
  lcd.print(scrollText(CITY, scrollPos, 9));

  String details;

  if (weatherOK) {
    details = conditionText + " " +
              String(tempC) + "C H" +
              String(humidity) + "%";
  } else {
    details = weatherText;
  }

  lcd.setCursor(0, 1);
  lcd.print("WEATHER");
  lcd.print(scrollText(details, scrollPos, 9));
}

// ===== SERVO REACTION =====
void happyReaction() {
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

void enterInfoMode() {
  infoMode = true;
  infoStarted = millis();
  scrollPos = 0;

  drawInfoScreen();
  fetchWeather();
  drawInfoScreen();
}

// ===== BUTTON DEBOUNCE =====
void checkButton() {
  bool reading = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if (reading != lastButtonReading) {
    lastButtonChange = now;
    lastButtonReading = reading;
  }

  if (now - lastButtonChange > 40 &&
      reading != stableButtonState) {

    stableButtonState = reading;

    if (stableButtonState == LOW) {
      if (!infoMode) {
        enterInfoMode();
      } else {
        infoMode = false;
        drawEyes();
      }
    }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // Initialize I2C and LCD.
  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();

  lcd.createChar(0, eyeLeft);
  lcd.createChar(1, eyeRight);
  lcd.createChar(2, closedEye);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DESK PET");
  lcd.setCursor(0, 1);
  lcd.print("LCD initialized");
  delay(1200);

  // Initialize servo.
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(90);

  // Show Wi-Fi diagnostic information.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long wifiStart = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - wifiStart < 12000UL) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");

    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());

    configTime(
      19800, 0,
      "pool.ntp.org",
      "time.google.com"
    );
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    lcd.setCursor(0, 1);
    lcd.print("Eyes still work");

    Serial.println("WiFi connection failed");
  }

  delay(1500);

  // Explicit diagnostic before entering normal operation.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WIFI CHECK DONE");
  lcd.setCursor(0, 1);
  lcd.print(
    WiFi.status() == WL_CONNECTED
      ? "WiFi Connected"
      : "WiFi Failed"
  );

  delay(2000);

  blink = false;
  drawEyes();

  lastBlink = millis();
  lastMotion = millis();
}

// ===== MAIN LOOP =====
void loop() {
  unsigned long now = millis();

  checkButton();

  if (infoMode) {
    if (now - infoStarted >= INFO_DURATION) {
      infoMode = false;
      drawEyes();
    } else {
      if (now - lastWeatherAttempt >= WEATHER_INTERVAL) {
        fetchWeather();
        drawInfoScreen();
      }

      if (now - lastScroll >= 450) {
        lastScroll = now;
        scrollPos++;
        drawInfoScreen();
      }

      delay(5);
      return;
    }
  }

  // Motion detection.
  if (digitalRead(PIR_PIN) == HIGH &&
      now - lastMotion > 2000) {
    lastMotion = now;

    happyReaction();
    drawEyes();
  }

  // Automatic blinking.
  if (!blink && now - lastBlink >= 3500) {
    blink = true;
    drawEyes();
    lastBlink = now;
  } else if (blink &&
             now - lastBlink >= 180) {
    blink = false;
    drawEyes();
    lastBlink = now;
  }

  delay(10);
}
