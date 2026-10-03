/**
 * @file DeferredGpsTx.h
 * @brief Shared "flush after duck.run()" deferred-transmit helpers for
 * MamaDuck-based example sketches (Heltec, Seeed Wio Tracker L1 Pro, and
 * any future board).
 *
 * Both boards' loop() bodies run a couple of small state machines AFTER
 * duck.run() so LoRa transmits don't race the relay's stale TX_DONE
 * interrupt (see the comments at each call site for the full explanation):
 *   - a one-shot "deferred GPS/telemetry LoRa TX" flag+payload
 *   - a two-stage "arm a 350 ms relay-clear delay, then send BEACON_ACK"
 *     timer
 * These were previously duplicated verbatim (aside from minor log-text
 * differences) in both boards' loop(). Extracted here since the logic is
 * pure control-flow over caller-owned state (no board-specific hardware
 * calls) -- callers still own the actual state variables and still decide
 * what to do with the results (encrypt+send BEACON_ACK, log, etc.).
 *
 * Lives under examples/Basic-Ducks/common/ (not src/) for the same reason
 * as the other common/ headers: example-sketch glue code, not part of the
 * CDP library itself.
 */
#ifndef DUCK_COMMON_DEFERRED_GPS_TX_H_
#define DUCK_COMMON_DEFERRED_GPS_TX_H_

#include <Arduino.h>
#include <string>
#include <vector>
#include <cstdint>
#include "UplinkRouter.h"
#include "CDP.h"

// If a GPS/telemetry payload is pending, sends it on topics::gps and clears
// the pending flag. Must be called after duck.run() (see file comment).
// Writes the send result (true = success) to *loraOk when non-null.
// Returns true iff a send was attempted this call.
inline bool flushDeferredGpsTx(volatile bool &gpsTxPending,
                                const std::vector<uint8_t> &gpsTxPayload,
                                volatile bool *loraOk = nullptr) {
  if (!gpsTxPending) return false;
  gpsTxPending = false;
  int result = sendUplink(topics::gps, std::string(
      reinterpret_cast<const char *>(gpsTxPayload.data()), gpsTxPayload.size()));
  if (loraOk) *loraOk = (result == 0);
  return true;
}

// Arms the 350 ms relay-clear delay the first time beaconAckPending is seen
// set. Safe to call every loop() iteration; no-op once armed or if nothing
// is pending.
inline void armBeaconAckIfPending(volatile bool &beaconAckPending,
                                   unsigned long &beaconAckDeferMs) {
  if (beaconAckPending && beaconAckDeferMs == 0) {
    beaconAckDeferMs = millis() + 350;
  }
}

// Returns true (once) when the relay-clear delay has elapsed and no GPS TX
// is pending, clearing both the delay and the pending flag so the caller
// can encrypt+send its BEACON_ACK payload exactly once.
inline bool beaconAckReadyToSend(unsigned long &beaconAckDeferMs,
                                  volatile bool &beaconAckPending,
                                  volatile bool gpsTxPending) {
  if (beaconAckDeferMs > 0 && millis() >= beaconAckDeferMs && !gpsTxPending) {
    beaconAckDeferMs = 0;
    beaconAckPending = false;
    return true;
  }
  return false;
}

#endif // DUCK_COMMON_DEFERRED_GPS_TX_H_
