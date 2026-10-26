#pragma once

/*
 * BOARD "WIFI_LoRa_32_V3"
 * https://heltec.org/project/wifi-lora-32-v3/
 */

// This is becoming ridiculous. 3 different build systems, 3 different ways to define the board name.
// Arduino IDE: ARDUINO_HELTEC_WIFI_LORA_32_V3
// PlatformIO: ARDUINO_heltec_wifi_lora_32_V3
// Arduino Cli: ARDUINO_heltec_wifi_32_lora_V3
#if defined(ARDUINO_HELTEC_WIFI_LORA_32_V3) || defined(ARDUINO_heltec_wifi_lora_32_V3) || defined(ARDUINO_heltec_wifi_32_lora_V3)
#define CDP_BOARD_NAME "Heltec WiFi LoRa 32 V3"
#define CDPCFG_RADIO_SX1262


// Uncomment this to enable the OLED display
#define ENABLE_DISPLAY

// Lora configurations
#define CDPCFG_PIN_LORA_CS      8
#define CDPCFG_PIN_LORA_RST     12
#define CDPCFG_PIN_LORA_DIO0    13
#define CDPCFG_PIN_LORA_BUSY    13 // SX1262 BUSY
#define CDPCFG_PIN_LORA_DIO1    14 // SX1262 IRQ

#if defined(ARDUINO_heltec_wifi_lora_32_V4) && (defined(USE_KCT8103L_PA) || defined(USE_KCT8103L_LNA))
// Heltec WiFi LoRa 32 V4 revision 4.3 (and the higher-PSRAM V4_R8 variant)
// replaced the V4.2 board's GC1109 front-end module with a KCT8103L FEM.
// Unlike earlier Heltec boards (which rely purely on the SX1262's internal
// DIO2-as-RF-switch), this FEM needs its PA (TX) / LNA (RX) signal path
// selected explicitly over 3 GPIOs. RadioLib has no built-in knowledge of
// this board-specific external FEM, so DuckLoRa.cpp drives these pins
// directly around every TX/RX transition (see setExternalPaMode()).
//
// Pin numbers and control sequence confirmed against Heltec's own Arduino
// board package (HelTecAutomation/Heltec_ESP32, src/driver/board-config.h
// "#elif defined(USE_KCT8103L_PA)" + src/radio/radio.c RadioTx()/RadioRx())
// and https://wiki.heltec.org/docs/devices/open-source-hardware/esp32-series/lora-32/wifi-lora-32-v4/
// ("V4.3, select USE_KCT8103L_PA" FEM option).
//
// NOTE: this assumes a V4.3 (KCT8103L FEM) board, matching the hardware this
// was implemented/tested for. An earlier V4.2 board uses a different FEM
// (GC1109) with a different pin/control scheme that isn't implemented here.
//
// Each signal path is independently opt-in and disabled by default -- pass
// one or both flags in build_flags (e.g. in a local platformio.ini env):
//   -DUSE_KCT8103L_PA   -- power the FEM for TX (PA) when transmitting.
//   -DUSE_KCT8103L_LNA  -- power the FEM for RX (LNA) when receiving.
// Whichever flag is NOT set, that transition instead fully powers down the
// whole FEM (see disableExternalPa() in DuckLoRa.cpp) rather than selecting
// that path. WARNING: POWER/CSD gate the entire antenna-switch chip, not
// one path in isolation -- disabling a path you still need (e.g. LNA while
// still expecting to receive) may disconnect the antenna entirely rather
// than just reducing gain. Verify real TX/RX on hardware before relying on
// a single-path configuration.
#define CDPCFG_LORA_HAS_EXTERNAL_PA
#define CDPCFG_PIN_LORA_PA_POWER 7  // FEM power supply enable
#define CDPCFG_PIN_LORA_PA_CSD   2  // FEM chip shutdown/select
#define CDPCFG_PIN_LORA_PA_CTX   5  // FEM TX(PA)/RX(LNA) path select
#endif

// Diagnostic: if this board resets with "rst:0x1 (POWERON)" during LoRa TX
// (seen even on wall-charger/battery power, so it's not a weak USB port),
// uncomment the line below and rebuild to test whether a lower PA current
// peak stops the resets. If it does, the board's onboard decoupling can't
// absorb the SX1262's TX current transient at the default power and either a
// bulk capacitor across the 3.3V rail near the module or a permanent TX
// power reduction is needed. If it still resets near minimum power, suspect
// a hardware defect on this specific unit instead.
// #define CDPCFG_RF_LORA_TXPOW 2

// Oled Display settings
#ifdef ENABLE_DISPLAY
#define CDPCFG_PIN_OLED_CLOCK SCL_OLED
#define CDPCFG_PIN_OLED_DATA SDA_OLED
#define CDPCFG_PIN_OLED_RESET RST_OLED
#define CDPCFG_PIN_OLED_ROTATION U8G2_R0
#endif

// ── WiFi ──────────────────────────────────────────────────────────────────────
// This ESP32-S3 has WiFi hardware, but none of the Heltec sketches use it
// (MamaDuck defaults to DuckWifiNone regardless). Disabling it here prevents
// CDP.h from pulling in WiFi.h / WiFiClientSecure.h / Update.h / EEPROM.h via
// DuckWifi.h, saving flash/RAM for code that's never exercised. Comment this
// out if a future Heltec sketch needs WiFi (e.g. OTA/AP-provisioning).
#define CDPCFG_WIFI_NONE

#endif
