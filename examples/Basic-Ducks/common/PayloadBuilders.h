/**
 * @file PayloadBuilders.h
 * @brief Shared protobuf payload-construction helpers for MamaDuck-based
 * example sketches (Heltec, Seeed Wio Tracker L1 Pro, and any future
 * board).
 *
 * These builder functions were originally duplicated, field-for-field,
 * across each example's MamaDuck.ino (in sendEmergency()/handleSOS(),
 * handleMsg(), handleGps(), and sendMamaTalk()). They only ever assemble
 * a nanopb-generated struct from plain values already computed by the
 * caller -- no GPS/battery hardware reads, no display, no radio I/O -- so
 * they are factored out here to avoid re-typing (and re-drifting) the
 * same field-assignment logic in every new sketch.
 *
 * Every hardware-derived input (lat/lng/alt/spd/hdg strings, battery
 * percent, RSSI) is passed in by the caller exactly as it already
 * computed it, so extracting these builders changes no behavior: each
 * call site keeps deciding what value to pass (including intentionally
 * omitting one, e.g. by passing 0).
 *
 * Lives under examples/Basic-Ducks/common/ (not src/) because it is
 * example-sketch glue code, not part of the CDP library itself -- keeping
 * it out of src/ avoids mixing sketch-level conventions into the
 * upstream library's own directory structure.
 */

#ifndef DUCK_COMMON_PAYLOAD_BUILDERS_H_
#define DUCK_COMMON_PAYLOAD_BUILDERS_H_

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <Arduino.h>
#include "payloads/DuckPayloads.h"

// ── SosAlert (see duck_payloads.proto) ──────────────────────────────────
// Used by both sendEmergency() (hardware SOS button, origin=DEVICE) and
// handleSOS() (phone-triggered SOS, origin=PHONE).
inline duckcdp_SosAlert buildSosAlert(duckcdp_SosOrigin origin,
                                       duckcdp_GpsSource gpsSource,
                                       const String &lat, const String &lng,
                                       const String &alt, const String &spd,
                                       const String &hdg, int battPct,
                                       int32_t rssiDbm) {
  duckcdp_SosAlert alertMsg = duckcdp_SosAlert_init_zero;
  alertMsg.origin = origin;
  bool hasGps = (lat.length() > 0 && lng.length() > 0);
  alertMsg.has_gps = hasGps;
  if (hasGps) {
    alertMsg.gps_source = gpsSource;
    alertMsg.lat_e7 = (int32_t)lround(atof(lat.c_str()) * 1e7);
    alertMsg.lng_e7 = (int32_t)lround(atof(lng.c_str()) * 1e7);
    if (alt.length() > 0) alertMsg.alt_m = (int32_t)lround(atof(alt.c_str()));
    if (spd.length() > 0) alertMsg.spd_dkmh = (uint32_t)lround(atof(spd.c_str()) * 10);
    if (hdg.length() > 0) alertMsg.hdg_deg = (uint32_t)lround(atof(hdg.c_str()));
  } else {
    alertMsg.gps_source = duckcdp_GpsSource_GPS_SOURCE_NONE;
  }
  alertMsg.batt_pct = battPct;
  alertMsg.rssi_dbm = rssiDbm;
  return alertMsg;
}

// ── StatusMsg (see duck_payloads.proto) ─────────────────────────────────
// Used by handleMsg() (phone-composed message to LoRa, on the `status`
// topic via encodeStatusReportMsg()).
inline duckcdp_StatusMsg buildStatusMsg(const String &urgency, const String &lat,
                                         const String &lng, const String &text) {
  duckcdp_StatusMsg statusMsg = duckcdp_StatusMsg_init_zero;
  statusMsg.src = duckcdp_StatusMsgSrc_STATUS_MSG_SRC_PHONE;
  std::snprintf(statusMsg.urgency, sizeof(statusMsg.urgency), "%s", urgency.c_str());
  bool hasGps = (lat.length() > 0 && lng.length() > 0);
  statusMsg.has_gps = hasGps;
  if (hasGps) {
    statusMsg.lat_e7 = (int32_t)lround(atof(lat.c_str()) * 1e7);
    statusMsg.lng_e7 = (int32_t)lround(atof(lng.c_str()) * 1e7);
  }
  std::snprintf(statusMsg.text, sizeof(statusMsg.text), "%s", text.c_str());
  return statusMsg;
}

// ── GpsReading, valid-fix case (see duck_payloads.proto) ────────────────
// Used by handleGps() (phone GPS reply) for the "fix received" branch.
inline duckcdp_GpsReading buildGpsReadingFix(duckcdp_GpsSource source,
                                              const String &lat, const String &lng,
                                              const String &alt, const String &spd,
                                              const String &hdg, int battPct,
                                              int32_t rssiDbm) {
  duckcdp_GpsReading reading = duckcdp_GpsReading_init_zero;
  reading.has_fix = true;
  reading.source = source;
  reading.no_fix_reason = duckcdp_GpsNoFixReason_GPS_REASON_NONE;
  reading.lat_e7 = (int32_t)lround(atof(lat.c_str()) * 1e7);
  reading.lng_e7 = (int32_t)lround(atof(lng.c_str()) * 1e7);
  if (alt.length() > 0) reading.alt_m = (int32_t)lround(atof(alt.c_str()));
  if (spd.length() > 0) reading.spd_dkmh = (uint32_t)lround(atof(spd.c_str()) * 10);
  if (hdg.length() > 0) reading.hdg_deg = (uint32_t)lround(atof(hdg.c_str()));
  reading.batt_pct = battPct;
  reading.rssi_dbm = rssiDbm;
  return reading;
}

// ── GpsReading, no-fix case (see duck_payloads.proto) ────────────────────
// Used by handleGps() (phone reported no fix), the GPS-request timeout
// fallback, and the ping handler's no-response report.
inline duckcdp_GpsReading buildGpsReadingNoFix(duckcdp_GpsSource source,
                                                duckcdp_GpsNoFixReason reason,
                                                int battPct, int32_t rssiDbm) {
  duckcdp_GpsReading reading = duckcdp_GpsReading_init_zero;
  reading.has_fix = false;
  reading.source = source;
  reading.no_fix_reason = reason;
  reading.batt_pct = battPct;
  reading.rssi_dbm = rssiDbm;
  return reading;
}

// ── MTalk (see duck_payloads.proto) ─────────────────────────────────────
// Used by sendMamaTalk() (outgoing MamaDuck-to-MamaDuck chat) and the
// MTALK_ACK delivery-receipt reply sent from handleDuckData().
inline duckcdp_MTalk buildMTalk(duckcdp_MTalkKind kind, const String &mid,
                                 const String &text) {
  duckcdp_MTalk mtalk = duckcdp_MTalk_init_zero;
  mtalk.kind = kind;
  std::snprintf(mtalk.mid, sizeof(mtalk.mid), "%s", mid.c_str());
  std::snprintf(mtalk.text, sizeof(mtalk.text), "%s", text.c_str());
  return mtalk;
}

// ── OpText (see duck_payloads.proto) ────────────────────────────────────
// Used by handleDuckData()'s dcmd (22)/ALERT (23)/PMSG (25) cases to
// protobuf-encode operator-message ACK replies (e.g. "MSG_READ:TEXT:...",
// "ALERT_ACK"). Incoming OpText decode still uses
// duckpayload::decodeOpText() directly at each call site, since there's
// no field assembly to share on the decode side -- this only covers the
// encode-side construction that was duplicated verbatim.
inline duckcdp_OpText buildOpText(const String &text) {
  duckcdp_OpText opText = duckcdp_OpText_init_zero;
  std::snprintf(opText.text, sizeof(opText.text), "%s", text.c_str());
  return opText;
}

#endif // DUCK_COMMON_PAYLOAD_BUILDERS_H_
