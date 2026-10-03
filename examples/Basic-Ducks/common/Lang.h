#pragma once

// ── Shared display-language strings ────────────────────────────────────────
// UI text macros whose value is IDENTICAL between the Heltec and
// Seeed/WioTrackerL1 sketches. Each board's own Lang.h keeps its
// board-specific/compound macros locally (different display hardware needs
// different line-wrapping: Heltec's SSD1306 macros bundle multi-line "2L"/
// "3L" strings, WioTrackerL1's u8g2 wrapper macros are split into single
// lines) and includes this file for everything that's shared.
//
// Selected by the same DUCK_LANG_EN build flag used by each board's Lang.h
// (uncomment there, or add -DDUCK_LANG_EN in platformio.ini). This only
// affects on-screen text, never wire-protocol/mesh keywords.

#if defined(DUCK_LANG_EN)

  #define TXT_GPS_SENT_OK            "GPS SENT OK!"
  #define TXT_GPS_SEND_FAIL          "GPS SEND FAILED!"
  #define TXT_SRC_PHONE              "SRC:PHONE"
  #define TXT_ROGER_SENT             "ROGER SENT!"
  #define TXT_GPS_NO_MODULE          "GPS: NO MODULE"
  #define TXT_GPS_SEARCH_FMT         "GPS: SEARCH %uSAT"
  #define TXT_SENDING_GPS_DATA       "SENDING GPS DATA"
  #define TXT_SIG_WEAK               "SIG: WEAK ("
  #define TXT_SIG_OK                 "SIG: OK ("
  #define TXT_SIG_STRONG             "SIG: STRONG ("
  #define TXT_SIG_VSTRONG            "SIG: V.STRONG ("
  #define TXT_SEND_OK                "SEND OK"
  #define TXT_SEND_FAIL              "SEND FAILED"
  #define TXT_SIG_NONE               "SIG: NO SIGNAL"
  #define TXT_PRESS_BUTTON_ABOVE     "PRESS BUTTON ABOVE"
  #define TXT_2SEC_EMERGENCY         "2 SEC = EMERGENCY"
  #define TXT_HOLD_FOR_SOS           "HOLD FOR SOS"
  #define TXT_SOS_SENT_HINT_3L       "PRESS BUTTON ABOVE\nFOR 2 SECONDS FOR\nEMERGENCY SIGNAL"
  #define TXT_MSG_SENT               "MESSAGE SENT!"
  #define TXT_EMERGENCY_MESSAGE_HEADER "[EMERGENCY MESSAGE]"
  #define TXT_GPS_FIX_FMT            "GPS: FIX %uSAT"
  #define TXT_BATT_LABEL             "BATT:"

#else  // Bahasa Melayu (default)

  #define TXT_GPS_SENT_OK            "BERJAYA HANTAR GPS!"
  #define TXT_GPS_SEND_FAIL          "GAGAL HANTAR GPS!"
  #define TXT_SRC_PHONE              "SRC:TELEFON"
  #define TXT_ROGER_SENT             "ROGER DIHANTAR!"
  #define TXT_GPS_NO_MODULE          "GPS: TIADA MODUL"
  #define TXT_GPS_SEARCH_FMT         "GPS: CARI %uSAT"
  #define TXT_SENDING_GPS_DATA       "MENGHANTAR DATA GPS"
  #define TXT_SIG_WEAK               "SIG: LEMAH ("
  #define TXT_SIG_OK                 "SIG: CUKUP ("
  #define TXT_SIG_STRONG             "SIG: KUAT ("
  #define TXT_SIG_VSTRONG            "SIG: SG.KUAT ("
  #define TXT_SEND_OK                "BERJAYA HANTAR"
  #define TXT_SEND_FAIL              "GAGAL HANTAR"
  #define TXT_SIG_NONE               "SIG: TIADA ISYARAT"
  #define TXT_PRESS_BUTTON_ABOVE     "TEKAN BUTANG ATAS"
  #define TXT_2SEC_EMERGENCY         "2 SAAT = KECEMASAN"
  #define TXT_HOLD_FOR_SOS           "TAHAN UNTUK SOS"
  #define TXT_SOS_SENT_HINT_3L       "TEKAN BUTANG ATAS\nSELAMA 2 SAAT UTK\nISYARAT KECEMASAN"
  #define TXT_MSG_SENT               "MESEJ TELAH DIHANTAR!"
  #define TXT_EMERGENCY_MESSAGE_HEADER "[MESEJ KECEMASAN]"
  #define TXT_GPS_FIX_FMT            "GPS: TETAP %uSAT"
  #define TXT_BATT_LABEL             "BATT:"

#endif
