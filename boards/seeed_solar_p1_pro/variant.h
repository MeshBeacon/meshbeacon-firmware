#ifndef _SEEED_SOLAR_P1_PRO_H_
#define _SEEED_SOLAR_P1_PRO_H_
#include "WVariant.h"
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Clock Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define VARIANT_MCK (64000000ul) // Master clock frequency
#define USE_LFXO                // 32.768kHz crystal for LFCLK

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Pin Capacity Definitions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define PINS_COUNT (33u)       // Total GPIO pins
#define NUM_DIGITAL_PINS (33u) // Digital I/O pins
#define NUM_ANALOG_INPUTS (8u) // Analog inputs (A0-A5 + VBAT + AREF)
#define NUM_ANALOG_OUTPUTS (0u)

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  LED Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  LEDs
#define PIN_LED1 (11) // White user LED    P0.15
#define PIN_LED2 (12) // Blue LoRa-TX LED  P0.19

#define LED_GREEN PIN_LED1
#define LED_BLUE PIN_LED2
#define LED_STATE_ON 1 // State when LED is lit
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Button Configuration
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define CANCEL_BUTTON_PIN D13 // Front-panel user button
// #define BUTTON_NEED_PULLUP   1
#define CANCEL_BUTTON_ACTIVE_LOW true
#define CANCEL_BUTTON_ACTIVE_PULLUP false

// A second, user-defined button also exists on this board (per Seeed specs:
// power on/off, reset, user-defined) -- wired separately from the cancel/menu
// button above.
#define PIN_BUTTON2 D20 // Second user-defined button
//  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//   Digital Pin Mapping (D0-D20)
//  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Pin roles confirmed against meshcore-dev/MeshCore's shipped
// variants/sensecap_solar/variant.cpp (same physical board), since Seeed does
// not publish a carrier-board schematic for this product.
#define D0 0   // P0.02 GNSS_WAKEUP
#define D1 1   // P0.03 LORA_DIO1
#define D2 2   // P0.28 LORA_RESET
#define D3 3   // P0.29 LORA_BUSY
#define D4 4   // P0.04 LORA_CS
#define D5 5   // P0.05 LORA_SW (RF switch / RXEN)
#define D6 6   // P1.11 GNSS_TX (data from MCU to GPS)
#define D7 7   // P1.12 GNSS_RX (data from GPS to MCU)
#define D8 8   // P1.13 SPI_SCK
#define D9 9   // P1.14 SPI_MISO
#define D10 10 // P1.15 SPI_MOSI
#define D11 11 // P0.15 White user LED
#define D12 12 // P0.19 Blue LED (LoRa TX indicator)
#define D13 13 // P1.01 User Button
#define D14 14 // P0.09 NFC1 / Grove SDA
#define D15 15 // P0.10 NFC2 / Grove SCL
#define D16 16 // P0.31 VBAT_ADC
#define D17 17 // P1.03 GNSS_RESET
#define D18 18 // P1.05 GNSS_ENABLE
#define D19 19 // P0.14 BAT_READ (VBAT_ENABLE, active LOW)
#define D20 20 // P1.07 Second user button
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Analog Pin Definitions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define PIN_A0 0     // P0.02 Analog Input 0
#define PIN_A1 1     // P0.03 Analog Input 1
#define PIN_A2 2     // P0.28 Analog Input 2
#define PIN_A3 3     // P0.29 Analog Input 3
#define PIN_A4 4     // P0.04 Analog Input 4
#define PIN_A5 5     // P0.05 Analog Input 5
#define PIN_VBAT D16 // P0.31 Battery voltage sense
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Communication Interfaces
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  I2C Configuration (single bus: Grove / NFC header -- no on-board display)
#define PIN_WIRE_SDA D14 // P0.09
#define PIN_WIRE_SCL D15 // P0.10
#define WIRE_INTERFACES_COUNT 1
#define I2C_NO_RESCAN

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

// No physical display on this board -- outdoor solar node/repeater, status
// is conveyed via the 4 status LEDs (charging x2, solar x1, mesh heartbeat
// x1) plus the user LEDs above, not a screen.
#define HAS_SCREEN 0

// SPI Configuration (SX1262)

#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO 9  // P1.14 (D9)
#define PIN_SPI_MOSI 10 // P1.15 (D10)
#define PIN_SPI_SCK 8   // P1.13 (D8)

// SX1262 LoRa Module Pins (Wio-SX1262 for XIAO add-on module)
#define USE_SX1262
#define SX126X_CS D4                 // Chip select
#define SX126X_DIO1 D1               // Digital IO 1 (Interrupt)
#define SX126X_BUSY D3               // Busy status
#define SX126X_RESET D2              // Reset control
#define SX126X_DIO3_TCXO_VOLTAGE 1.8 // TCXO supply voltage
#define SX126X_RXEN D5               // RX enable control
#define SX126X_TXEN RADIOLIB_NC
#define SX126X_DIO2_AS_RF_SWITCH // This Line is really necessary for SX1262 to work with RF switch or will loss TX power
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Power Management
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

#define BAT_READ 19 // D19 = P0.14  Battery divider enable (VBAT_ENABLE), active LOW on this board.
#define ADC_CTRL BAT_READ
#define ADC_CTRL_ENABLED LOW // Output LOW (not HIGH) to enable reading of the BAT voltage
#define BATTERY_SENSE_RESOLUTION_BITS 12
#define ADC_MULTIPLIER 3.0 // 1M / 512k divider bridge
#define BATTERY_PIN PIN_VBAT
#define AREF_VOLTAGE 3.0
// We rely on the nrf52840 USB controller to tell us if we are hooked to a power supply
#define NRF_APM
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  GPS L76K
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#define GPS_L76K
#ifdef GPS_L76K
#define GPS_TX_PIN D6 // P1.11 - This is data from the MCU
#define GPS_RX_PIN D7 // P1.12 - This is data from the GNSS
#define HAS_GPS 1
#define GPS_BAUDRATE 9600
#define GPS_THREAD_INTERVAL 50
#define PIN_SERIAL1_RX GPS_RX_PIN
#define PIN_SERIAL1_TX GPS_TX_PIN

#define PIN_GPS_STANDBY D0

// GNSS module enable line -- active-HIGH (confirmed via MeshCore's
// MicroNMEALocationProvider.h GPS_EN_ACTIVE default + sensecap_solar's lack
// of a GPS_EN_ACTIVE override); variant.cpp's initVariant() drives it HIGH.
#define GPS_EN D18 // P1.05
#endif

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  On-board QSPI Flash
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Logical pin indices D21-D26 (see variant.cpp g_ADigitalPinMap for the
// raw nRF52840 GPIO numbers each one maps to).
#define PIN_QSPI_SCK (21)
#define PIN_QSPI_CS (22)
#define PIN_QSPI_IO0 (23)
#define PIN_QSPI_IO1 (24)
#define PIN_QSPI_IO2 (25)
#define PIN_QSPI_IO3 (26)
#define EXTERNAL_FLASH_DEVICES P25Q16H
#define EXTERNAL_FLASH_USE_QSPI

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Compatibility Definitions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#ifdef __cplusplus
extern "C" {
#endif
// Serial port placeholders

#define PIN_SERIAL2_RX (-1)
#define PIN_SERIAL2_TX (-1)
#ifdef __cplusplus
}
#endif

#endif // _SEEED_SOLAR_P1_PRO_H_
