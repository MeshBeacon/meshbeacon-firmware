/**
 * @file DuckDisplayAdapter.h
 * @brief Hardware-agnostic OLED drawing interface shared between board
 * sketches (Heltec/SSD1306 via heltec_unofficial, Seeed Wio Tracker L1/SH1106
 * via U8g2 -- two completely different display libraries).
 *
 * Each board implements this interface once against its own native display
 * object/library (see HeltecDisplayAdapter in Heltec/MamaDuck.ino and
 * WioDisplayAdapter in Seeed/WioTrackerL1/MamaDuck.ino). Only the ~6 screen
 * functions that were byte-for-byte duplicated between boards
 * (displayHome/displayID/displayBatt/displayMessage/displayAnnouncement/
 * showHoldProgress, plus the boot-splash bitmap draw) have been converted to
 * go through this interface -- the many one-off inline status screens
 * scattered through each sketch (BLE/USB connect messages, GPS-sent
 * confirmations, SOS flow screens, etc.) still call their board's native
 * display API/helpers directly and were intentionally left untouched.
 *
 * This is deliberately just an abstraction over DRAWING PRIMITIVES, not a
 * merge of the screen layouts themselves: each board keeps its own
 * displayHome()/etc. function bodies (with their own Y-coordinate spacing,
 * which differs because the two boards use different fonts with different
 * glyph metrics) -- only the hardware-specific API calls inside those
 * bodies were swapped for calls through this interface.
 *
 * All y-coordinates are measured from the TOP of the glyph (not the
 * baseline), matching the convention Wio Tracker L1's existing dspStr()
 * family of helpers already used.
 */
#ifndef DUCK_COMMON_DISPLAY_ADAPTER_H_
#define DUCK_COMMON_DISPLAY_ADAPTER_H_

#include <Arduino.h>
#include <cstdint>

class IDuckDisplay {
public:
  virtual ~IDuckDisplay() {}

  // Clear the back buffer. Must be paired with end() to flush to hardware.
  virtual void begin() = 0;
  // Flush the back buffer to the physical screen.
  virtual void end() = 0;

  // Draw a left-aligned string with its top-left corner at (x, yTop).
  virtual void drawStr(int x, int yTop, const char *s) = 0;
  // Draw a horizontally-centered string at top row yTop.
  virtual void drawStrCenter(int yTop, const char *s) = 0;
  // Draw a right-aligned string at top row yTop.
  virtual void drawStrRight(int yTop, const char *s) = 0;
  // Draw a string that may wrap across multiple lines starting at
  // (x, yTop), constrained to maxWidth pixels.
  virtual void drawStrMaxWidth(int x, int yTop, int maxWidth, const char *s) = 0;

  // Draw a packed XBM bitmap at (x, y).
  virtual void drawXBM(int x, int y, int w, int h, const uint8_t *bits) = 0;
  // Draw a filled progress bar (0-100%) inside the given frame.
  virtual void drawProgressBar(int x, int y, int w, int h, uint8_t pct) = 0;

  // Toggle the physical panel's power-save/dim mode. true = screen off/dim.
  virtual void powerSave(bool on) = 0;
};

#endif // DUCK_COMMON_DISPLAY_ADAPTER_H_
