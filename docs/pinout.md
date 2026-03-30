# Esquema de conexiones — ESP32 BLE HID Controller

Conexiones de la placa de desarrollo personal (ESP32-WROOM-32).

## Diagrama de pines

```
                        ESP32-WROOM-32
                        ┌─────────────┐
                  3.3V  │             │ GND
                   EN   │             │ GPIO23  ──── SD MOSI
                GPIO36  │             │ GPIO22  ──── OLED SCL
                GPIO39  │             │ GPIO1   (TX)
                GPIO34  │             │ GPIO3   (RX)
                GPIO35  │             │ GPIO21  ──── OLED SDA
                GPIO32  │             │ GPIO19  ──── SD MISO
     ENC CLK ── GPIO32  │             │ GPIO18  ──── SD SCK
      ENC DT ── GPIO33  │             │ GPIO5   
      ENC SW ── GPIO25  │             │ GPIO17
       BTN 1 ── GPIO26  │             │ GPIO16  ──── SD CS 
       BTN 2 ── GPIO27  │             │ GPIO4
       BTN 3 ── GPIO14  │             │ GPIO2
      BUZZER ── GPIO13  │             │ GPIO15
                GPIO12  │             │ GND
                  GND   │             │ VIN (5V)
                        └─────────────┘
```

## Tabla de asignación

| Componente | Señal | GPIO | Notas |
|---|---|---|---|
| OLED SSD1306 | SDA | 21 | I2C (bus compartido) |
| OLED SSD1306 | SCL | 22 | I2C (bus compartido) |
| MicroSD | MOSI | 23 | SPI bus |
| MicroSD | MISO | 19 | SPI bus |
| MicroSD | SCK | 18 | SPI bus |
| MicroSD | CS | **16** | Evitar GPIO5 (strapping pin) |
| Encoder HW-040 | CLK | 32 | Input digital |
| Encoder HW-040 | DT | 33 | Input digital |
| Encoder HW-040 | SW | 25 | Input digital (pull-up interno) |
| Pulsador 1 | — | 26 | Input digital (pull-up interno) |
| Pulsador 2 | — | 27 | Input digital (pull-up interno) |
| Pulsador 3 | — | 14 | Input digital (pull-up interno) |
| Buzzer CEM-1203 | — | 13 | PWM (canal 0) |

## Alimentación

```
  LiPo 3.7V 1000mAh
        │
        ▼
   TP4056 (cargador)
        │
        ▼
  Boost converter ──── 5V ──── VIN del ESP32
        │
       Switch (encendido/apagado)
```

## Notas importantes

- Todos los pulsadores y el SW del encoder se conectan entre el GPIO y GND, usando la resistencia pull-up interna del ESP32 (`INPUT_PULLUP`). En reposo leen `HIGH`, al pulsarlos leen `LOW`.
- El bus I2C (OLED) usa los pines por defecto del ESP32 (21/22). La dirección del dispositivo es `0x3C`.
- El bus SPI de la SD comparte físicamente los pines 18/19/23 con el SPI por defecto del ESP32. Si se añaden otros periféricos SPI, gestionar el CS de cada uno de forma independiente.
