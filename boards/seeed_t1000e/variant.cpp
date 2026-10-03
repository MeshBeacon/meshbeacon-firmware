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

#include "variant.h"
#include "nrf.h"
#include "wiring_constants.h"
#include "wiring_digital.h"

// Identity mapping: Arduino "digital pin number" N == raw nRF52840 pin
// number N (P0.00-P0.31 -> 0-31, P1.00-P1.15 -> 32-47). All pin macros in
// variant.h are already raw nRF pin numbers, so no remapping is needed here
// (unlike seeed_wio_tracker_l1's variant, which uses a shuffled D0..D30
// table instead).
const uint32_t g_ADigitalPinMap[] = {
    // P0
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    // P1
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};

void initVariant()
{
    pinMode(PIN_3V3_EN, OUTPUT);
    digitalWrite(PIN_3V3_EN, HIGH);

    pinMode(PIN_3V3_ACC_EN, OUTPUT);
    digitalWrite(PIN_3V3_ACC_EN, HIGH);

    pinMode(T1000X_SENSOR_EN_PIN, OUTPUT);
    digitalWrite(T1000X_SENSOR_EN_PIN, HIGH);

    pinMode(BUZZER_EN_PIN, OUTPUT);
    digitalWrite(BUZZER_EN_PIN, HIGH);

    // GNSS enable/reset sequencing -- must happen before Serial1.begin() in
    // the sketch's setup() so the Airoha GNSS module is out of reset and
    // powered when NMEA parsing starts.
    pinMode(PIN_GPS_EN, OUTPUT);
    digitalWrite(PIN_GPS_EN, GPS_EN_ACTIVE);

    pinMode(GPS_VRTC_EN, OUTPUT);
    digitalWrite(GPS_VRTC_EN, HIGH);

    pinMode(PIN_GPS_RESET, OUTPUT);
    digitalWrite(PIN_GPS_RESET, !GPS_RESET_MODE);

    pinMode(GPS_SLEEP_INT, OUTPUT);
    digitalWrite(GPS_SLEEP_INT, HIGH);

    pinMode(GPS_RTC_INT, OUTPUT);
    digitalWrite(GPS_RTC_INT, LOW);

    pinMode(GPS_RESETB_OUT, INPUT_PULLUP);
}
