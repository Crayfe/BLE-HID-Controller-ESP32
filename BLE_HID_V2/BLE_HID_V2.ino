/*  ESP32_BLE_HID_SD.ino
 *
 *  BLE HID keyboard controller for ESP32-WROOM-32
 *  Modes and key mappings are loaded from .txt files on the SD card.
 *
 *  Hardware (personal dev board by Crayfe):
 *    Button 1     →  pin 25      OLED SDA  →  pin 21
 *    Button 2     →  pin 33      OLED SCL  →  pin 22
 *    Button 3     →  pin 32      Buzzer    →  pin 26
 *    Encoder CLK  →  pin 27      SD CS     →  pin 5
 *    Encoder DT   →  pin 14
 *    Encoder SW   →  pin 12
 *
 *  Library: Mystfit/ESP32-BLE-CompositeHID
 *    https://github.com/Mystfit/ESP32-BLE-CompositeHID
 *    Dependencies: NimBLE-Arduino, Callback (Tom Stewart)
 *
 *  SD card structure:
 *    /modes/
 *      media.txt
 *      system.txt
 *      ...
 *
 *  See ConfigLoader.h for full config file format documentation.
 */


// ============================================================
//  LIBRARIES
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <SD.h>
#include <vector>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>

#include "ConfigLoader.h"


// ============================================================
//  DISPLAY
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// ============================================================
//  PINS
// ============================================================

#define BTN1_PIN 25
#define BTN2_PIN 33
#define BTN3_PIN 32

#define ENC_CLK_PIN 27
#define ENC_DT_PIN 14
#define ENC_SW_PIN 12

#define BUZZER_PIN 26
#define BUZZER_PWM_CH 0

#define SD_CS_PIN 16


// ============================================================
//  BLE HID
// ============================================================

BleCompositeHID compositeHID("Crayfesp32", "Crayfe", 100);

KeyboardConfiguration makeKbConfig() {
  KeyboardConfiguration cfg;
  cfg.setUseMediaKeys(true);
  return cfg;
}

KeyboardDevice keyboard(makeKbConfig());


// ============================================================
//  MODES
// ============================================================

std::vector<Mode> modes;
int currentModeIndex = 0;

// Returns the active Mode object (or a safe empty default)
Mode& activeMode() {
  static Mode emptyMode;
  if (modes.empty()) return emptyMode;
  return modes[currentModeIndex];
}


// ============================================================
//  INPUT STATE
// ============================================================

// Encoder
int encoderLastClk;

// Button 3 long-press detection
unsigned long btn3PressTime = 0;
bool btn3Holding = false;
bool btn3LongFired = false;

const unsigned long BTN_DEBOUNCE_MS = 50;
const unsigned long BTN_LONG_PRESS_MS = 800;

// BLE connection tracking
bool wasConnected = false;


// ============================================================
//  FUNCTION PROTOTYPES
// ============================================================

void handleButtons();
void handleEncoder();
void triggerAction(const Action& action, const String& controlLabel);
void nextMode();

void drawSplash();
void drawStatus(const String& line1, const String& line2, const String& line3);
void drawAction(const String& control, const String& action);
void drawModeChange();
void drawError(const String& msg);
void drawModeBar();

void beep(int freq, int durationMs);
void beepOk();
void beepAction();
void beepModeChange();
void beepError();


// ============================================================
//  SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  Serial.println("============================");
  Serial.println("  ESP32 BLE HID - SD Modes");
  Serial.println("============================");

  // --- OLED display ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[ERROR] OLED SSD1306 not found");
  } else {
    Serial.println("[OK] OLED display");
  }
  drawSplash();

  // --- Buttons ---
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  Serial.println("[OK] Buttons");

  // --- Rotary encoder ---
  pinMode(ENC_CLK_PIN, INPUT);
  pinMode(ENC_DT_PIN, INPUT);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);
  encoderLastClk = digitalRead(ENC_CLK_PIN);
  Serial.println("[OK] Encoder");

  // --- SD card ---
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[ERROR] SD card not found or failed to mount");
    beepError();
    drawError("SD card error");
    delay(2000);
  } else {
    Serial.println("[OK] SD card mounted");

    // Load all mode files from /modes/
    if (!loadAllModes(modes)) {
      Serial.println("[ERROR] No modes loaded from SD");
      beepError();
      drawError("No modes on SD");
      delay(2000);
    } else {
      Serial.printf("[OK] %d mode(s) loaded\n", (int)modes.size());
      for (int i = 0; i < (int)modes.size(); i++) {
        Serial.printf("     [%d] %s\n", i, modes[i].name.c_str());
      }
    }
  }

  // --- BLE HID ---
  compositeHID.addDevice(&keyboard);
  compositeHID.begin();
  Serial.println("[OK] BLE HID started - waiting for connection...");

  drawStatus("BLE HID", "Waiting...",
             modes.empty() ? "No modes!" : activeMode().name);
}


// ============================================================
//  LOOP
// ============================================================

void loop() {

  bool connected = compositeHID.isConnected();

  // Detect BLE connection state changes
  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) {
      Serial.println("[BLE] Connected");
      beepOk();
      drawStatus("Connected", activeMode().name, "Ready");
    } else {
      Serial.println("[BLE] Disconnected");
      drawStatus("Disconnected", "", "");
    }
  }

  if (connected && !modes.empty()) {
    handleButtons();
    handleEncoder();
  }
}


// ============================================================
//  BUTTON HANDLING
// ============================================================

void handleButtons() {

  unsigned long now = millis();

  // ── Button 1 ── simple debounced press ────────────────────
  static unsigned long lastBtn1 = 0;
  if (digitalRead(BTN1_PIN) == LOW && (now - lastBtn1) > 200) {
    lastBtn1 = now;
    Serial.println("[BTN1] Pressed");
    triggerAction(activeMode().btn1, "BTN1");
  }

  // ── Button 2 ── simple debounced press ────────────────────
  static unsigned long lastBtn2 = 0;
  if (digitalRead(BTN2_PIN) == LOW && (now - lastBtn2) > 200) {
    lastBtn2 = now;
    Serial.println("[BTN2] Pressed");
    triggerAction(activeMode().btn2, "BTN2");
  }

  // ── Button 3 ── short press / long press ──────────────────
  bool btn3State = digitalRead(BTN3_PIN);

  // Falling edge: button pressed down
  if (btn3State == LOW && !btn3Holding) {
    btn3Holding = true;
    btn3LongFired = false;
    btn3PressTime = now;
  }

  // While held: detect long press threshold
  if (btn3Holding && btn3State == LOW) {
    if (!btn3LongFired && (now - btn3PressTime) >= BTN_LONG_PRESS_MS) {
      btn3LongFired = true;
      Serial.println("[BTN3] Long press");
      triggerAction(activeMode().btn3Long, "BTN3 LONG");
    }
  }

  // Rising edge: button released
  if (btn3State == HIGH && btn3Holding) {
    btn3Holding = false;
    unsigned long duration = now - btn3PressTime;
    if (!btn3LongFired && duration >= BTN_DEBOUNCE_MS) {
      Serial.println("[BTN3] Short press");
      triggerAction(activeMode().btn3Short, "BTN3");
    }
  }
}


// ============================================================
//  ENCODER HANDLING
// ============================================================

void handleEncoder() {

  int clkState = digitalRead(ENC_CLK_PIN);

  if (clkState != encoderLastClk) {
    bool clockwise = (digitalRead(ENC_DT_PIN) != clkState);

    if (clockwise) {
      Serial.println("[ENC] Clockwise");
      triggerAction(activeMode().encCW, "ENC >");
    } else {
      Serial.println("[ENC] Counter-clockwise");
      triggerAction(activeMode().encCCW, "ENC <");
    }
  }
  encoderLastClk = clkState;

  // Encoder switch (push button)
  static unsigned long lastEncSW = 0;
  if (digitalRead(ENC_SW_PIN) == LOW && (millis() - lastEncSW) > 300) {
    lastEncSW = millis();
    Serial.println("[ENC SW] Pressed");
    triggerAction(activeMode().encSW, "ENC SW");
  }
}


// ============================================================
//  ACTION DISPATCHER
// ============================================================

void triggerAction(const Action& action, const String& controlLabel) {

  if (action.type == ACTION_NONE) {
    Serial.printf("[%s] No action assigned\n", controlLabel.c_str());
    return;
  }

  if (action.type == ACTION_NEXT_MODE) {
    nextMode();
    return;
  }

  Serial.printf("[%s] Action: %s\n", controlLabel.c_str(), action.label.c_str());
  drawAction(controlLabel, action.label);
  beepAction();
  executeAction(action, keyboard);
}

// Cycle to the next available mode
void nextMode() {
  if (modes.size() <= 1) return;
  currentModeIndex = (currentModeIndex + 1) % modes.size();
  Serial.printf("[MODE] Switched to: %s (%d/%d)\n",
                activeMode().name.c_str(),
                currentModeIndex + 1,
                (int)modes.size());
  beepModeChange();
  drawModeChange();
}


// ============================================================
//  DISPLAY FUNCTIONS
// ============================================================

// Startup splash screen
void drawSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 6);
  display.println("BLE  HID");
  display.setTextSize(1);
  display.setCursor(22, 28);
  display.println("by  Crayfe");
  display.setCursor(10, 44);
  display.println("SD mode loader");
  display.display();
  delay(1500);
}

// General status screen with mode bar at the bottom
void drawStatus(const String& line1, const String& line2, const String& line3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(line1);
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.println(line2);
  display.setCursor(0, 32);
  display.println(line3);
  drawModeBar();
  display.display();
}

// Action feedback screen
void drawAction(const String& control, const String& action) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(control);
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.print("> ");
  display.println(action);
  drawModeBar();
  display.display();
}

// Mode change screen (inverted colors)
void drawModeChange() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(18, 8);
  display.printf("Mode %d / %d", currentModeIndex + 1, (int)modes.size());
  display.setTextSize(2);
  display.setCursor(4, 26);
  // Truncate name if too long for 2x font (max ~10 chars at size 2)
  String name = activeMode().name;
  if (name.length() > 10) name = name.substring(0, 10);
  display.println(name);
  display.display();
  delay(900);
  drawStatus("Connected", activeMode().name, "Ready");
}

// Error screen
void drawError(const String& msg) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("!! ERROR !!");
  display.setCursor(0, 16);
  display.println(msg);
  display.display();
}

// Bottom bar showing current mode name and index
void drawModeBar() {
  display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 56);
  if (modes.empty()) {
    display.print("No modes loaded");
  } else {
    display.printf("[%d/%d] %s",
                   currentModeIndex + 1,
                   (int)modes.size(),
                   activeMode().name.c_str());
  }
}


// ============================================================
//  BUZZER
// ============================================================

void beep(int freq, int durationMs) {
  ledcAttachPin(BUZZER_PIN, BUZZER_PWM_CH);
  ledcWriteTone(BUZZER_PWM_CH, freq);
  delay(durationMs);
  ledcWriteTone(BUZZER_PWM_CH, 0);
  ledcDetachPin(BUZZER_PIN);
}

// BLE connected / mode loaded successfully
void beepOk() {
  beep(1200, 80);
}

// Normal action feedback (short, unobtrusive)
void beepAction() {
  beep(1000, 25);
}

// Mode change: two-tone sweept
void beepModeChange() {
  beep(700, 55);
  delay(40);
  beep(1100, 55);
}

// Error: low descending tone
void beepError() {
  beep(400, 150);
  delay(60);
  beep(300, 200);
}
