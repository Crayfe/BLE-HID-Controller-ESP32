/*  ESP32_BLE_HID_POC
 *
 *  Prueba de concepto BLE HID (solo teclado) para la placa de
 *  desarrollo personal ESP32-WROOM-32 (by Crayfe)
 *
 *  Librería: Mystfit/ESP32-BLE-CompositeHID
 *    https://github.com/Mystfit/ESP32-BLE-CompositeHID
 *    Dependencias: NimBLE-Arduino, Callback (Tom Stewart)
 *
 *  ── CONTROLES ───────────────────────────────────────────────
 *
 *  El encoder y los pulsadores tienen DOS modos seleccionables
 *  con una pulsación larga en BTN3 (>800 ms):
 *
 *  MODO A – MEDIA (por defecto)
 *    Encoder giro    →  Volumen ↑ / Volumen ↓
 *    Encoder pulsado →  Mute
 *    BTN1            →  Play / Pause
 *    BTN2            →  Pista siguiente
 *    BTN3 corto      →  Pista anterior
 *    BTN3 largo      →  Cambiar a modo SISTEMA
 *
 *  MODO B – SISTEMA
 *    Encoder giro    →  Brillo monitor (F2 / F1)
 *    Encoder pulsado →  Bloquear sesión (Win + L)
 *    BTN1            →  Mostrar escritorio (Win + D)
 *    BTN2            →  Captura de pantalla (Win + Shift + S)
 *    BTN3 corto      →  Abrir explorador de archivos (Win + E)
 *    BTN3 largo      →  Cambiar a modo MEDIA
 *
 *  ── PINES ───────────────────────────────────────────────────
 *    Pulsador 1   →  25      OLED SDA  →  21
 *    Pulsador 2   →  33      OLED SCL  →  22
 *    Pulsador 3   →  32      Buzzer    →  26
 *    Encoder CLK  →  27
 *    Encoder DT   →  14
 *    Encoder SW   →  12
 *
 *  ── NOMBRES DE TECLAS (KeyboardHIDCodes.h) ──────────────────
 *    Tecla Windows  →  KEY_LEFTMETA       (NO KEY_LEFT_GUI)
 *    Shift izq.     →  KEY_LEFTSHIFT      (NO KEY_LEFT_SHIFT)
 *    Ctrl izq.      →  KEY_LEFTCTRL
 *    Alt izq.       →  KEY_LEFTALT
 */


// ============================================================
//  LIBRERÍAS
// ============================================================

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <BleCompositeHID.h>
#include <KeyboardDevice.h>


// ============================================================
//  DISPLAY OLED
// ============================================================

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// ============================================================
//  PINES
// ============================================================

#define BTN1_PIN         25
#define BTN2_PIN         33
#define BTN3_PIN         32

#define CLK_PIN          27
#define DT_PIN           14
#define SW_PIN           12

#define TONE_OUTPUT_PIN  26
#define TONE_PWM_CHANNEL  0


// ============================================================
//  BLE HID
// ============================================================
/*
 *  KeyboardDevice contiene un std::mutex internamente, por lo que
 *  NO admite operador de asignación (operator=).
 *
 *  Solución: usar una función auxiliar que construye y devuelve
 *  la KeyboardConfiguration ya preparada, y pasársela directamente
 *  al constructor global de KeyboardDevice. Así nunca se asigna,
 *  solo se construye una vez con la config correcta.
 */

BleCompositeHID compositeHID("Crayfesp32", "Crayfe", 100);

KeyboardConfiguration makeKbConfig() {
  KeyboardConfiguration cfg;
  cfg.setUseMediaKeys(true);
  return cfg;
}

KeyboardDevice keyboard(makeKbConfig());


// ============================================================
//  MODOS
// ============================================================

enum Mode { MODE_MEDIA, MODE_SYSTEM };
Mode currentMode = MODE_MEDIA;

const char* modeNames[] = { "MEDIA", "SISTEMA" };


// ============================================================
//  VARIABLES AUXILIARES
// ============================================================

int lastClkState;

unsigned long btn3PressTime = 0;
bool          btn3Holding   = false;
bool          btn3LongFired = false;

const unsigned long DEBOUNCE_MS   = 50;
const unsigned long LONG_PRESS_MS = 800;

bool wasConnected = false;


// ============================================================
//  PROTOTIPOS
// ============================================================

void handleButtons();
void handleEncoder();
void showSplash();
void showStatus(String line1, String line2, String line3);
void showAction(String control, String action);
void showModeChange();
void _drawModeBar();
void beep(int freq, int duration);


// ============================================================
//  SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  Serial.println("=== ESP32 BLE HID POC ===");

  // --- Display OLED ---
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[ERROR] SSD1306 no encontrado");
  } else {
    Serial.println("[OK] Display OLED");
  }
  showSplash();

  // --- Pulsadores ---
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  Serial.println("[OK] Pulsadores");

  // --- Encoder ---
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN,  INPUT);
  pinMode(SW_PIN,  INPUT_PULLUP);
  lastClkState = digitalRead(CLK_PIN);
  Serial.println("[OK] Encoder");

  // --- BLE HID ---
  compositeHID.addDevice(&keyboard);
  compositeHID.begin();
  Serial.println("[OK] BLE HID iniciado - esperando conexion...");

  showStatus("BLE HID", "Esperando...", "");
}


// ============================================================
//  LOOP
// ============================================================

void loop() {

  bool connected = compositeHID.isConnected();

  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) {
      Serial.println("[BLE] Conectado");
      beep(1200, 80);
      showStatus("CONECTADO", modeNames[currentMode], "listo");
    } else {
      Serial.println("[BLE] Desconectado");
      showStatus("SIN CONEXION", "", "");
    }
  }

  if (connected) {
    handleButtons();
    handleEncoder();
  }
}


// ============================================================
//  GESTIÓN DE PULSADORES
// ============================================================

void handleButtons() {

  unsigned long now = millis();

  // ── BTN1 ──────────────────────────────────────────────────
  static unsigned long lastBtn1 = 0;
  if (digitalRead(BTN1_PIN) == LOW && (now - lastBtn1) > 200) {
    lastBtn1 = now;
    if (currentMode == MODE_MEDIA) {
      Serial.println("[BTN1] Play/Pause");
      keyboard.mediaKeyPress(KEY_MEDIA_PLAYPAUSE);
      delay(30);
      keyboard.mediaKeyRelease(KEY_MEDIA_PLAYPAUSE);
      showAction("BTN1", "Play / Pause");
    } else {
      Serial.println("[BTN1] Win+D");
      keyboard.keyPress(KEY_LEFTMETA);
      keyboard.keyPress(KEY_D);
      delay(30);
      keyboard.keyRelease(KEY_D);
      keyboard.keyRelease(KEY_LEFTMETA);
      showAction("BTN1", "Escritorio");
    }
    beep(900, 40);
  }

  // ── BTN2 ──────────────────────────────────────────────────
  static unsigned long lastBtn2 = 0;
  if (digitalRead(BTN2_PIN) == LOW && (now - lastBtn2) > 200) {
    lastBtn2 = now;
    if (currentMode == MODE_MEDIA) {
      Serial.println("[BTN2] Siguiente pista");
      keyboard.mediaKeyPress(KEY_MEDIA_NEXTTRACK);
      delay(30);
      keyboard.mediaKeyRelease(KEY_MEDIA_NEXTTRACK);
      showAction("BTN2", "Siguiente");
    } else {
      Serial.println("[BTN2] Win+Shift+S");
      keyboard.keyPress(KEY_LEFTMETA);
      keyboard.keyPress(KEY_LEFTSHIFT);
      keyboard.keyPress(KEY_S);
      delay(30);
      keyboard.keyRelease(KEY_S);
      keyboard.keyRelease(KEY_LEFTSHIFT);
      keyboard.keyRelease(KEY_LEFTMETA);
      showAction("BTN2", "Captura");
    }
    beep(900, 40);
  }

  // ── BTN3 – detección larga/corta ──────────────────────────
  bool btn3State = digitalRead(BTN3_PIN);

  if (btn3State == LOW && !btn3Holding) {
    btn3Holding   = true;
    btn3LongFired = false;
    btn3PressTime = now;
  }

  if (btn3Holding && btn3State == LOW) {
    if (!btn3LongFired && (now - btn3PressTime) >= LONG_PRESS_MS) {
      btn3LongFired = true;
      currentMode = (currentMode == MODE_MEDIA) ? MODE_SYSTEM : MODE_MEDIA;
      Serial.printf("[BTN3 LARGO] Modo -> %s\n", modeNames[currentMode]);
      beep(600, 60);
      delay(60);
      beep(1000, 60);
      showModeChange();
    }
  }

  if (btn3State == HIGH && btn3Holding) {
    btn3Holding = false;
    unsigned long duration = now - btn3PressTime;
    if (!btn3LongFired && duration >= DEBOUNCE_MS) {
      if (currentMode == MODE_MEDIA) {
        Serial.println("[BTN3 CORTO] Pista anterior");
        keyboard.mediaKeyPress(KEY_MEDIA_PREVIOUSTRACK);
        delay(30);
        keyboard.mediaKeyRelease(KEY_MEDIA_PREVIOUSTRACK);
        showAction("BTN3", "Anterior");
      } else {
        Serial.println("[BTN3 CORTO] Win+E");
        keyboard.keyPress(KEY_LEFTMETA);
        keyboard.keyPress(KEY_E);
        delay(30);
        keyboard.keyRelease(KEY_E);
        keyboard.keyRelease(KEY_LEFTMETA);
        showAction("BTN3", "Explorador");
      }
      beep(900, 40);
    }
  }
}


// ============================================================
//  GESTIÓN DEL ENCODER
// ============================================================

void handleEncoder() {

  int clkState = digitalRead(CLK_PIN);

  if (clkState != lastClkState) {
    bool right = (digitalRead(DT_PIN) != clkState);

    if (currentMode == MODE_MEDIA) {
      if (right) {
        Serial.println("[ENC >] Volumen +");
        keyboard.mediaKeyPress(KEY_MEDIA_VOLUMEUP);
        delay(20);
        keyboard.mediaKeyRelease(KEY_MEDIA_VOLUMEUP);
        showAction("ENC >", "Volumen +");
      } else {
        Serial.println("[ENC <] Volumen -");
        keyboard.mediaKeyPress(KEY_MEDIA_VOLUMEDOWN);
        delay(20);
        keyboard.mediaKeyRelease(KEY_MEDIA_VOLUMEDOWN);
        showAction("ENC <", "Volumen -");
      }
    } else {
      if (right) {
        Serial.println("[ENC >] Brillo + (F2)");
        keyboard.keyPress(KEY_F2);
        delay(20);
        keyboard.keyRelease(KEY_F2);
        showAction("ENC >", "Brillo +");
      } else {
        Serial.println("[ENC <] Brillo - (F1)");
        keyboard.keyPress(KEY_F1);
        delay(20);
        keyboard.keyRelease(KEY_F1);
        showAction("ENC <", "Brillo -");
      }
    }
    beep(1100, 25);
  }
  lastClkState = clkState;

  // Pulsación del encoder SW
  static unsigned long lastSW = 0;
  if (digitalRead(SW_PIN) == LOW && (millis() - lastSW) > 300) {
    lastSW = millis();
    if (currentMode == MODE_MEDIA) {
      Serial.println("[ENC SW] Mute");
      keyboard.mediaKeyPress(KEY_MEDIA_MUTE);
      delay(30);
      keyboard.mediaKeyRelease(KEY_MEDIA_MUTE);
      showAction("ENC SW", "Mute");
    } else {
      Serial.println("[ENC SW] Win+L (bloquear)");
      keyboard.keyPress(KEY_LEFTMETA);
      keyboard.keyPress(KEY_L);
      delay(30);
      keyboard.keyRelease(KEY_L);
      keyboard.keyRelease(KEY_LEFTMETA);
      showAction("ENC SW", "Bloquear");
    }
    beep(700, 60);
  }
}


// ============================================================
//  DISPLAY
// ============================================================

void showSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(10, 5);
  display.println("BLE HID");
  display.setTextSize(1);
  display.setCursor(20, 28);
  display.println("by Crayfe");
  display.setCursor(8, 44);
  display.println("MEDIA / SISTEMA");
  display.display();
  delay(1500);
}

void showStatus(String line1, String line2, String line3) {
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
  _drawModeBar();
  display.display();
}

void showAction(String control, String action) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(control);
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.print("> ");
  display.println(action);
  _drawModeBar();
  display.display();
}

void showModeChange() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(20, 10);
  display.println("Modo activado:");
  display.setTextSize(2);
  display.setCursor(10, 28);
  display.println(modeNames[currentMode]);
  display.display();
  delay(800);
  showStatus(wasConnected ? "CONECTADO" : "SIN CONEXION",
             modeNames[currentMode], "");
}

void _drawModeBar() {
  display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(4, 56);
  display.print("Modo: ");
  display.print(modeNames[currentMode]);
}


// ============================================================
//  BUZZER
// ============================================================

void beep(int freq, int duration) {
  ledcAttachPin(TONE_OUTPUT_PIN, TONE_PWM_CHANNEL);
  ledcWriteTone(TONE_PWM_CHANNEL, freq);
  delay(duration);
  ledcWriteTone(TONE_PWM_CHANNEL, 0);
  ledcDetachPin(TONE_OUTPUT_PIN);
}
