# BLE HID Controller — ESP32

**Controlador HID multifunción inalámbrico basado en ESP32 y Bluetooth Low Energy**

> De la prueba de concepto a un dispositivo configurable desde tarjeta SD.

---

## Índice

- [Descripción](#descripción)
- [Hardware](#hardware)
- [Versiones](#versiones)
  - [BLE_HID_V1 — Prueba de concepto](#v1--prueba-de-concepto)
  - [BLE_HID_V2 — Perfiles configurables desde SD](#v2--perfiles-configurables-desde-sd)
- [Instalación y dependencias](#instalación-y-dependencias)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Hoja de ruta](#hoja-de-ruta)
- [Proyectos relacionados](#proyectos-relacionados)
- [Licencia](#licencia)

---

## Descripción

Este proyecto convierte un ESP32 en un **teclado HID Bluetooth** completamente funcional, sin necesidad de instalar ningún driver en el equipo receptor. El dispositivo se empareja como un teclado estándar en Windows, macOS, Linux y Android, y permite ejecutar atajos de teclado, teclas multimedia y macros personalizadas a través de botones físicos y un encoder rotativo.

El proyecto está estructurado en dos versiones que documentan la evolución desde una prueba de concepto con modos hardcodeados hasta un sistema completamente configurable a través de archivos de texto en una tarjeta microSD.

La idea es que el prototipo sea **100% inalámbrico**, alimentado por una batería LiPo de 3.7V / 1000mAh con cargador TP4056 y un convertidor que fije una tensión estable de 3.3V. En la primera etapa del prototipo he utilizado un boost converter para elevar la tensión a 5V y luego ya el regulador lineal que viene en la placa de desarrollo ESP32 baja a 3.3V, en un futuro cambiaré esto por un conversor buck optimizado para bateriás de 3.7V, ya que los buck que tenía a mi disposición necesitaban una diferencia de tensión de entre la entrada y la salida mayor de la que puede dar la bateria lipo y no me servian.

La placa de desarrollo reutilizada en este proyecto forma parte de la colección **[MCU Programming Collection](https://github.com/Crayfe/MCU-Programming-Collection)**, donde se documenta el proceso de construcción de la placa junto con otras pruebas y proyectos de programación de microcontroladores.

---

## Hardware

| Componente | Modelo | Pin(es) GPIO | Protocolo |
|---|---|---|---|
| Microcontrolador | ESP32-WROOM-32 | — | — |
| Display OLED | SSD1306 128×64 | SDA: 21 / SCL: 22 | I2C |
| Lector microSD | Módulo SPI | MISO: 19 / MOSI: 23 / SCK: 18 / CS: 16 | SPI |
| Encoder rotativo | HW-040 | CLK: 32 / DT: 33 / SW: 25 | Digital |
| Pulsador 1 | Táctil | 26 | Digital (pull-up) |
| Pulsador 2 | Táctil | 27 | Digital (pull-up) |
| Pulsador 3 | Táctil | 14 | Digital (pull-up) |
| Buzzer | CEM-1203(42) | 13 | PWM |
| Batería | LiPo 3.7V 1000mAh | — | — |
| Cargador | TP4056 | — | — |
| Convertidor boost | 3.7V → 5V | — | — |

> **Nota sobre el pin CS de la SD:** evitar el GPIO5, que es un strapping pin del ESP32 y puede corromper la tarjeta durante el arranque. El pin recomendado es el **GPIO16**.

---

## Versiones

### BLE_HID_V1 — Prueba de concepto

Firmware con dos modos de operación definidos directamente en el código. El objetivo de esta versión es validar el stack BLE HID sobre el hardware y explorar la detección de pulsaciones cortas y largas sin bloquear el loop principal.

**Modo Media**

| Control | Acción |
|---|---|
| Encoder (giro) | Volumen ↑ / Volumen ↓ |
| Encoder (pulsado) | Mute |
| BTN1 | Play / Pause |
| BTN2 | Pista siguiente |
| BTN3 (corto) | Pista anterior |
| BTN3 (largo > 800ms) | Cambiar a modo Sistema |

**Modo Sistema**

| Control | Acción |
|---|---|
| Encoder (giro) | Brillo ↑ / Brillo ↓ (F2 / F1) |
| Encoder (pulsado) | Bloquear sesión (Win + L) |
| BTN1 | Escritorio (Win + D) |
| BTN2 | Captura de pantalla (Win + Shift + S) |
| BTN3 (corto) | Explorador de archivos (Win + E) |
| BTN3 (largo > 800ms) | Cambiar a modo Media |

**Aspectos técnicos destacados de esta versión:**

- `KeyboardDevice` contiene un `std::mutex` interno y no admite operador de asignación. Se resuelve inicializándolo en el ámbito global mediante una función auxiliar que devuelve la configuración ya preparada.
- Los nombres de teclas en `KeyboardHIDCodes.h` de la librería difieren de otras implementaciones: `KEY_LEFTMETA` (tecla Windows), `KEY_LEFTSHIFT`, etc.
- La detección de pulsación larga se implementa por flancos (sin `delay()`), lo que no bloquea el resto del loop.

---

### BLE_HID_V2 — Perfiles configurables desde SD

El firmware lee al arrancar todos los archivos `.txt` de la carpeta `/modes/` de la tarjeta SD y construye en memoria una lista de modos. Cada archivo define el comportamiento completo de todos los controles para ese modo. No es necesario recompilar ni reflashear para cambiar los mappings.

**Estructura de un archivo de modo:**

```
# Comentario — líneas con # se ignoran
NAME=Media

BTN1=MEDIA_PLAYPAUSE
BTN2=MEDIA_NEXTTRACK
BTN3_SHORT=MEDIA_PREVIOUSTRACK
BTN3_LONG=NEXT_MODE

ENC_CW=MEDIA_VOLUMEUP
ENC_CCW=MEDIA_VOLUMEDOWN
ENC_SW=MEDIA_MUTE
```

**Sistema de macros — tres niveles de complejidad:**

```
# Combo simple
BTN1=MACRO:LEFTMETA+D

# Secuencia de pasos con retardo
BTN2=MACRO:LEFTCTRL+C,DELAY:200,LEFTCTRL+V

# Texto literal (carácter a carácter, distribución QWERTY US)
BTN3_SHORT=TEXT:Hola desde ESP32!
```

**Valores de acción disponibles:**

| Valor | Descripción |
|---|---|
| `MEDIA_PLAYPAUSE`, `MEDIA_NEXTTRACK`, ... | Teclas multimedia |
| `MACRO:KEY1+KEY2` | Combo de teclas simultáneo |
| `MACRO:...,DELAY:ms,...` | Secuencia con retardo en ms |
| `TEXT:cadena` | Escribe texto carácter a carácter |
| `NEXT_MODE` | Cicla al siguiente modo cargado |

**Nombres de teclas modificadoras disponibles:**

`LEFTCTRL`, `LEFTSHIFT`, `LEFTALT`, `LEFTMETA` (Win), `RIGHTCTRL`, `RIGHTSHIFT`, `RIGHTALT`, `RIGHTMETA`

**Modos de ejemplo incluidos:**

| Archivo | Descripción |
|---|---|
| `media.txt` | Control multimedia |
| `system.txt` | Atajos del sistema operativo (Windows) |
| `macros.txt` | Ejemplos de todos los tipos de macro |
| `presentation.txt` | Control de presentaciones (PowerPoint / Impress) |

**Arquitectura del código:**

- `ESP32_BLE_HID_SD.ino` — setup, loop, gestión de entradas, display y buzzer.
- `ConfigLoader.h` — estructuras de datos, parseo de archivos, lookup tables de keycodes y ejecución de acciones. Toda la lógica de configuración está aislada del sketch principal.

---

## Instalación y dependencias

**Requisitos del entorno:**
- Arduino IDE 1.8.x o 2.x
- Soporte para placas ESP32 ([instrucciones](https://github.com/espressif/arduino-esp32#installation-instructions))

**Librerías necesarias:**

| Librería | Fuente |
|---|---|
| [ESP32-BLE-CompositeHID](https://github.com/Mystfit/ESP32-BLE-CompositeHID) | GitHub (instalar como .zip) |
| [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) | Arduino Library Manager |
| Callback (Tom Stewart) | Arduino Library Manager |
| Adafruit SSD1306 | Arduino Library Manager |
| Adafruit GFX Library | Arduino Library Manager |
| SD (built-in) | Incluida en el core ESP32 |

**Pasos:**

1. Clona o descarga este repositorio.
2. Instala todas las librerías listadas.
3. Abre el sketch de la versión deseada (`v1/` o `v2/`).
4. Selecciona la placa **ESP32 Dev Module** en el IDE.
5. Flashea.

**Para la v2, preparar la SD:**
1. Formatea la tarjeta en **FAT32** (máximo 32 GB recomendado).
2. Crea la carpeta `/modes/` en la raíz.
3. Copia los archivos `.txt` de `v2/modes/` en esa carpeta.
4. Inserta la SD y reinicia el ESP32.

---

## Hoja de ruta

- [ ] **Refinamiento del firmware** — mejora del sistema de macros, soporte para distribuciones de teclado distintas a QWERTY US en la escritura de texto.
- [ ] **Web UI OTA** — aprovechar el WiFi del ESP32 para servir una interfaz web local desde la que editar y subir archivos de modo sin retirar la SD.
- [ ] **PCB a medida** — diseño de una placa de circuito impreso que integre todos los componentes y elimine el cableado de la placa de pruebas.
- [ ] **Carcasa** — diseño en FreeCAD e impresión 3D de una carcasa que integre todos los componentes en un prototipo compacto y portable.
- [ ] **Expansión de entradas** — soporte para más botones, joystick analógico o trackpad.
- [ ] **Soporte para ratón HID** — la librería ya incluye `MouseDevice`, pendiente de integrar.

---

## Proyectos relacionados

La placa de desarrollo usada en este proyecto proviene de la reutilización de hardware documentado en **[MCU Programming Collection](https://github.com/Crayfe/MCU-Programming-Collection)**, un repositorio con una colección de proyectos y pruebas de programación con microcontroladores donde se puede encontrar la plantilla base de la placa y otros experimentos con el mismo hardware.

---

## Licencia

Distribuido bajo la licencia MIT. Consulta el archivo [LICENSE](LICENSE) para más información.
