/*
  Copyright (c) 2014-2015 Arduino LLC.  All right reserved.
  Copyright (c) 2016 Sandeep Mistry All right reserved.
  Copyright (c) 2018, Adafruit Industries (adafruit.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 * BOARD "Seeed SenseCAP Card Tracker T1000-E"
 * nRF52840 + LR1110 (LoRa/GNSS-scan combo radio) + Airoha GNSS + QMA6100P accel
 * No OLED / display of any kind on this board (HAS_SCREEN 0).
 *
 * Pin numbers here are RAW nRF52840 pin numbers (Pn.xx = n*32+xx), and
 * g_ADigitalPinMap in variant.cpp is an IDENTITY map (digital pin N == raw
 * nRF pin N) -- unlike seeed_wio_tracker_l1's variant, which remaps a
 * separate logical D0..D30 numbering through a shuffled table. This matches
 * the upstream Meshtastic tracker-t1000-e variant this port is based on
 * (see docs/repo memory for the source URLs), which defines pins the same
 * way. Verify all pin numbers against your hardware schematic before
 * flashing new hardware revisions.
 */
#ifndef _VARIANT_SEEED_T1000E_
#define _VARIANT_SEEED_T1000E_

/** Master clock frequency */
#define VARIANT_MCK (64000000ul)

#define USE_LFXO // Board uses 32kHz crystal for LF

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

// Number of pins defined in the g_ADigitalPinMap array (identity-mapped P0.00-P1.15)
#define PINS_COUNT (48)
#define NUM_DIGITAL_PINS (48)
#define NUM_ANALOG_INPUTS (6)
#define NUM_ANALOG_OUTPUTS (0)

// Use the native nrf52 usb power detection
#define NRF_APM

// ── Power enables ─────────────────────────────────────────────────────────────
#define PIN_3V3_EN (32 + 6)     // P1.06, power to sensors
#define PIN_3V3_ACC_EN (32 + 7) // P1.07, power to accelerometer

// ── LED ───────────────────────────────────────────────────────────────────────
#define PIN_LED1 (0 + 24) // P0.24
#define LED_POWER PIN_LED1
#define LED_GREEN PIN_LED1
#define LED_BLUE -1       // actually green
#define LED_STATE_ON 1    // state when LED is lit

// ── Button ────────────────────────────────────────────────────────────────────
#define BUTTON_PIN (0 + 6) // P0.06
#define BUTTON_ACTIVE_LOW false
#define BUTTON_ACTIVE_PULLUP false
#define BUTTON_SENSE_TYPE 0x5 // enable input pull-down

// ── I2C (QMA6100P accelerometer -- not used by this firmware, kept for compat) ─
#define HAS_WIRE 1
#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA (0 + 26) // P0.26
#define PIN_WIRE_SCL (0 + 27) // P0.27
#define I2C_NO_RESCAN
#define HAS_QMA6100P
#define QMA_6100P_INT_PIN (32 + 2) // P1.02

// ── Serial interfaces ─────────────────────────────────────────────────────────
#define PIN_SERIAL1_RX (0 + 14) // P0.14 -- GNSS TX -> MCU RX
#define PIN_SERIAL1_TX (0 + 13) // P0.13 -- MCU TX -> GNSS RX

#define PIN_SERIAL2_RX (0 + 17) // P0.17
#define PIN_SERIAL2_TX (0 + 16) // P0.16

// ── SPI / LR1110 radio ───────────────────────────────────────────────────────
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_MISO (32 + 8) // P1.08
#define PIN_SPI_MOSI (32 + 9) // P1.09
#define PIN_SPI_SCK  (0 + 11) // P0.11
#define PIN_SPI_NSS  (0 + 12) // P0.12

#define LORA_RESET (32 + 10) // P1.10 -- RST
#define LORA_DIO1  (32 + 1)  // P1.01 -- IRQ
#define LORA_DIO2  (0 + 7)   // P0.07 -- BUSY

#define LORA_SCK  PIN_SPI_SCK
#define LORA_MISO PIN_SPI_MISO
#define LORA_MOSI PIN_SPI_MOSI
#define LORA_CS   PIN_SPI_NSS

// LR1110 module (Semtech LoRa/GNSS-scan/WiFi-scan combo radio)
#define LR1110_IRQ_PIN       LORA_DIO1
#define LR1110_NRESET_PIN    LORA_RESET
#define LR1110_BUSY_PIN      LORA_DIO2
#define LR1110_SPI_NSS_PIN   LORA_CS
#define LR1110_SPI_SCK_PIN   LORA_SCK
#define LR1110_SPI_MOSI_PIN  LORA_MOSI
#define LR1110_SPI_MISO_PIN  LORA_MISO

#define LR11X0_DIO3_TCXO_VOLTAGE 1.6
#define LR11X0_DIO_AS_RF_SWITCH // see rfswitch.h -- DIO5/6/7/8 drive the RF/GNSS switch matrix

// ── GNSS (Airoha) ─────────────────────────────────────────────────────────────
#define HAS_GPS 1
#define GNSS_AIROHA
#define GPS_RX_PIN PIN_SERIAL1_RX
#define GPS_TX_PIN PIN_SERIAL1_TX
#define GPS_BAUDRATE 115200
#define GPS_PROBETRIES 5

#define PIN_GPS_EN (32 + 11) // P1.11
#define GPS_EN_ACTIVE HIGH

#define PIN_GPS_RESET (32 + 15) // P1.15
#define GPS_RESET_MODE HIGH

#define GPS_VRTC_EN   (0 + 8)   // P0.08, always high
#define GPS_SLEEP_INT (32 + 12) // P1.12, always high
#define GPS_RTC_INT   (0 + 15)  // P0.15, normal LOW, wake by HIGH
#define GPS_RESETB_OUT (32 + 14) // P1.14, always input pull-up

// ── Battery / charger ─────────────────────────────────────────────────────────
#define BATTERY_PIN 2 // P0.02/AIN0, BAT_ADC
#define ADC_MULTIPLIER (2.0F)
// P0.04 is sensor power enable, P0.05/AIN3 is CHARGER_DET,
// P1.03 is CHARGE_STA, P1.04 is CHARGE_DONE
#define EXT_CHRG_DETECT (32 + 3) // P1.03
#define EXT_CHRG_DETECT_VALUE LOW
#define EXT_PWR_DETECT (0 + 5)  // P0.05

#define ADC_RESOLUTION 14
#define BATTERY_SENSE_RESOLUTION_BITS 12

#undef AREF_VOLTAGE
#define AREF_VOLTAGE 3.0
#define VBAT_AR_INTERNAL AR_INTERNAL_3_0

// ── Buzzer ────────────────────────────────────────────────────────────────────
#define BUZZER_EN_PIN (32 + 5) // P1.05, always high
#define PIN_BUZZER (0 + 25)    // P0.25, pwm output

// ── Misc sensors (not wired up by this firmware; kept for pin-map completeness) ─
#define T1000X_SENSOR_EN
#define T1000X_SENSOR_EN_PIN (0 + 4) // P0.4, power to sensor (GPIO, not ADC)
#define T1000X_NTC_PIN (0 + 31)      // P0.31/AIN7
#define T1000X_LUX_PIN (0 + 29)      // P0.29/AIN5

#define HAS_SCREEN 0

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#endif // _VARIANT_SEEED_T1000E_
