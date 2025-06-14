#include "env.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

#define RST_PIN D3
#define SS_PIN  D8
MFRC522 mfrc522(SS_PIN, RST_PIN);

#define GREEN_LED_PIN D4
#define RED_LED_PIN D0

const unsigned long displayDuration = 3000;
unsigned long messageShownAt = 0;
bool messageVisible = false;
const String VALIDATOR_SECRET = SECRET_KEY;
enum LedState { NONE, GREEN, RED };
LedState currentLedState = NONE;

void displayMessage(const String& line1, const String& line2 = "") {
  oled.clearBuffer();
  oled.setFont(u8g2_font_ncenB08_tr);
  oled.drawStr(0, 20, line1.c_str());
  if (!line2.isEmpty()) oled.drawStr(0, 40, line2.c_str());
  oled.sendBuffer();
  messageShownAt = millis();
  messageVisible = true;
}

void connectToWiFi() {
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  if (!wifiManager.autoConnect("Entryflow Wi-Fi Setup")) {
    delay(3000);
    ESP.restart();
  }
}

String validateUID(const String& uid) {
  HTTPClient http;
  WiFiClient client;
  http.begin(client, "http://192.168.40.117:5085/Access/validate");
  http.addHeader("Content-Type", "application/json");
  String payload = "{\"uid\":\"" + uid + "\",\"validatorSecret\":\"" + VALIDATOR_SECRET + "\"}";
  int httpCode = http.POST(payload);
  http.getString();
  http.end();
  if (httpCode == 200) {
    return uid;
  } else {
    return "";
  }
}

void setLed(LedState state) {
  if (state == GREEN) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
  } else if (state == RED) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
  }
  currentLedState = state;
}

void checkRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  Serial.println("Card UID: " + uid);

  String grantedUid = validateUID(uid);

  if (!grantedUid.isEmpty()) {
    displayMessage("Access", "Granted!");
    setLed(GREEN);
  } else {
    displayMessage("Access", "Denied!");
    setLed(RED);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void handleDisplayTimeout() {
  if (messageVisible && millis() - messageShownAt >= displayDuration) {
    displayMessage("Waiting", "for card...");
    setLed(NONE);
    messageVisible = false;
  }
}

void setup() {
  Serial.begin(115200);
  oled.begin();

  SPI.begin();
  mfrc522.PCD_Init();

  displayMessage("Connecting...");
  connectToWiFi();

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  setLed(NONE);

  displayMessage("Waiting", "for card...");
}

void loop() {
  checkRFID();
  handleDisplayTimeout();
}
