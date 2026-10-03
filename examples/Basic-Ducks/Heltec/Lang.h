#pragma once

// ── Display language selection ─────────────────────────────────────────────
// All on-screen (OLED) text in this sketch defaults to Bahasa Melayu. To
// compile with English UI text instead, either:
//   1) uncomment the #define below, or
//   2) add a build flag in platformio.ini:  -DDUCK_LANG_EN
//
// NOTE: This only affects text shown on the display. It does NOT change any
// wire-protocol / mesh keywords (e.g. the "SOS DITERIMA" content check and
// "CDK:SOS_ACK,TEXT:SOS DITERIMA" broadcast), which are a fixed contract with
// the external OpenDMS system and must remain unchanged regardless of the
// sketch's display language.

// #define DUCK_LANG_EN

// Macros shared byte-for-byte with the WioTrackerL1 sketch (identical EN/BM
// text) live in common/Lang.h; only Heltec-specific strings are defined here.
#include "../common/Lang.h"

#if defined(DUCK_LANG_EN)

  #define TXT_BLE_ADV_FAIL           "BLE ERROR\nCANNOT ADVERTISE"
  #define TXT_PHONE_GPS_NO_SIGNAL_2L "PHONE GPS\nNO SIGNAL"
  #define TXT_BT_CONNECTED           "BLUETOOTH\nCONNECTED!"
  #define TXT_BT_DISCONNECTED        "BLUETOOTH\nDISCONNECTED"
  #define TXT_USB_CONNECTED          "USB SERIAL\nCONNECTED!"
  #define TXT_USB_DISCONNECTED       "USB SERIAL\nDISCONNECTED"
  #define TXT_SOS_ACK_DISPLAY        "SOS RECEIVED!\nHELP IS BEING\nSENT"
  #define TXT_SENDING_SOS_2L         "SENDING\nEMERGENCY SIGNAL..."
  #define TXT_REQ_GPS_FROM_PHONE_2L  "REQUESTING GPS\nFROM PHONE..."
  #define TXT_SOS_CANCELLED          "SOS CANCELLED"
  #define TXT_DATETIME_NO_SIGNAL_2L  "DATE/TIME\nNO SIGNAL"
  #define TXT_GPS_MODULE_ACTIVE_2L   "GPS: MODULE ACTIVE\nWAITING FOR SIGNAL..."
  #define TXT_REQ_GPS_DATA_FROM_PHONE_2L "REQUESTING GPS DATA\nFROM PHONE..."
  #define TXT_NO_PHONE_NO_GPS_2L     "NO PHONE\nNO GPS DATA"
  #define TXT_HOME_HINT_3L           "PRESS BUTTON ABOVE\nFOR TWO SECONDS FOR\nEMERGENCY SIGNAL"
  #define TXT_SOS_SENT_GPS_3L        "SENT OK\nEMERGENCY SIGNAL\nWITH GPS!"
  #define TXT_SOS_SENT_NOGPS_3L      "SENT OK\nEMERGENCY SIGNAL\nWITHOUT GPS!"
  #define TXT_SOS_ERR_2L             "ERROR. CANNOT\nSEND EMERGENCY SIGNAL"

#else  // Bahasa Melayu (default)

  #define TXT_BLE_ADV_FAIL           "RALAT BLE\nTIDAK BOLEH IKLAN"
  #define TXT_PHONE_GPS_NO_SIGNAL_2L "GPS TELEFON\nTIADA ISYARAT"
  #define TXT_BT_CONNECTED           "BLUETOOTH\nTERSAMBUNG!"
  #define TXT_BT_DISCONNECTED        "BLUETOOTH\nTERPUTUS"
  #define TXT_USB_CONNECTED          "USB BERSIRI\nTERSAMBUNG!"
  #define TXT_USB_DISCONNECTED       "USB BERSIRI\nTERPUTUS"
  #define TXT_SOS_ACK_DISPLAY        "SOS DITERIMA!\nBANTUAN SEDANG\nDIHANTAR"
  #define TXT_SENDING_SOS_2L         "SEDANG HANTAR\nISYARAT KECEMASAN..."
  #define TXT_REQ_GPS_FROM_PHONE_2L  "MEMINTA GPS\nDARIPADA TELEFON..."
  #define TXT_SOS_CANCELLED          "SOS DIBATALKAN"
  #define TXT_DATETIME_NO_SIGNAL_2L  "TARIKH/MASA\nTIADA ISYARAT"
  #define TXT_GPS_MODULE_ACTIVE_2L   "GPS: MODUL AKTIF\nMENUNGGU ISYARAT..."
  #define TXT_REQ_GPS_DATA_FROM_PHONE_2L "MEMINTA DATA GPS\nDARIPADA TELEFON..."
  #define TXT_NO_PHONE_NO_GPS_2L     "TIADA TELEFON\nTIADA DATA GPS"
  #define TXT_HOME_HINT_3L           "TEKAN BUTANG ATAS\nSELAMA DUA SAAT UTK\nISYARAT KECEMASAN"
  #define TXT_SOS_SENT_GPS_3L        "BERJAYA HANTAR\nISYARAT KECEMASAN\nDENGAN GPS!"
  #define TXT_SOS_SENT_NOGPS_3L      "BERJAYA HANTAR\nISYARAT KECEMASAN\nTANPA GPS!"
  #define TXT_SOS_ERR_2L             "RALAT. TIDAK BOLEH\nHANTAR ISYARAT KECEMASAN"

#endif

