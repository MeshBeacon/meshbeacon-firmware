#pragma once

/*
 * BOARD "Seeed SenseCAP Card Tracker T1000-E"
 * nRF52840 + LR1110 (LoRa) + Airoha GNSS
 * No display of any kind (HAS_SCREEN 0 in variant.h).
 *
 * Board define set by boards/seeed_t1000e.json:
 *   ARDUINO_SEEED_T1000E
 *
 * Verify all pin numbers against your hardware schematic.
 * Pin numbers here are RAW nRF52840 pin numbers (Pn.xx = n*32+xx), matching
 * variant.h's identity g_ADigitalPinMap.
 */
#if defined(ARDUINO_SEEED_T1000E)

#define CDP_BOARD_NAME "Seeed SenseCAP Card Tracker T1000-E"

// ── Radio ─────────────────────────────────────────────────────────────────────
#define CDPCFG_RADIO_LR1110

// LR1110 SPI chip-select / control lines (raw nRF pin numbers from variant.h)
#define CDPCFG_PIN_LORA_CS      LORA_CS     // P0.12 — LR1110 NSS
#define CDPCFG_PIN_LORA_RST     LORA_RESET  // P1.10 — LR1110 NRESET
#define CDPCFG_PIN_LORA_DIO1    LORA_DIO1   // P1.01 — LR1110 DIO1 (IRQ)
#define CDPCFG_PIN_LORA_DIO0    LORA_DIO1   // alias — CDP expects this name for LR1110
#define CDPCFG_PIN_LORA_BUSY    LORA_DIO2   // P0.07 — LR1110 BUSY

// TCXO reference oscillator on this board (1.6 V).
// Enables lora.setTCXO() in DuckLoRa.cpp.
#define CDPCFG_LORA_TCXO_VOLTAGE  1.6f

// DIO5/DIO6/DIO7/DIO8 drive the LR1110's RF/GNSS switch matrix on this
// board (see boards/seeed_t1000e/rfswitch.h) — enable it in DuckLoRa.cpp.
#define CDPCFG_LORA_RFSWITCH_TABLE

// ── Display ───────────────────────────────────────────────────────────────────
// No OLED/display hardware on this board at all. Defining CDPCFG_OLED_NONE
// prevents DuckDisplay.cpp from instantiating an SSD1306 object (which
// would fail to compile on nRF52 anyway).
#define CDPCFG_OLED_NONE

// ── WiFi ──────────────────────────────────────────────────────────────────────
// nRF52840 has no WiFi; disabling prevents WiFi.h / EEPROM.h from being pulled
// into the build via CDP.h → DuckWifi.h.
#define CDPCFG_WIFI_NONE


#endif  // ARDUINO_SEEED_T1000E
