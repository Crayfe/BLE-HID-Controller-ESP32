#pragma once

/*  ConfigLoader.h
 *
 *  Handles loading and parsing of mode configuration files from the SD card.
 *
 *  SD folder structure:
 *    /modes/
 *      media.txt
 *      system.txt
 *      mymode.txt
 *      ...
 *
 *  Config file format:
 *    # Lines starting with # are comments and are ignored
 *    NAME=Media
 *
 *    BTN1=MEDIA_PLAYPAUSE
 *    BTN2=MEDIA_NEXTTRACK
 *    BTN3_SHORT=MEDIA_PREVIOUSTRACK
 *    BTN3_LONG=NEXT_MODE
 *    ENC_CW=MEDIA_VOLUMEUP
 *    ENC_CCW=MEDIA_VOLUMEDOWN
 *    ENC_SW=MEDIA_MUTE
 *
 *  Macro format (value starts with MACRO:):
 *    BTN1=MACRO:LEFTMETA+D
 *    BTN2=MACRO:LEFTCTRL+C,DELAY:150,LEFTCTRL+V
 *    BTN3_SHORT=TEXT:Hello world!
 *
 *  Macro step syntax:
 *    KEY1+KEY2+KEY3     simultaneous key combo
 *    DELAY:ms           wait N milliseconds
 *    TEXT:string        type a literal string character by character
 *
 *  Key names follow KeyboardHIDCodes.h from Mystfit/ESP32-BLE-CompositeHID:
 *    LEFTCTRL, LEFTSHIFT, LEFTALT, LEFTMETA (Win key)
 *    RIGHTCTRL, RIGHTSHIFT, RIGHTALT, RIGHTMETA
 *    A-Z, 0-9, F1-F12
 *    ENTER, ESC, BACKSPACE, TAB, SPACE, DELETE
 *    UP, DOWN, LEFT, RIGHT, HOME, END, PAGEUP, PAGEDOWN
 *    INSERT, CAPSLOCK, NUMLOCK, SCROLLLOCK, SYSRQ (PrintScreen)
 *
 *  Special action values (not macros):
 *    NEXT_MODE          cycle to the next loaded mode
 *    MEDIA_PLAYPAUSE, MEDIA_NEXTTRACK, MEDIA_PREVIOUSTRACK
 *    MEDIA_VOLUMEUP, MEDIA_VOLUMEDOWN, MEDIA_MUTE
 *    MEDIA_STOP, MEDIA_FASTFORWARD, MEDIA_REWIND
 *    MEDIA_PLAY, MEDIA_PAUSE
 */

#include <SD.h>
#include <KeyboardDevice.h>
#include <vector>

// ============================================================
//  ACTION TYPES
// ============================================================

enum ActionType {
  ACTION_NONE,
  ACTION_KEY,        // single key or combo (non-media)
  ACTION_MEDIA,      // media key
  ACTION_MACRO,      // sequence of steps
  ACTION_TEXT,       // type a literal string
  ACTION_NEXT_MODE   // cycle to next mode
};

// ============================================================
//  MACRO STEP
// ============================================================

enum StepType {
  STEP_COMBO,   // one or more simultaneous keys
  STEP_DELAY,   // wait N ms
  STEP_TEXT     // type a string
};

struct MacroStep {
  StepType type;
  uint8_t  keys[6];   // up to 6 simultaneous keys per combo step
  uint8_t  keyCount;
  uint16_t delayMs;
  String   text;
};

// ============================================================
//  ACTION
// ============================================================

struct Action {
  ActionType         type    = ACTION_NONE;
  uint32_t           keyCode = 0;          // for ACTION_KEY or ACTION_MEDIA
  std::vector<MacroStep> steps;            // for ACTION_MACRO
  String             text;                 // for ACTION_TEXT
  String             label;                // human-readable label for display
};

// ============================================================
//  MODE
// ============================================================

struct Mode {
  String name;
  Action btn1;
  Action btn2;
  Action btn3Short;
  Action btn3Long;
  Action encCW;
  Action encCCW;
  Action encSW;
};

// ============================================================
//  KEY NAME → HID CODE LOOKUP TABLE
// ============================================================

struct KeyEntry {
  const char* name;
  uint8_t     code;
};

static const KeyEntry KEY_TABLE[] = {
  // Modifiers
  { "LEFTCTRL",    0xe0 }, { "LEFTSHIFT",   0xe1 },
  { "LEFTALT",     0xe2 }, { "LEFTMETA",    0xe3 },
  { "RIGHTCTRL",   0xe4 }, { "RIGHTSHIFT",  0xe5 },
  { "RIGHTALT",    0xe6 }, { "RIGHTMETA",   0xe7 },
  // Letters
  { "A", 0x04 }, { "B", 0x05 }, { "C", 0x06 }, { "D", 0x07 },
  { "E", 0x08 }, { "F", 0x09 }, { "G", 0x0a }, { "H", 0x0b },
  { "I", 0x0c }, { "J", 0x0d }, { "K", 0x0e }, { "L", 0x0f },
  { "M", 0x10 }, { "N", 0x11 }, { "O", 0x12 }, { "P", 0x13 },
  { "Q", 0x14 }, { "R", 0x15 }, { "S", 0x16 }, { "T", 0x17 },
  { "U", 0x18 }, { "V", 0x19 }, { "W", 0x1a }, { "X", 0x1b },
  { "Y", 0x1c }, { "Z", 0x1d },
  // Numbers row
  { "1", 0x1e }, { "2", 0x1f }, { "3", 0x20 }, { "4", 0x21 },
  { "5", 0x22 }, { "6", 0x23 }, { "7", 0x24 }, { "8", 0x25 },
  { "9", 0x26 }, { "0", 0x27 },
  // Common keys
  { "ENTER",      0x28 }, { "ESC",        0x29 }, { "BACKSPACE",  0x2a },
  { "TAB",        0x2b }, { "SPACE",      0x2c }, { "DELETE",     0x4c },
  { "INSERT",     0x49 }, { "HOME",       0x4a }, { "END",        0x4d },
  { "PAGEUP",     0x4b }, { "PAGEDOWN",   0x4e },
  { "RIGHT",      0x4f }, { "LEFT",       0x50 }, { "DOWN",       0x51 },
  { "UP",         0x52 }, { "CAPSLOCK",   0x39 }, { "NUMLOCK",    0x53 },
  { "SCROLLLOCK", 0x47 }, { "SYSRQ",      0x46 },
  // Function keys
  { "F1",  0x3a }, { "F2",  0x3b }, { "F3",  0x3c }, { "F4",  0x3d },
  { "F5",  0x3e }, { "F6",  0x3f }, { "F7",  0x40 }, { "F8",  0x41 },
  { "F9",  0x42 }, { "F10", 0x43 }, { "F11", 0x44 }, { "F12", 0x45 },
  // Misc
  { "MINUS",      0x2d }, { "EQUAL",      0x2e }, { "LEFTBRACE",  0x2f },
  { "RIGHTBRACE", 0x30 }, { "BACKSLASH",  0x31 }, { "SEMICOLON",  0x33 },
  { "APOSTROPHE", 0x34 }, { "GRAVE",      0x35 }, { "COMMA",      0x36 },
  { "DOT",        0x37 }, { "SLASH",      0x38 },
};
static const uint8_t KEY_TABLE_SIZE = sizeof(KEY_TABLE) / sizeof(KEY_TABLE[0]);

// ============================================================
//  MEDIA NAME → KEYCODE LOOKUP TABLE
// ============================================================

struct MediaEntry {
  const char* name;
  uint32_t    code;
};

static const MediaEntry MEDIA_TABLE[] = {
  { "MEDIA_PLAYPAUSE",    KEY_MEDIA_PLAYPAUSE    },
  { "MEDIA_PLAY",         KEY_MEDIA_PLAY         },
  { "MEDIA_PAUSE",        KEY_MEDIA_PAUSE        },
  { "MEDIA_NEXTTRACK",    KEY_MEDIA_NEXTTRACK    },
  { "MEDIA_PREVIOUSTRACK",KEY_MEDIA_PREVIOUSTRACK},
  { "MEDIA_STOP",         KEY_MEDIA_STOP         },
  { "MEDIA_VOLUMEUP",     KEY_MEDIA_VOLUMEUP     },
  { "MEDIA_VOLUMEDOWN",   KEY_MEDIA_VOLUMEDOWN   },
  { "MEDIA_MUTE",         KEY_MEDIA_MUTE         },
  { "MEDIA_FASTFORWARD",  KEY_MEDIA_FASTFORWARD  },
  { "MEDIA_REWIND",       KEY_MEDIA_REWIND       },
};
static const uint8_t MEDIA_TABLE_SIZE = sizeof(MEDIA_TABLE) / sizeof(MEDIA_TABLE[0]);


// ============================================================
//  HELPER: string trim and uppercase
// ============================================================

static String trimStr(String s) {
  s.trim();
  return s;
}

static String upperStr(String s) {
  s.toUpperCase();
  return s;
}

// ============================================================
//  RESOLVE A SINGLE KEY NAME → HID CODE
// ============================================================

static uint8_t resolveKeyName(const String& name) {
  String upper = upperStr(trimStr(name));
  for (uint8_t i = 0; i < KEY_TABLE_SIZE; i++) {
    if (upper == KEY_TABLE[i].name) return KEY_TABLE[i].code;
  }
  Serial.printf("[ConfigLoader] Unknown key name: '%s'\n", name.c_str());
  return 0x00;
}

// ============================================================
//  RESOLVE A MEDIA KEY NAME → MEDIA CODE
// ============================================================

static uint32_t resolveMediaName(const String& name) {
  String upper = upperStr(trimStr(name));
  for (uint8_t i = 0; i < MEDIA_TABLE_SIZE; i++) {
    if (upper == MEDIA_TABLE[i].name) return MEDIA_TABLE[i].code;
  }
  return 0;
}

// ============================================================
//  PARSE A COMBO STRING  "LEFTMETA+LEFTSHIFT+S"
//  Fills a MacroStep of type STEP_COMBO
// ============================================================

static MacroStep parseCombo(const String& comboStr) {
  MacroStep step;
  step.type     = STEP_COMBO;
  step.keyCount = 0;
  step.delayMs  = 0;

  String remaining = comboStr;
  while (remaining.length() > 0) {
    int plus = remaining.indexOf('+');
    String token;
    if (plus == -1) {
      token     = remaining;
      remaining = "";
    } else {
      token     = remaining.substring(0, plus);
      remaining = remaining.substring(plus + 1);
    }
    token = trimStr(token);
    if (token.length() == 0) continue;
    uint8_t code = resolveKeyName(token);
    if (code != 0x00 && step.keyCount < 6) {
      step.keys[step.keyCount++] = code;
    }
  }
  return step;
}

// ============================================================
//  PARSE MACRO STRING
//  "LEFTCTRL+C,DELAY:150,TEXT:Hello,LEFTMETA+D"
//  Returns a vector of MacroStep
// ============================================================

static std::vector<MacroStep> parseMacro(const String& macroStr) {
  std::vector<MacroStep> steps;
  String remaining = macroStr;

  while (remaining.length() > 0) {
    int comma = remaining.indexOf(',');
    String token;
    if (comma == -1) {
      token     = remaining;
      remaining = "";
    } else {
      token     = remaining.substring(0, comma);
      remaining = remaining.substring(comma + 1);
    }
    token = trimStr(token);
    if (token.length() == 0) continue;

    String upper = upperStr(token);

    if (upper.startsWith("DELAY:")) {
      // Delay step
      MacroStep step;
      step.type    = STEP_DELAY;
      step.keyCount = 0;
      step.delayMs = (uint16_t)token.substring(6).toInt();
      steps.push_back(step);

    } else if (upper.startsWith("TEXT:")) {
      // Text step — preserve original case
      MacroStep step;
      step.type     = STEP_TEXT;
      step.keyCount = 0;
      step.delayMs  = 0;
      step.text     = token.substring(5);
      steps.push_back(step);

    } else {
      // Key combo step
      steps.push_back(parseCombo(token));
    }
  }
  return steps;
}

// ============================================================
//  BUILD AN Action FROM A VALUE STRING
// ============================================================

static Action parseAction(const String& rawValue) {
  Action action;
  String value = trimStr(rawValue);
  String upper = upperStr(value);

  if (upper == "NEXT_MODE") {
    action.type  = ACTION_NEXT_MODE;
    action.label = "Next mode";
    return action;
  }

  if (upper == "NONE" || value.length() == 0) {
    action.type  = ACTION_NONE;
    action.label = "None";
    return action;
  }

  // Media key?
  uint32_t mediaCode = resolveMediaName(upper);
  if (mediaCode != 0) {
    action.type    = ACTION_MEDIA;
    action.keyCode = mediaCode;
    action.label   = value;
    return action;
  }

  // Macro?
  if (upper.startsWith("MACRO:")) {
    action.type  = ACTION_MACRO;
    action.steps = parseMacro(value.substring(6));
    action.label = "Macro";
    return action;
  }

  // Text?
  if (upper.startsWith("TEXT:")) {
    action.type  = ACTION_TEXT;
    action.text  = value.substring(5);
    action.label = "Text";
    return action;
  }

  // Single key or combo?
  if (value.length() > 0) {
    // Treat as a single combo step wrapped in a macro
    action.type = ACTION_MACRO;
    action.steps.push_back(parseCombo(value));
    action.label = value;
    return action;
  }

  action.type  = ACTION_NONE;
  action.label = "None";
  return action;
}

// ============================================================
//  ASSIGN PARSED ACTION TO THE CORRECT CONTROL SLOT
// ============================================================

static void assignAction(Mode& mode, const String& key, const String& value) {
  String k = upperStr(trimStr(key));
  Action a = parseAction(value);

  if      (k == "BTN1")      mode.btn1      = a;
  else if (k == "BTN2")      mode.btn2      = a;
  else if (k == "BTN3_SHORT") mode.btn3Short = a;
  else if (k == "BTN3_LONG") mode.btn3Long  = a;
  else if (k == "ENC_CW")    mode.encCW     = a;
  else if (k == "ENC_CCW")   mode.encCCW    = a;
  else if (k == "ENC_SW")    mode.encSW     = a;
  else if (k == "NAME")      mode.name      = trimStr(value);
  else Serial.printf("[ConfigLoader] Unknown key in config: '%s'\n", k.c_str());
}

// ============================================================
//  PARSE A SINGLE MODE FILE
// ============================================================

static bool parseModeFile(const char* path, Mode& mode) {
  File f = SD.open(path);
  if (!f) {
    Serial.printf("[ConfigLoader] Cannot open file: %s\n", path);
    return false;
  }

  Serial.printf("[ConfigLoader] Parsing: %s\n", path);

  // Default mode name from filename (without path and extension)
  String fname = String(path);
  int    slash = fname.lastIndexOf('/');
  int    dot   = fname.lastIndexOf('.');
  mode.name    = fname.substring(slash + 1, dot != -1 ? dot : fname.length());

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    // Skip empty lines and comments
    if (line.length() == 0 || line.startsWith("#")) continue;

    int eq = line.indexOf('=');
    if (eq == -1) continue;

    String k = line.substring(0, eq);
    String v = line.substring(eq + 1);
    assignAction(mode, k, v);
  }

  f.close();
  Serial.printf("[ConfigLoader] Loaded mode: '%s'\n", mode.name.c_str());
  return true;
}

// ============================================================
//  SCAN /modes/ FOLDER AND LOAD ALL .txt FILES
// ============================================================

static bool loadAllModes(std::vector<Mode>& modes) {
  modes.clear();

  File dir = SD.open("/modes");
  if (!dir || !dir.isDirectory()) {
    Serial.println("[ConfigLoader] ERROR: /modes directory not found on SD");
    return false;
  }

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) { entry.close(); continue; }

    String name = String(entry.name());
    entry.close();

    // Only process .txt files
    name.toLowerCase();
    if (!name.endsWith(".txt")) continue;

    // Build full path
    String fullPath = "/modes/" + name;

    Mode mode;
    if (parseModeFile(fullPath.c_str(), mode)) {
      modes.push_back(mode);
    }
  }

  dir.close();

  if (modes.empty()) {
    Serial.println("[ConfigLoader] WARNING: No mode files found in /modes/");
    return false;
  }

  Serial.printf("[ConfigLoader] Total modes loaded: %d\n", (int)modes.size());
  return true;
}

// ============================================================
//  EXECUTE AN ACTION
//  Requires a reference to the active KeyboardDevice
// ============================================================

static void executeAction(const Action& action, KeyboardDevice& keyboard) {
  switch (action.type) {

    case ACTION_NONE:
      break;

    case ACTION_MEDIA:
      keyboard.mediaKeyPress(action.keyCode);
      delay(30);
      keyboard.mediaKeyRelease(action.keyCode);
      break;

    case ACTION_MACRO:
      for (const MacroStep& step : action.steps) {
        if (step.type == STEP_COMBO) {
          // Press all keys in the combo
          for (uint8_t i = 0; i < step.keyCount; i++) {
            keyboard.keyPress(step.keys[i]);
          }
          delay(30);
          // Release in reverse order
          for (int8_t i = step.keyCount - 1; i >= 0; i--) {
            keyboard.keyRelease(step.keys[i]);
          }
          delay(20);

        } else if (step.type == STEP_DELAY) {
          delay(step.delayMs);

        } else if (step.type == STEP_TEXT) {
          // Type each character individually
          // Maps printable ASCII to HID scan codes
          for (uint16_t i = 0; i < step.text.length(); i++) {
            char c = step.text.charAt(i);
            uint8_t keyCode = 0x00;
            bool    shifted = false;

            if (c >= 'a' && c <= 'z') {
              keyCode = 0x04 + (c - 'a');
            } else if (c >= 'A' && c <= 'Z') {
              keyCode = 0x04 + (c - 'A');
              shifted = true;
            } else if (c >= '1' && c <= '9') {
              keyCode = 0x1e + (c - '1');
            } else if (c == '0') {
              keyCode = 0x27;
            } else {
              // Common punctuation (US QWERTY layout)
              switch (c) {
                case ' ':  keyCode = 0x2c; break;
                case '\n': keyCode = 0x28; break;
                case '\t': keyCode = 0x2b; break;
                case '-':  keyCode = 0x2d; break;
                case '=':  keyCode = 0x2e; break;
                case '[':  keyCode = 0x2f; break;
                case ']':  keyCode = 0x30; break;
                case '\\': keyCode = 0x31; break;
                case ';':  keyCode = 0x33; break;
                case '\'': keyCode = 0x34; break;
                case '`':  keyCode = 0x35; break;
                case ',':  keyCode = 0x36; break;
                case '.':  keyCode = 0x37; break;
                case '/':  keyCode = 0x38; break;
                // Shifted symbols
                case '!': keyCode = 0x1e; shifted = true; break;
                case '@': keyCode = 0x1f; shifted = true; break;
                case '#': keyCode = 0x20; shifted = true; break;
                case '$': keyCode = 0x21; shifted = true; break;
                case '%': keyCode = 0x22; shifted = true; break;
                case '^': keyCode = 0x23; shifted = true; break;
                case '&': keyCode = 0x24; shifted = true; break;
                case '*': keyCode = 0x25; shifted = true; break;
                case '(': keyCode = 0x26; shifted = true; break;
                case ')': keyCode = 0x27; shifted = true; break;
                case '_': keyCode = 0x2d; shifted = true; break;
                case '+': keyCode = 0x2e; shifted = true; break;
                case '{': keyCode = 0x2f; shifted = true; break;
                case '}': keyCode = 0x30; shifted = true; break;
                case '|': keyCode = 0x31; shifted = true; break;
                case ':': keyCode = 0x33; shifted = true; break;
                case '"': keyCode = 0x34; shifted = true; break;
                case '~': keyCode = 0x35; shifted = true; break;
                case '<': keyCode = 0x36; shifted = true; break;
                case '>': keyCode = 0x37; shifted = true; break;
                case '?': keyCode = 0x38; shifted = true; break;
                default: break;
              }
            }

            if (keyCode != 0x00) {
              if (shifted) keyboard.keyPress(KEY_LEFTSHIFT);
              keyboard.keyPress(keyCode);
              delay(20);
              keyboard.keyRelease(keyCode);
              if (shifted) keyboard.keyRelease(KEY_LEFTSHIFT);
              delay(10);
            }
          }
        }
      }
      break;

    case ACTION_TEXT:
      // Handled as a macro with a single text step
      {
        Action wrapper;
        wrapper.type = ACTION_MACRO;
        MacroStep step;
        step.type = STEP_TEXT;
        step.keyCount = 0;
        step.delayMs  = 0;
        step.text = action.text;
        wrapper.steps.push_back(step);
        executeAction(wrapper, keyboard);
      }
      break;

    case ACTION_NEXT_MODE:
      // Handled externally in main sketch
      break;
  }
}
