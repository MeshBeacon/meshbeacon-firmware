/**
 * @file PhoneGpsHandler.h
 * @brief Shared parsing for the paired phone app's "CDK:GPS,LAT:...,LNG:..."
 * frame, used by MamaDuck-based example sketches (Heltec, Seeed Wio Tracker
 * L1 Pro, and any future board).
 *
 * This was previously duplicated, near byte-for-byte, in each board's own
 * handleGps() function. It only extracts fields, updates the phone-GPS
 * cache buffers, and builds the duckcdp_GpsReading protobuf struct (reusing
 * the existing buildGpsReadingFix()/buildGpsReadingNoFix() from
 * PayloadBuilders.h) -- it does not send anything or touch the display, so
 * each board's own handleGps() still decides what to do with the result
 * (set gpsTxPayload/gpsTxPending, log, etc.).
 *
 * Lives under examples/Basic-Ducks/common/ (not src/) for the same reason
 * as the other common/ headers: example-sketch glue code, not part of the
 * CDP library itself.
 */
#ifndef DUCK_COMMON_PHONE_GPS_HANDLER_H_
#define DUCK_COMMON_PHONE_GPS_HANDLER_H_

#include <Arduino.h>
#include <cstring>
#include "payloads/DuckPayloads.h"
#include "PayloadBuilders.h"
#include "CdkFrame.h"

struct PhoneGpsParseResult {
  bool hasFix;
  duckcdp_GpsReading reading;
};

// Parses a CDK:GPS frame body from the phone app. If LAT/LNG are missing or
// "none", returns hasFix=false with a GPS_REASON_NO_SIGNAL no-fix reading
// and leaves the cache buffers untouched. Otherwise updates latBuf/lngBuf
// (always) and altBuf/spdBuf/hdgBuf (cleared, then set only if the
// corresponding field was present in the frame) and returns hasFix=true
// with a fix reading built from the parsed fields.
inline PhoneGpsParseResult parsePhoneGpsFrame(const String &body,
                                               char *latBuf, size_t latBufSize,
                                               char *lngBuf, size_t lngBufSize,
                                               char *altBuf, size_t altBufSize,
                                               char *spdBuf, size_t spdBufSize,
                                               char *hdgBuf, size_t hdgBufSize,
                                               int battPct, int32_t rssiDbm) {
  PhoneGpsParseResult result;
  String lat = extractField(body, "LAT");
  String lng = extractField(body, "LNG");
  if (lat.length() == 0 || lat == "none" || lng.length() == 0 || lng == "none") {
    result.hasFix = false;
    result.reading = buildGpsReadingNoFix(duckcdp_GpsSource_GPS_SOURCE_PHONE,
        duckcdp_GpsNoFixReason_GPS_REASON_NO_SIGNAL, battPct, rssiDbm);
    return result;
  }
  String alt = extractField(body, "ALT");
  String spd = extractField(body, "SPD");
  String hdg = extractField(body, "HDG");
  result.hasFix = true;
  result.reading = buildGpsReadingFix(duckcdp_GpsSource_GPS_SOURCE_PHONE,
      lat, lng, alt, spd, hdg, battPct, rssiDbm);
  strncpy(latBuf, lat.c_str(), latBufSize - 1);
  strncpy(lngBuf, lng.c_str(), lngBufSize - 1);
  altBuf[0] = '\0';
  spdBuf[0] = '\0';
  hdgBuf[0] = '\0';
  if (alt.length() > 0) strncpy(altBuf, alt.c_str(), altBufSize - 1);
  if (spd.length() > 0) strncpy(spdBuf, spd.c_str(), spdBufSize - 1);
  if (hdg.length() > 0) strncpy(hdgBuf, hdg.c_str(), hdgBufSize - 1);
  return result;
}

#endif // DUCK_COMMON_PHONE_GPS_HANDLER_H_
