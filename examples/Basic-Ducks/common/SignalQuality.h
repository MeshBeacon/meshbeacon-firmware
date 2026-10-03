/**
 * @file SignalQuality.h
 * @brief Shared signal-quality label bucketing for MamaDuck-based example
 * sketches (Heltec, Seeed Wio Tracker L1 Pro, and any future board).
 *
 * Both boards' displayHome() screens map the same lastSignalPct/
 * lastTxResult state into one of the same seven TXT_SIG_* / TXT_SEND_*
 * labels (see common/Lang.h) using identical thresholds. Extracted here
 * since it's pure text-selection logic with no display/hardware
 * dependency -- callers still own drawing the returned string.
 *
 * Lives under examples/Basic-Ducks/common/ (not src/) for the same reason
 * as the other common/ headers: example-sketch glue code, not part of the
 * CDP library itself.
 */
#ifndef DUCK_COMMON_SIGNAL_QUALITY_H_
#define DUCK_COMMON_SIGNAL_QUALITY_H_

#include <Arduino.h>
#include "Lang.h"

// Maps received-packet signal quality (0-100, or negative if no packet has
// been received yet) and the last TX result (0 = ok, >0 = failed, <0 =
// none sent yet) to the on-screen signal/status string, e.g.
// "SIG: STRONG (80%)" or "SEND FAILED".
inline String signalQualityLabel(int lastSignalPct, int lastTxResult) {
  if      (lastSignalPct >= 0 && lastSignalPct <= 25) return TXT_SIG_WEAK    + String(lastSignalPct) + "%)";
  else if (lastSignalPct >= 0 && lastSignalPct <= 50) return TXT_SIG_OK     + String(lastSignalPct) + "%)";
  else if (lastSignalPct >= 0 && lastSignalPct <= 75) return TXT_SIG_STRONG + String(lastSignalPct) + "%)";
  else if (lastSignalPct >  75)                       return TXT_SIG_VSTRONG + String(lastSignalPct) + "%)";
  else if (lastTxResult == 0)                         return TXT_SEND_OK;
  else if (lastTxResult >  0)                         return TXT_SEND_FAIL;
  else                                                return TXT_SIG_NONE;
}

#endif // DUCK_COMMON_SIGNAL_QUALITY_H_
