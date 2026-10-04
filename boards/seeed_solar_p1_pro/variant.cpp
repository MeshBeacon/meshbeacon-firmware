/*
 * variant.cpp - Digital pin mapping for SenseCAP Solar Node P1-Pro
 *
 * This file defines the pin mapping array that maps logical digital pins (D0-D26)
 * to physical GPIO ports/pins on the Nordic nRF52840 microcontroller.
 *
 * Board: Seeed SenseCAP Solar Node P1-Pro for MeshCore
 * Hardware Features:
 *  - LoRa module: Wio-SX1262 for XIAO (SX1262, CS/SCK/MISO/MOSI/BUSY/RESET/RXEN)
 *  - GNSS module: L76K (TX/RX/Reset/Wakeup/Enable)
 *  - White user LED (D11) + Blue LoRa-TX LED (D12)
 *  - User buttons (D13 front-panel, D20 second user-defined)
 *  - NFC/Grove interface (D14-D15, shared with I2C)
 *  - Battery voltage monitoring (D16, enable on D19, active LOW)
 *  - On-board QSPI flash (D21-D26)
 *
 * Pin roles were confirmed against meshcore-dev/MeshCore's shipped
 * variants/sensecap_solar/variant.cpp (same physical hardware -- Xiao
 * nRF52840 Plus + Wio-SX1262 + L76K), since Seeed does not publish a
 * carrier-board schematic for this product. Verify against real hardware
 * before production use.
 */

#include "variant.h"
#include "nrf.h"
#include "wiring_constants.h"
#include "wiring_digital.h"

namespace
{
void configureWakeOnPress(uint8_t digitalPin)
{
    const uint32_t gpioPin = g_ADigitalPinMap[digitalPin];
    nrf_gpio_cfg_input(gpioPin, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_sense_set(gpioPin, NRF_GPIO_PIN_SENSE_LOW);
}
} // namespace

/**
 * @brief Digital pin to GPIO port/pin mapping table
 *
 * Format: Logical Pin (Dx) -> nRF Port.Pin (Px.xx)
 *
 */

extern "C" {
const uint32_t g_ADigitalPinMap[] = {
    // D0 .. D10 - Peripheral control pins
    2,  // D0  P0.02 GNSS_WAKEUP
    3,  // D1  P0.03 LORA_DIO1
    28, // D2  P0.28 LORA_RESET
    29, // D3  P0.29 LORA_BUSY
    4,  // D4  P0.04 (A4/SDA) LORA_CS
    5,  // D5  P0.05 (A5/SCL) LORA_SW
    43, // D6  P1.11 (UART_TX) GNSS_TX
    44, // D7  P1.12 (UART_RX) GNSS_RX
    45, // D8  P1.13 (SPI_SCK) LORA_SCK
    46, // D9  P1.14 (SPI_MISO) LORA_MISO
    47, // D10 P1.15 (SPI_MOSI) LORA_MOSI

    // D11-D12 - LED outputs
    15, // D11 P0.15 White user LED
    19, // D12 P0.19 Blue LED (LoRa TX indicator)

    // D13 - User input
    33, // D13 P1.01 User Button

    // D14-D15 - Grove/NFC interface (shared with I2C)
    9,  // D14 P0.09 NFC1 / Grove SDA
    10, // D15 P0.10 NFC2 / Grove SCL

    // D16 - Battery voltage ADC input
    31, // D16 P0.31 VBAT_ADC

    // D17-D18 - GNSS control
    35, // D17 P1.03 GNSS_RESET
    37, // D18 P1.05 GNSS_ENABLE

    // D19 - Battery divider enable (active LOW)
    14, // D19 P0.14 BAT_READ (VBAT_ENABLE)

    // D20 - Second user button
    39, // D20 P1.07 USER_BUTTON

    // D21-D26 - On-board QSPI flash
    21, // D21 P0.21 (QSPI_SCK)
    25, // D22 P0.25 (QSPI_CSN)
    20, // D23 P0.20 (QSPI_SIO_0 DI)
    24, // D24 P0.24 (QSPI_SIO_1 DO)
    22, // D25 P0.22 (QSPI_SIO_2 WP)
    23, // D26 P0.23 (QSPI_SIO_3 HOLD)
};
}

void initVariant()
{
    pinMode(PIN_QSPI_CS, OUTPUT);
    digitalWrite(PIN_QSPI_CS, HIGH);

    // VBAT_ENABLE / BAT_READ is active LOW on this board (opposite polarity
    // from WioTrackerL1's active-HIGH divider enable) -- drive LOW to enable
    // the battery voltage divider for reading.
    pinMode(BAT_READ, OUTPUT);
    digitalWrite(BAT_READ, LOW);

    // GPS_EN is active-HIGH on this board (confirmed via MeshCore's
    // MicroNMEALocationProvider.h: GPS_EN_ACTIVE defaults to HIGH, and
    // sensecap_solar/variant.h never overrides it) -- drive HIGH to enable
    // the GNSS module. Driving it LOW (as MeshCore's own initVariant() does
    // transiently before LocationProvider::begin() runs) holds the module in
    // reset/powered-down.
    pinMode(GPS_EN, OUTPUT);
    digitalWrite(GPS_EN, HIGH);

    pinMode(PIN_LED1, OUTPUT);
    digitalWrite(PIN_LED1, LOW);
    pinMode(PIN_LED2, OUTPUT);
    digitalWrite(PIN_LED2, LOW);
}

void variant_shutdown()
{
    configureWakeOnPress(CANCEL_BUTTON_PIN);
}
