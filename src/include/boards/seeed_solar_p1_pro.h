#pragma once

/*
 * BOARD "SenseCAP Solar Node P1-Pro"
 * nRF52840 (Xiao nRF52840 Plus) + SX1262 (Wio-SX1262 for XIAO) + L76K GPS
 * No display (HAS_SCREEN 0 in variant.h) -- status conveyed via LEDs only.
 *
 * Board define set by boards/seeed_solar_p1_pro.json:
 *   ARDUINO_SEEED_SOLAR_P1_PRO
 *
 * Pin roles were confirmed against meshcore-dev/MeshCore's shipped
 * variants/sensecap_solar/ port (same physical hardware), since Seeed does
 * not publish a carrier-board schematic for this product. Verify all pin
 * numbers against your hardware before production use.
 * Pin numbers are Arduino (variant) indices, not raw P0.xx/P1.xx pads.
 */
#if defined(ARDUINO_SEEED_SOLAR_P1_PRO)

#define CDP_BOARD_NAME "SenseCAP Solar Node P1-Pro"

// ── Radio ─────────────────────────────────────────────────────────────────────
#define CDPCFG_RADIO_SX1262

// SX1262 SPI chip-select / control lines (Arduino pin numbers from variant.h)
#define CDPCFG_PIN_LORA_CS      4    // D4  = P0.04 — SX1262 NSS
#define CDPCFG_PIN_LORA_RST     2    // D2  = P0.28 — SX1262 NRESET
#define CDPCFG_PIN_LORA_DIO1    1    // D1  = P0.03 — SX1262 DIO1 (IRQ)
#define CDPCFG_PIN_LORA_DIO0    1    // alias — CDP expects this name for SX1262
#define CDPCFG_PIN_LORA_BUSY    3    // D3  = P0.29 — SX1262 BUSY

// TCXO reference oscillator on this board (1.8 V).
// Enables lora.setTCXO() in DuckLoRa.cpp.
#define CDPCFG_LORA_TCXO_VOLTAGE  1.8f

// DIO2 drives the on-board RF switch — enable it in DuckLoRa.cpp.
#define CDPCFG_LORA_DIO2_RF_SWITCH

// ── Display ───────────────────────────────────────────────────────────────────
// This board has no physical display -- status is conveyed via the white
// user LED, blue LoRa-TX LED, and the board's own charging/solar/heartbeat
// indicator LEDs. Defining CDPCFG_OLED_NONE prevents DuckDisplay.cpp from
// instantiating an SSD1306 object (which would fail to compile on nRF52).
#define CDPCFG_OLED_NONE

// ── WiFi ──────────────────────────────────────────────────────────────────────
// nRF52840 has no WiFi; disabling prevents WiFi.h / EEPROM.h from being pulled
// into the build via CDP.h → DuckWifi.h.
#define CDPCFG_WIFI_NONE


#endif  // ARDUINO_SEEED_SOLAR_P1_PRO
