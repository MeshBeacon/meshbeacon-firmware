/**
 * @file MamaDuck.ino — Seeed SenseCAP Solar Node P1-Pro (nRF52840 + SX1262)
 * @brief MamaDuck example using ClusterDuck Protocol on the nRF52840 platform.
 *
 * Hardware: Seeed SenseCAP Solar Node P1-Pro for MeshCore
 *   - MCU:     nRF52840 @ 64 MHz (Xiao nRF52840 Plus)
 *   - Radio:   SX1262 (SPI, TCXO 1.8 V, DIO2 RF switch) -- same Wio-SX1262
 *              add-on module as the Wio Tracker L1 Pro.
 *   - Display: none (outdoor solar node/repeater; status via LEDs only)
 *   - GPS:     L76K NMEA (Serial1, 9600 baud)
 *   - Power:   5W solar panel + 4x 18650 battery, no buzzer.
 *
 * Ported from examples/Basic-Ducks/Seeed/WioTrackerL1/MamaDuck.ino. Platform
 * differences from that board:
 *   - No display: all dsp*() helpers and the IDuckDisplay implementation are
 *     no-ops (NullDisplayAdapter, same pattern as the T1000E port) -- this
 *     board has no OLED.
 *   - No buzzer: this board's hardware (per Seeed's spec sheet: power/reset/
 *     user-defined buttons, 4 status LEDs) has no buzzer, unlike
 *     WioTrackerL1's D12 passive buzzer. beepBuzzer() is a no-op; every
 *     call site elsewhere in this file is unchanged (audible feedback is
 *     simply absent on this board -- LED blinks still provide visual
 *     feedback for the same events).
 *   - BAT_READ (VBAT_ENABLE) is active LOW on this board (opposite polarity
 *     from WioTrackerL1's active-HIGH divider enable), and is already
 *     permanently asserted enabled by variant.cpp's initVariant() at boot
 *     -- readVbat() here does not toggle it per-read (see readVbat()).
 *   - LEDs are on P0 (PIN_LED1=P0.15, PIN_LED2=P0.19), not P1 like
 *     WioTrackerL1 -- BLINK_LED/HardFault_Handler raw-register blink
 *     helpers use NRF_P0 instead of NRF_P1.
 *   - CANCEL_BUTTON_ACTIVE_PULLUP is false for this board (external pull
 *     assumed) -- the button pin is configured as plain INPUT, not
 *     INPUT_PULLUP.
 *   - Pin roles were cross-referenced from meshcore-dev/MeshCore's shipped
 *     variants/sensecap_solar/ port (same physical hardware), since Seeed
 *     does not publish a carrier-board schematic for this product -- see
 *     boards/seeed_solar_p1_pro/variant.h for details.
 *
 * @date 2026-10-04
 */

#include <string>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <CDP.h>
#include <TinyGPSPlus.h>
#include <bluefruit.h>   // Adafruit Bluefruit52Lib — Nordic UART Service

#include "../../common/BrandingLogo.h"
#include "Lang.h"
#include "payloads/DuckPayloads.h"
#include "../../common/DuckIdFactory.h"
#include "../../common/BeaconCrypto.h"
#include "../../common/UplinkRouter.h"
#include "../../common/CdkFrame.h"
#include "../../common/PayloadBuilders.h"
#include "../../common/DuckDisplayAdapter.h"
#include "../../common/PhoneGpsHandler.h"
#include "../../common/DeferredGpsTx.h"
#include "../../common/SignalQuality.h"
#include "security/SecurityEventCounters.h"

// ADC_RESOLUTION is not defined in this board's variant.h; 14-bit gives
// full-scale 16383 and must match the analogReadResolution(14) call in setup().
#ifndef ADC_RESOLUTION
#  define ADC_RESOLUTION 14
#endif

// Declare here (before any function definition in this file, including
// currentRssiDbm() just below) so Arduino's auto-prototype generator --
// which inserts all forward-declared function prototypes immediately
// before the first function definition it finds -- sees this enum before
// any prototype that returns/takes it (e.g. checkButton() further down).
enum BtnEvent { BTN_NONE, BTN_SINGLE, BTN_DOUBLE, BTN_TRIPLE, BTN_QUAD, BTN_HOLD_2S };

// Access the RadioLib radio instance from DuckLoRa.cpp to read RSSI/SNR.
extern CDPCFG_LORA_CLASS lora;

// Current RSSI (dBm) of the last LoRa packet received by this duck's radio,
// rounded to the nearest integer. Included in GPS-request responses and SOS
// alerts so OpenDMS/the gateway gets a rough mesh-link-quality signal
// alongside the location report.
static int32_t currentRssiDbm() {
  return (int32_t)lround(lora.getRSSI());
}

// ── Board sanity check ────────────────────────────────────────────────────────
#ifndef ARDUINO_SEEED_SOLAR_P1_PRO
#error "This sketch is for the Seeed SenseCAP Solar Node P1-Pro. \
Define ARDUINO_SEEED_SOLAR_P1_PRO (or use env:local_solar_p1_pro)."
#endif

// ── Identification ────────────────────────────────────────────────────────────
// Duck ID: MUST be exactly 8 bytes and unique on the mesh.
// To pin a fixed, human-readable ID, `#define DUCK_ID "MYDUCK01"` above this
// line (exactly 8 characters). If DUCK_ID is left undefined, one is
// auto-derived below from this board's factory-unique BLE device address
// (see duckesp::getDuckMacAddress()), so every device gets a distinct,
// reboot-stable ID with no manual configuration required.
// NOTE: the ID-generation function itself lives further down (after enum
// BtnEvent) so it isn't the first function definition in the file --
// Arduino's ctags-based prototype generator inserts all forward-declared
// prototypes right before the first function definition it finds, and
// those prototypes must come after BtnEvent's declaration.
static char DUCK_ID_BUF[9] = {0};
#define DUCK_NAME DUCK_ID_BUF   // kept so the rest of this sketch is unchanged

// ── GPS ───────────────────────────────────────────────────────────────────────
static TinyGPSPlus tinyGps;
static bool gpsModuleDetected = false;
static bool gpsFix            = false;

// ── Display ───────────────────────────────────────────────────────────────────
// This board has no display of any kind (HAS_SCREEN 0 in variant.h; outdoor
// solar node/repeater, status is conveyed via LEDs only). All the dsp*()
// helpers and the IDuckDisplay implementation below are no-ops so the rest
// of this sketch (ported from the OLED-equipped WioTrackerL1) compiles and
// runs unchanged -- every dspStr()/dspBegin()/etc. call elsewhere in this
// file is a harmless no-op on this hardware.

// enum BtnEvent is declared near the top of the file (right before
// currentRssiDbm()) so it precedes every function definition, per
// Arduino's auto-prototype-generator ordering requirement explained there.

// Fills DUCK_ID_BUF with either the user-defined DUCK_ID literal or an
// 8-char ID auto-derived from this board's factory-unique BLE device
// address. Called once, below, before DUCK_ID_BUF's first use.
static bool initDuckId() {
#ifdef DUCK_ID
    strncpy(DUCK_ID_BUF, DUCK_ID, 8);
    DUCK_ID_BUF[8] = '\0';
#else
    duckidfactory::deriveFromMac(DUCK_ID_BUF);
#endif
    return true;
}
static bool duckIdReady = initDuckId();

static bool gDisplayOk = false;   // always false: no display hardware present

static inline void dspStr(int /*x*/, int /*y*/, const char* /*s*/) {}
static inline void dspStrCenter(int /*y*/, const char* /*s*/) {}
static inline void dspStrRight(int /*y*/, const char* /*s*/) {}
static void dspMulti(int /*x*/, int /*y*/, const char* /*text*/) {}
static void dspBegin() {}
static void dspEnd() {}
static inline void dspPowerSave(uint8_t /*on*/) {}

// No-op implementation of the shared IDuckDisplay interface (see
// common/DuckDisplayAdapter.h) for this display-less board.
class NullDisplayAdapter : public IDuckDisplay {
public:
  void begin() override {}
  void end() override {}
  void drawStr(int /*x*/, int /*yTop*/, const char * /*s*/) override {}
  void drawStrCenter(int /*yTop*/, const char * /*s*/) override {}
  void drawStrRight(int /*yTop*/, const char * /*s*/) override {}
  void drawStrMaxWidth(int /*x*/, int /*yTop*/, int /*maxWidth*/, const char * /*s*/) override {}
  void drawXBM(int /*x*/, int /*y*/, int /*w*/, int /*h*/, const uint8_t * /*bits*/) override {}
  void drawProgressBar(int /*x*/, int /*y*/, int /*w*/, int /*h*/, uint8_t /*pct*/) override {}
  void powerSave(bool /*on*/) override {}
};
static NullDisplayAdapter gDisplay;

// No-op: no display hardware to initialise on this board.
static void initDisplay() {}

// Show 1-2 centred status lines. No-op on this board (no display hardware).
static void dspStatus(const char* /*line1*/, const char* /*line2*/ = nullptr) {}

// ── BLE state ────────────────────────────────────────────────────────────────
// Modelled directly on MeshCore's SerialBLEInterface (nrf52/SerialBLEInterface.cpp).
// No PIN/pairing — open NUS (Nordic UART Service).  BLE is initialised last in
// setup(), after all other hardware, mirroring MeshCore's setup() ordering.
static BLEUart  bleuart;
static bool     blePhoneSeen          = false;
static String   bleInBuf              = "";
static volatile bool bleRxPending     = false;
static char     bleRxLine[512]        = {};
static bool     bleAdvertisingStarted = false;
static unsigned long lastBleHealthMs  = 0;
#define BLE_HEALTH_INTERVAL_MS 10000UL



// BLE connection params (from MeshCore): units 1.25 ms / 10 ms.
#define BLE_MIN_CONN_INTERVAL  12   // 15 ms
#define BLE_MAX_CONN_INTERVAL  24   // 30 ms
#define BLE_SLAVE_LATENCY       4
#define BLE_CONN_SUP_TIMEOUT  200   // 2000 ms

static void setupBLE();
static void bleSendLine(const String& line);

// ── USB Serial state ──────────────────────────────────────────────────────────
static String         usbInBuf            = "";
static unsigned long  lastUsbRxMs         = 0;
static bool           usbPhoneSeen        = false;
const  unsigned long  USB_IDLE_TIMEOUT_MS = 30000UL;

// ── Display state ─────────────────────────────────────────────────────────────
static bool           displayEnabled      = true;

// ── Deferred / pending display flags ─────────────────────────────────────────
static volatile bool  phoneGpsDisplayPending     = false;
static volatile bool  phoneGpsNoFix              = false;
static volatile bool  gpsLoraOk                  = false;
static volatile bool  gpsTxPending               = false;
static volatile bool  usbConnectDisplayPending   = false;
static volatile bool  usbDisconnectDisplayPending= false;
static volatile bool  sosAckDisplayPending       = false;

// ── SOS state (non-blocking) ─────────────────────────────────────────────────
// Set when the 2s-hold has fired but we're waiting (asynchronously) on a
// phone GPS reply before actually transmitting. A single click while this is
// true cancels the pending SOS (see BTN_SINGLE handling in loop()).
static bool           sosPending      = false;
static unsigned long  sosWaitStartMs  = 0;
const  unsigned long  SOS_GPS_WAIT_MS = 2500UL;
const  uint32_t       SOS_HOLD_MS     = 2000UL;   // shared with checkButton()'s hold detection

// ── GPS TX payload and phone GPS cache ───────────────────────────────────────
static std::vector<uint8_t> gpsTxPayload;   // encoded protobuf payload for deferred GPS TX
static char phoneGpsLatBuf[20] = {};
static char phoneGpsLngBuf[20] = {};
static char phoneGpsAltBuf[12] = {};
static char phoneGpsSpdBuf[12] = {};
static char phoneGpsHdgBuf[12] = {};
static unsigned long gpsReqSentMs         = 0;
static unsigned long gpsReqDeferredSendMs = 0;
static unsigned long gpsDisplayClearMs    = 0;

// ── Signal / TX tracking ─────────────────────────────────────────────────────
static int           lastSignalPct        = -1;
static int           lastTxResult         = -1;
static unsigned long lastTxMs             = 0;
static unsigned long lastHomeRefreshMs    = 0;
const  unsigned long HOME_REFRESH_MS      = 5000UL;

// ── Message display ───────────────────────────────────────────────────────────
static bool          messagePending          = false;
static bool          emergencyDisplayPending = false;
static unsigned long sosAckUntilMs          = 0;    // epoch when SOS ACK screen auto-dismisses (0 = not active)
const  unsigned long SOS_ACK_DISPLAY_MS     = 10000UL;

// Beep requests from duck.run() callbacks are deferred here and executed in
// loop() after duck.run() returns, avoiding recursive duck.run() deadlocks.
// (On this board beepBuzzer() is a no-op -- see below -- but the deferral
// mechanism is kept so the rest of the ported code is unchanged.)
static struct { int times; int onMs; int offMs; } gBeepReq = {};

// ── Battery ───────────────────────────────────────────────────────────────────
static unsigned long lastBattMs  = 0;

// ── Per-duck GPS cache ────────────────────────────────────────────────────────
struct DuckGps { float lat; float lng; unsigned long tsMs; };
static std::map<String, DuckGps> duckGpsCache;
constexpr unsigned long DUCK_GPS_TTL_MS = 300000UL;   // 5 minutes

// ── Custom discovery topics (BEACON / BEACON_ACK) ────────────────────────────
static const uint8_t  TOPIC_BEACON      = 0x20;  // 32 -- was 27 (collided with topics::encrypted_cmd=0x1B)
static const uint8_t  TOPIC_BEACON_ACK  = 0x21;  // 33 -- was 28 (collided with topics::sealed_uplink=0x1C)
static volatile bool  beaconAckPending  = false;
static char           beaconAckPayload[80] = {};
static unsigned long  beaconAckDeferMs  = 0;

// ── Duck instance ─────────────────────────────────────────────────────────────
MamaDuck<DuckWifiNone, DuckLoRa> duck(DUCK_NAME);

// encryptBeaconPayload()/decryptBeaconPayload()/verifyBroadcastMac() now
// live in examples/Basic-Ducks/common/BeaconCrypto.h (shared with Heltec
// and future boards).

static bool setupOK = false;
static int  counter = 1;
static char idBuf[12];    // "ID:IBRAHIM1\0" header string shown on screen

// sendUplink()/sendUplinkSos()/sendMamaLink()/announcedIdentityTo now live
// in examples/Basic-Ducks/common/UplinkRouter.h (shared with Heltec and
// future boards).

// ── Function declarations ─────────────────────────────────────────────────────
void handleDuckData(CdpPacket packet);
void displayMessage(String msg);
void displayAnnouncement(const String& msg);
void displayHome();
void displayID();
void displayBatt();
void flashLED();
void handleFrame(const String& line);
void broadcast(const String& frame);
void sendBattery();
void handleSOS(const String& body);
void handleMsg(const String& body);
void handleMamaTalk(const String& body);
bool sendMamaTalk(const String& targetId, const String& msg, const String& mid = "");
void handleGps(const String& body);
void handleRadioRegion(const String& body);
void handleGpsRequestCommand();
void blinkLed(int times);
void beepBuzzer(int times, int onMs = 100, int offMs = 100);
bool sendEmergency(String lat = "", String lng = "", String alt = "",
                   String spd = "", String hdg = "", bool gpsFromPhone = false);
static float readVbat();
static int batteryPercent(float vbat);
static bool isPhoneConnected();

// ── SVC dispatch & fault handling ────────────────────────────────────────────
// Strong SVC_Handler overrides the BSP's weak vPortSVCHandler (port.c patched).
// Routes: MSP path / SVC 0 → FreeRTOS first-task restore (vPortStartFirstTask)
//         PSP path, SVC >0 → SoftDevice handler at *(0x102C)
// After sd_softdevice_enable(), VTOR=0x00000000 (MBR routing); all subsequent
// SD SVC calls go MBR→SD directly — no RAM VT needed.
extern "C" { extern void * volatile pxCurrentTCB; }

extern "C" __attribute__((naked, used)) void SVC_Handler(void) {
    __asm volatile (
        "tst   lr, #0x04         \n"   // EXC_RETURN bit2: 0=MSP, 1=PSP
        "beq   1f                \n"   // MSP → FreeRTOS first-task start
        "mrs   r0, psp           \n"
        "ldr   r1, [r0, #0x18]   \n"   // stacked PC
        "ldrb  r2, [r1, #-2]     \n"   // SVC immediate byte
        "cmp   r2, #0            \n"
        "bne   2f                \n"   // SVC>0 → SD
        "1:                      \n"
        "ldr   r3, =pxCurrentTCB \n"
        "ldr   r1, [r3]          \n"
        "ldr   r0, [r1]          \n"
        "ldmia r0!, {r4-r11, r14}\n"
        "msr   psp, r0           \n"
        "isb                     \n"
        "mov   r0, #0            \n"
        "msr   basepri, r0       \n"
        "bx    r14               \n"
        "2:                      \n"
        "ldr   r1, =0x102C       \n"
        "ldr   r1, [r1]          \n"
        "bx    r1               \n"
    );
}

// Override BSP's HardFault_Handler (NVIC_SystemReset) with SOS LED blinks
// so a fault is visible without a serial monitor.  debug.cpp patched weak.
// PIN_LED1 is raw pin 15 = P0.15 on this board (unlike WioTrackerL1's P1.01).
extern "C" void HardFault_Handler(void) {
    NRF_P0->DIRSET = (1u << 15);
    while (true) {
        for (int i=0;i<3;i++){NRF_P0->OUTSET=(1u<<15);for(volatile uint32_t d=0;d<1920000u;d++){}NRF_P0->OUTCLR=(1u<<15);for(volatile uint32_t d=0;d<1920000u;d++){}}
        for(volatile uint32_t d=0;d<3840000u;d++){}
        for (int i=0;i<3;i++){NRF_P0->OUTSET=(1u<<15);for(volatile uint32_t d=0;d<6400000u;d++){}NRF_P0->OUTCLR=(1u<<15);for(volatile uint32_t d=0;d<3200000u;d++){}}
        for(volatile uint32_t d=0;d<3840000u;d++){}
        for (int i=0;i<3;i++){NRF_P0->OUTSET=(1u<<15);for(volatile uint32_t d=0;d<1920000u;d++){}NRF_P0->OUTCLR=(1u<<15);for(volatile uint32_t d=0;d<1920000u;d++){}}
        for(volatile uint32_t d=0;d<9600000u;d++){}
    }
}

static void ble_on_connect(uint16_t /*conn_handle*/) {
    blePhoneSeen = true;
    usbConnectDisplayPending = true;
    Serial.println("[BLE] connected"); Serial.flush();
    broadcast(String("CDK:ID,VALUE:") + DUCK_ID_BUF);
    sendBattery();
}

static void ble_on_disconnect(uint16_t /*conn_handle*/, uint8_t reason) {
    blePhoneSeen = false;
    usbDisconnectDisplayPending = true;
    bleInBuf = "";
    Serial.printf("[BLE] disconnected reason=0x%02X\n", reason); Serial.flush();
}

static void ble_uart_rx_cb(uint16_t /*conn_handle*/) {
    // Called from BLE task context — buffer the line, set flag for loop().
    while (bleuart.available()) {
        char c = (char)bleuart.read();
        if (c == '\n') {
            if (bleInBuf.length() > 0) {
                bleInBuf.toCharArray(bleRxLine, sizeof(bleRxLine));
                bleRxPending = true;
            }
            bleInBuf = "";
        } else if (c != '\r') {
            if (bleInBuf.length() < (sizeof(bleRxLine) - 2))
                bleInBuf += c;
        }
    }
}

static void bleSendLine(const String& line) {
    if (!blePhoneSeen || !Bluefruit.connected()) return;
    String payload = line.endsWith("\n") ? line : line + "\n";
    bleuart.write((const uint8_t*)payload.c_str(), payload.length());
}

static bool bleIsAdvertising() {
    ble_gap_addr_t addr;
    return (sd_ble_gap_adv_addr_get(0, &addr) == NRF_SUCCESS);
}

// Called last in setup() — after Serial, display, GPS, CDP/LoRa are all stable.
static void setupBLE() {
    // Disable the Bluefruit library's automatic connection-LED blink timer --
    // this board dedicates both LEDs to user/status indication, not a
    // library-owned BLE connection indicator.
    Bluefruit.autoConnLed(false);
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
    bool ble_ok = Bluefruit.begin(1, 0);
    // Enable DC-DC — must be after begin() (SD must be running first).
    // Reduces coil-whine from RADIO current spikes during advertising.
    sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);
    if (!ble_ok) {
        dspStatus("BLE FAIL", "begin()");
        Serial.println("[BLE] begin() FAILED"); Serial.flush();
        return;
    }
    // (removed success-path debug println/flush -- see setup() for rationale)

    // ── PPCP + name ──────────────────────────────────────────────────────────
    ble_gap_conn_params_t ppcp;
    ppcp.min_conn_interval = BLE_MIN_CONN_INTERVAL;
    ppcp.max_conn_interval = BLE_MAX_CONN_INTERVAL;
    ppcp.slave_latency     = BLE_SLAVE_LATENCY;
    ppcp.conn_sup_timeout  = BLE_CONN_SUP_TIMEOUT;
    sd_ble_gap_ppcp_set(&ppcp);
    Bluefruit.setTxPower(4);
    Bluefruit.setName(DUCK_NAME);

    // ── callbacks + NUS UART ─────────────────────────────────────────────────
    Bluefruit.Periph.setConnectCallback(ble_on_connect);
    Bluefruit.Periph.setDisconnectCallback(ble_on_disconnect);
    bleuart.begin();
    bleuart.setRxCallback(ble_uart_rx_cb);

    // ── advertising ──────────────────────────────────────────────────────────
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(bleuart);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    // Fast interval 160×0.625ms=100ms (was 20ms) — reduces coil-whine tick rate
    // while still being discoverable. Slow interval 244×0.625ms≈152ms unchanged.
    // Fast timeout 10s (was 30s) — switches to slow mode sooner.
    Bluefruit.Advertising.setInterval(160, 244);
    Bluefruit.Advertising.setFastTimeout(10);

    if (!Bluefruit.Advertising.start(0)) {
        Serial.println("[BLE] Advertising.start() FAILED"); Serial.flush();
        dspStatus("BLE FAIL", "adv.start()");
        return;
    }
    bleAdvertisingStarted = true;
    lastBleHealthMs = millis();
    sd_power_gpregret_clr(0, 0xFF);
    dspStatus("ADV STARTED!", DUCK_NAME);
    // (removed success-path debug println/flush -- see setup() for rationale)
}

// ── Busy-wait LED blink helper ─────────────────────────────────────────────
// Works without FreeRTOS tick or any library. PIN_LED1 is raw pin 15 = P0.15
// on this board (unlike WioTrackerL1's P1.01) -- see variant.cpp.
#define BLINK_LED(n) do { \
    NRF_P0->DIRSET = (1u<<15); \
    for(int _b=0;_b<(n);_b++){ \
        NRF_P0->OUTSET=(1u<<15); for(volatile uint32_t _d=0;_d<9600000u;_d++){} \
        NRF_P0->OUTCLR=(1u<<15); for(volatile uint32_t _d=0;_d<9600000u;_d++){} \
    } \
    for(volatile uint32_t _d=0;_d<32000000u;_d++){} \
} while(0)

// ── Battery ADC ───────────────────────────────────────────────────────────────
static float readVbat() {
    // Unlike WioTrackerL1 (which toggles its active-HIGH BAT_READ gate only
    // during a read to save power), this board's BAT_READ/VBAT_ENABLE is
    // active LOW and already driven permanently enabled by variant.cpp's
    // initVariant() at boot -- no per-read toggling is done here.
    //
    // This board's AREF_VOLTAGE/ADC_MULTIPLIER constants are calibrated for
    // the nRF52's internal 3.0V reference at 12-bit resolution (BAT_READ/
    // VBAT_ENABLE divider is 1M/512k) -- NOT the default ~3.6V supply
    // reference WioTrackerL1 relies on at 14-bit. Confirmed via MeshCore's
    // own SenseCapSolarBoard::getBattMilliVolts() (same hardware): it sets
    // analogReference(AR_INTERNAL_3_0) + analogReadResolution(12) before
    // every read and uses the identical "* ADC_MULTIPLIER * AREF_VOLTAGE /
    // 4.096" formula. Our previous version never set analogReference() (so
    // the ADC silently used the wrong default reference) and read at 14-bit
    // -- producing a badly wrong, usually too-low voltage that always
    // clamped to 0% in batteryPercent().
    analogReference(AR_INTERNAL_3_0);
    analogReadResolution(BATTERY_SENSE_RESOLUTION_BITS); // 12-bit
    delay(10); // let the reference settle, matches MeshCore's reference impl
    float raw  = (float)analogRead(BATTERY_PIN);
    float vbat = (raw * ADC_MULTIPLIER * AREF_VOLTAGE) / 4.096f / 1000.0f; // mV -> V
    return vbat;
}

static int batteryPercent(float vbat) {
    // LiPo: 4.2 V = 100%, 3.5 V = 0%.  Adjust thresholds as needed.
    float pct = (vbat - 3.5f) / (4.2f - 3.5f) * 100.0f;
    return (int)constrain(pct, 0.0f, 100.0f);
}

// Draw a live progress bar while the SOS button is held, so the user gets
// clear visual (not just audible) confirmation the hold is registering and
// can see roughly how much longer is needed. No-op on this board (no
// display hardware) -- kept so the rest of checkButton() is unchanged.
static void showHoldProgress(uint32_t heldMs) {
    uint8_t pct = (uint8_t)constrain((heldMs * 100UL) / SOS_HOLD_MS, 0UL, 100UL);
    gDisplay.begin();
    gDisplay.drawStrRight(0, idBuf);
    gDisplay.drawStrCenter(14, TXT_HOLD_FOR_SOS);
    gDisplay.drawProgressBar(14, 30, 100, 14, pct);
    gDisplay.end();
}

// ── Button debouncer ──────────────────────────────────────────────────────────
// (enum BtnEvent declared earlier near top of file)

static BtnEvent checkButton() {
    static bool     wasDown      = false;   // debounced previous-press state
    static uint32_t pressStartMs = 0;
    static uint8_t  clickCount   = 0;
    static uint32_t lastReleaseMs = 0;
    static bool     holdFired    = false;
    static uint8_t  holdBeepsFired = 0;   // how many hold-progress beeps fired this press
    static bool     progressShown  = false; // true once the on-screen hold bar has been drawn this press
    static uint32_t lastProgressDrawMs = 0;
    // Raw-read debounce state: filters switch contact bounce and brief
    // vibration/movement-induced contact closures (e.g. worn/carried on a
    // moving body) that would otherwise register as real presses and
    // trigger unintended single/double/triple-click mode changes.
    static bool     rawDown       = false;
    static uint32_t rawChangeMs   = 0;

    const uint32_t HOLD_MS        = SOS_HOLD_MS;
    const uint32_t CLICK_GAP      = 400;   // max ms between clicks in a multi-click burst
    const uint32_t PROGRESS_DELAY = 200;   // ms held before showing the bar (avoids flicker on quick clicks)
    const uint32_t DEBOUNCE_MS    = 30;    // raw reading must be stable this long before being trusted
    const uint32_t MIN_PRESS_MS   = 30;    // debounced press must last at least this long to count as a click

    bool rawNow = (digitalRead(CANCEL_BUTTON_PIN) == LOW);  // active LOW (per variant.h)
    if (rawNow != rawDown) {
        rawDown     = rawNow;
        rawChangeMs = millis();
    }
    bool btnDown = wasDown;
    if (millis() - rawChangeMs >= DEBOUNCE_MS) {
        btnDown = rawDown;
    }

    if (btnDown && !wasDown) {
        wasDown      = true;
        pressStartMs = millis();
        holdFired    = false;
        holdBeepsFired = 0;
        progressShown  = false;
    }
    // Live feedback while holding, so the user knows the SOS hold is being
    // registered and roughly how much longer to keep pressing (helps avoid
    // releasing too early, or wondering if the button is unresponsive).
    // beepBuzzer() is a no-op on this board (no buzzer hardware) -- these
    // calls are kept so the rest of the shared state machine is unchanged.
    if (wasDown && btnDown && !holdFired) {
        uint32_t heldMs = millis() - pressStartMs;
        if (holdBeepsFired < 1 && heldMs >= 500)  { beepBuzzer(1, 40, 0); holdBeepsFired = 1; }
        if (holdBeepsFired < 2 && heldMs >= 1000) { beepBuzzer(1, 40, 0); holdBeepsFired = 2; }
        if (holdBeepsFired < 3 && heldMs >= 1500) { beepBuzzer(2, 40, 40); holdBeepsFired = 3; }

        if (heldMs >= PROGRESS_DELAY && (!progressShown || millis() - lastProgressDrawMs >= 100)) {
            lastProgressDrawMs = millis();
            progressShown      = true;
            showHoldProgress(heldMs);
        }
    }
    // Detect 2-second hold while button is still pressed (fast path).
    if (wasDown && btnDown && !holdFired && (millis() - pressStartMs >= HOLD_MS)) {
        holdFired  = true;
        wasDown    = false;
        clickCount = 0;
        return BTN_HOLD_2S;
    }
    // Button released — also check for hold on release in case polling was
    // delayed by a beep (the button may have been released during the block).
    if (!btnDown && wasDown) {
        wasDown       = false;
        lastReleaseMs = millis();
        uint32_t heldMs = lastReleaseMs - pressStartMs;
        if (!holdFired && heldMs >= HOLD_MS) {
            clickCount = 0;
            return BTN_HOLD_2S;
        }
        // Reject presses shorter than MIN_PRESS_MS: a genuine intentional tap
        // holds contact far longer than a momentary vibration/movement
        // jolt, so this filters out accidental clicks without affecting
        // normal use.
        if (!holdFired && heldMs >= MIN_PRESS_MS) {
            clickCount++;
            beepBuzzer(1, 25, 0);   // immediate tick per click so the user can
                                    // self-correct a multi-click gesture in progress
        }
        // Released before the hold completed — restore whatever the screen
        // showed before we interrupted it with the progress bar.
        if (progressShown) {
            progressShown = false;
            displayHome();
        }
    }
    // Evaluate click burst after the inter-click silence window expires
    if (!btnDown && !wasDown && clickCount > 0 && (millis() - lastReleaseMs >= CLICK_GAP)) {
        uint8_t n  = clickCount;
        clickCount = 0;
        if      (n == 1) return BTN_SINGLE;
        else if (n == 2) return BTN_DOUBLE;
        else if (n == 3) return BTN_TRIPLE;
        else if (n >= 4) return BTN_QUAD;
    }
    return BTN_NONE;
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
    // 1 LED blink = firmware is alive, setup() entered (visible before Serial init).
    // If you see no LED activity at all after reset, the firmware is not running.
    BLINK_LED(1);

    // USB serial (debug / phone comms)
    Serial.begin(115200);

    // No display on this board -- initDisplay()/dspStatus() are no-ops.
    initDisplay();
    dspStatus("Booting...", DUCK_NAME);

    // Debug println()/flush() calls that used to run unconditionally on every
    // boot ("[BOOT] ...", "[SETUP] USB serial ready", etc.) were removed here
    // and at the other call sites noted below: printing/flushing to a
    // connected-but-unread USB CDC port (e.g. powered but no serial monitor
    // attached) can block once the TX ring buffer fills, adding real delay to
    // every boot. Failure-path diagnostics (only run on the rare error
    // branches) and the CDK:ID,VALUE protocol line the phone app reads are
    // kept.

    // ADC resolution — must be called before any analogRead().
    // ADC_RESOLUTION = 14 is defined above; the BSP defaults to 10 if this
    // call is omitted.  14-bit gives full-scale 16383 (0x3FFF).
    analogReadResolution(ADC_RESOLUTION);

    // LED + Button (no buzzer on this board -- see file header)
    pinMode(PIN_LED1, OUTPUT);
    digitalWrite(PIN_LED1, LOW);
    pinMode(PIN_LED2, OUTPUT);
    digitalWrite(PIN_LED2, LOW);
    // CANCEL_BUTTON_ACTIVE_PULLUP is false for this board (variant.h) -- a
    // plain INPUT is used rather than INPUT_PULLUP, unlike WioTrackerL1.
    pinMode(CANCEL_BUTTON_PIN, INPUT);   // active LOW (per variant.h)

    // GPS — wake the L76K before starting Serial1. GPS_EN is already driven
    // HIGH (enabled, active-HIGH on this board) once by variant.cpp's
    // initVariant() before setup() runs; PIN_GPS_STANDBY (GNSS_WAKEUP) is
    // asserted here, same pattern as WioTrackerL1's L76KB.
    pinMode(PIN_GPS_STANDBY, OUTPUT);
    digitalWrite(PIN_GPS_STANDBY, HIGH);   // STDBY_N high = active
    Serial1.begin(GPS_BAUDRATE);

    // Enable all three GNSS constellations for faster and more reliable signal acquisition.
    // The L76K default is GPS-only; adding GLONASS + BeiDou roughly triples visible satellites.
    // PMTK353: GPS(1) + GLONASS(1) + BeiDou(1) + Galileo(0) + NAVIC(0) — checksum 0x2A verified.
    delay(100);  // brief settle time after module power-on
    Serial1.println("$PMTK353,1,1,1,0,0*2A");
    delay(50);

    // initDisplay() + dspStatus("Booting...") were moved to before the Serial
    // wait at the top of setup() so the display always shows content on boot
    // (no-op here, kept for structural parity with the OLED-equipped ports).

    // ── CDP init (LoRa / routing / storage) ─────────────────────────────────
    dspStatus("CDP init", DUCK_NAME);
    if (duck.setupWithDefaults() != DUCK_ERR_NONE) {
        BLINK_LED(10);  // 10 blinks = CDP setup failed
        Serial.println("[MAMA] Failed to setup MamaDuck"); Serial.flush();
        return;
    }
    duck.goPublic();
    duck.onReceiveDuckData(handleDuckData);
    setupOK = true;
    snprintf(idBuf, sizeof(idBuf), "ID:%s", DUCK_NAME);

    // Broadcasts this Duck's long-term public key so OpenDMS and nearby
    // MamaDucks can learn it (TOFU) and use encrypted_cmd/encrypted_data
    // instead of plaintext (see Duck.h's announceIdentity()). Only useful
    // once encryption is actually enabled for this build/runtime (see
    // Duck::isMamaLinkEncryptionEnabled()/isUplinkEncryptionEnabled()), but
    // harmless to broadcast unconditionally either way.
    duck.announceIdentity();
    BLINK_LED(5);   // 5 blinks = CDP fully initialized
    dspStatus("CDP OK", DUCK_NAME);

    // Re-init display after CDP/LoRa are stable -- no-op on this board (no
    // display hardware); kept for structural parity with the OLED-equipped
    // ports.
    dspStatus("CDP Ready", DUCK_NAME);
    BLINK_LED(3);

    // BLE init goes last — same order as MeshCore (radio/filesystem first,
    // BLE last).  setupBLE() calls Bluefruit.begin() and starts advertising.
    setupBLE();

    // Re-assert GPS wakeup — BLE init can take several hundred ms.
    digitalWrite(PIN_GPS_STANDBY, HIGH);

    // Re-configure button — defensive in case BLE/SD peripheral init disturbed it.
    pinMode(CANCEL_BUTTON_PIN, INPUT);

    Serial.println(String("CDK:ID,VALUE:") + DUCK_ID_BUF);
    sendBattery();
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    // Diagnostic heartbeat — always runs, even if setup() failed.
    // Lets us see the reason via serial without missing the one-shot setup() log.
    static unsigned long lastDiagMs = 0;
    if (!setupOK && millis() - lastDiagMs >= 2000UL) {
        lastDiagMs = millis();
        Serial.println("[DIAG] setupOK=false — setup() did not complete");
    }
    // Diagnostic: 7 fast blinks at first loop() entry — visible even if setupOK=false.
    static bool loopEntryBlinked = false;
    if (!loopEntryBlinked) {
        loopEntryBlinked = true;
        for (int _i = 0; _i < 7; _i++) {
            digitalWrite(PIN_LED1, HIGH); delay(50);
            digitalWrite(PIN_LED1, LOW);  delay(50);
        }
        delay(400);
    }
    if (!setupOK) return;

    // ── First-run display init ────────────────────────────────────────────────
    // Wait up to 5 s for the serial monitor to attach so early logs are visible.
    static unsigned long loopFirstMs = 0;
    if (loopFirstMs == 0) loopFirstMs = millis();
    if (!Serial && millis() - loopFirstMs < 500) return;

    static bool displayProbed = false;
    if (!displayProbed) {
        displayProbed = true;
        // No-op on this board (gDisplayOk is always false); kept for
        // structural parity with the OLED-equipped ports.
        if (gDisplayOk) {
            displayHome();
        }
    }

    // ── Display deferred rendering ─────────────────────────────────────────────

    if (phoneGpsDisplayPending) {
        phoneGpsDisplayPending = false;
        dspPowerSave(0);
        dspBegin();
        dspStr(0, 0, (TXT_BATT_LABEL + String(batteryPercent(readVbat())) + "%").c_str());
        dspStrRight(0, idBuf);
        if (phoneGpsNoFix) {
            dspStrCenter(26, TXT_PHONE_GPS);
            dspStrCenter(38, TXT_NO_SIGNAL);
            dspEnd();
            delay(2000);
        } else {
            dspStr(0, 14, gpsLoraOk ? TXT_GPS_SENT_OK : TXT_GPS_SEND_FAIL);
            dspStr(0, 28, ("LAT:" + String(phoneGpsLatBuf)).c_str());
            dspStr(0, 40, ("LNG:" + String(phoneGpsLngBuf)).c_str());
            dspStr(0, 52, TXT_SRC_PHONE);
            dspEnd();
            delay(3000);
        }
        dspPowerSave(1);
    }

    // USB connected / disconnected splashes.
    if (usbConnectDisplayPending) {
        usbConnectDisplayPending = false;
        dspPowerSave(0);
        dspBegin();
        dspStrRight(0, idBuf);
        dspStrCenter(26, TXT_USB_SERIAL);
        dspStrCenter(38, TXT_CONNECTED_BANG);
        dspEnd();
        delay(2000);
        displayHome();
    }
    if (usbDisconnectDisplayPending) {
        usbDisconnectDisplayPending = false;
        dspPowerSave(0);
        displayEnabled = true;
        dspBegin();
        dspStrRight(0, idBuf);
        dspStrCenter(26, TXT_USB_SERIAL);
        dspStrCenter(38, TXT_DISCONNECTED);
        dspEnd();
        delay(2000);
        displayHome();
    }

    // SOS acknowledgement from operator — show for SOS_ACK_DISPLAY_MS then auto-return home.
    if (sosAckDisplayPending) {
        sosAckDisplayPending = false;
        displayEnabled = true;
        dspPowerSave(0);
        dspBegin();
        dspStrRight(0, idBuf);
        dspStrCenter(16, TXT_SOS_RECEIVED);
        dspStrCenter(28, TXT_HELP_BEING);
        dspStrCenter(40, TXT_SENT);
        dspEnd();
        blinkLed(3);
        messagePending = false;
        sosAckUntilMs  = millis() + SOS_ACK_DISPLAY_MS;
    }

    // Auto-dismiss SOS ACK screen after timeout.
    if (sosAckUntilMs > 0 && millis() >= sosAckUntilMs) {
        sosAckUntilMs = 0;
        dspPowerSave(0);
        displayHome();
    }

    // Auto-refresh home screen.
    if (displayEnabled && !phoneGpsDisplayPending
        && !usbConnectDisplayPending && !messagePending && !emergencyDisplayPending
        && sosAckUntilMs == 0
        && (millis() - lastHomeRefreshMs >= HOME_REFRESH_MS)) {
        lastHomeRefreshMs = millis();
        float rawRssi = lora.getRSSI();
        if (rawRssi < 0.0f) {
            float normRssi = constrain((rawRssi       - RSSI_MIN) / (RSSI_MAX - RSSI_MIN), 0.0f, 1.0f);
            float normSnr  = constrain((lora.getSNR() - SNR_MIN)  / (SNR_MAX  - SNR_MIN),  0.0f, 1.0f);
            lastSignalPct  = (int)(((normRssi + normSnr) / 2.0f) * 100.0f);
        }
        displayHome();
    }

    // ── Button ────────────────────────────────────────────────────────────────
    BtnEvent btn = checkButton();

    if (btn == BTN_HOLD_2S) {
        // Hardware-button SOS — request GPS from phone if we don't have a fix.
        String gpsLat, gpsLng, gpsAlt, gpsSpd, gpsHdg;
        bool gotGps = false;

        if (tinyGps.location.isValid() && tinyGps.location.age() < 5000) {
            gpsLat = String(tinyGps.location.lat(), 6);
            gpsLng = String(tinyGps.location.lng(), 6);
            if (tinyGps.altitude.isValid()) gpsAlt = String(tinyGps.altitude.meters(), 1);
            if (tinyGps.speed.isValid())    gpsSpd = String(tinyGps.speed.kmph(), 1);
            if (tinyGps.course.isValid())   gpsHdg = String(tinyGps.course.deg(), 1);
            gotGps = true;
        }

        if (gotGps) {
            // Already have a local fix — send immediately, no need to wait.
            dspBegin();
            dspStrRight(0, idBuf);
            displayBatt();
            dspStrCenter(22, TXT_SENDING);
            dspStrCenter(34, TXT_EMERGENCY_SIGNAL_DOTS);
            dspEnd();
            sendEmergency(gpsLat, gpsLng, gpsAlt, gpsSpd, gpsHdg, /* gpsFromPhone= */ false);
        } else if (phoneGpsLatBuf[0] != '\0') {
            // We already have a cached phone fix from earlier — use it now.
            dspBegin();
            dspStrRight(0, idBuf);
            displayBatt();
            dspStrCenter(22, TXT_SENDING);
            dspStrCenter(34, TXT_EMERGENCY_SIGNAL_DOTS);
            dspEnd();
            sendEmergency(String(phoneGpsLatBuf), String(phoneGpsLngBuf),
                          phoneGpsAltBuf[0] ? String(phoneGpsAltBuf) : "",
                          phoneGpsSpdBuf[0] ? String(phoneGpsSpdBuf) : "",
                          phoneGpsHdgBuf[0] ? String(phoneGpsHdgBuf) : "",
                          /* gpsFromPhone= */ true);
        } else if (isPhoneConnected()) {
            // No fix yet — ask the phone and continue asynchronously below
            // (see "Deferred SOS" block) so we don't block button polling
            // or duck.run(). Single-click cancels this while it's pending.
            dspBegin();
            dspStrRight(0, idBuf);
            dspStrCenter(22, TXT_REQUESTING_GPS);
            dspStrCenter(34, TXT_FROM_PHONE_DOTS);
            dspStrCenter(46, TXT_CLICK_TO_CANCEL);
            dspEnd();
            broadcast("CDK:GPSREQ");
            sosPending     = true;
            sosWaitStartMs = millis();
        } else {
            // No local fix, no phone connected — send anyway, but make sure
            // the user sees clearly that no location was included.
            dspBegin();
            dspStrRight(0, idBuf);
            displayBatt();
            dspStrCenter(22, TXT_SENDING);
            dspStrCenter(34, TXT_EMERGENCY_SIGNAL_DOTS);
            dspEnd();
            sendEmergency("", "", "", "", "", /* gpsFromPhone= */ false);
        }
    }

    if (btn == BTN_SINGLE) {
        if (sosPending) {
            // Cancel a pending SOS that's still waiting on a phone GPS reply.
            sosPending = false;
            beepBuzzer(1, 60, 0);
            dspPowerSave(0);
            displayEnabled = true;
            displayHome();
        } else if (sosAckUntilMs > 0) {
            sosAckUntilMs = 0;
            dspPowerSave(0);
            displayHome();
        } else if (emergencyDisplayPending) {
            emergencyDisplayPending = false;
            displayEnabled = true;
            dspPowerSave(0);
            displayHome();
        } else if (messagePending) {
            messagePending = false;
            displayEnabled = true;
            dspPowerSave(0);
            displayHome();
        } else {
            displayEnabled = !displayEnabled;
            if (displayEnabled) {
                dspPowerSave(0);
                displayHome();
            } else {
                dspPowerSave(1);
            }
        }
    }

    // Double-click: Roger acknowledgement — moved from triple-click since
    // this is the more time-critical rescue-coordination action and
    // deserves the fewer-clicks slot. The old double-click battery-send
    // feature was removed: battery is already auto-broadcast on BLE
    // connect (ble_on_connect()) and periodically over USB, so a manual
    // send added no information the phone didn't already have.
    if (btn == BTN_DOUBLE) {
        // protobuf-encoded StatusMsg wrapped in a StatusReport (same topic,
        // matching handleMsg()'s phone-composed messages).
        duckcdp_StatusMsg rogerMsg = duckcdp_StatusMsg_init_zero;
        rogerMsg.src = duckcdp_StatusMsgSrc_STATUS_MSG_SRC_DEVICE;
        std::snprintf(rogerMsg.text, sizeof(rogerMsg.text), "Roger");
        std::vector<uint8_t> rogerEncoded = duckpayload::encodeStatusReportMsg(rogerMsg);
        sendUplink(topics::status, std::string(reinterpret_cast<const char*>(rogerEncoded.data()), rogerEncoded.size()));
        broadcast("CDK:ACK,ID:ROGER");
        dspPowerSave(0);
        displayEnabled = true;
        dspBegin();
        dspStrRight(0, idBuf);
        dspStrCenter(28, TXT_ROGER_SENT);
        dspEnd();
        delay(2000);
        displayHome();
    }

    // Triple-click: GPS/date-time pages — moved from quadruple-click now
    // that only three gestures (single/double/triple) are used.
    if (btn == BTN_TRIPLE) {
        dspPowerSave(0);
        displayEnabled = true;
        dspBegin();
        dspStrRight(0, idBuf);
        displayBatt();
        if (tinyGps.location.isValid()) {
            dspStr(0, 14, ("LAT:" + String(tinyGps.location.lat(), 5)).c_str());
            dspStr(0, 26, ("LNG:" + String(tinyGps.location.lng(), 5)).c_str());
            dspStr(0, 38, ("SATS:" + String(tinyGps.satellites.value())).c_str());
            dspStr(0, 50, ("AGE:" + String(tinyGps.location.age()) + "ms").c_str());
            dspEnd();
            delay(2500);

            // Page 2: UTC+8 date/time
            int h  = tinyGps.time.hour() + 8;
            int mi = tinyGps.time.minute();
            int sc = tinyGps.time.second();
            int d  = tinyGps.date.day();
            int mo = tinyGps.date.month();
            int y  = tinyGps.date.year();
            if (h >= 24) {
                h -= 24; d++;
                const uint8_t dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
                uint8_t maxD = dim[mo - 1];
                if (mo == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) maxD = 29;
                if (d > maxD) { d = 1; mo++; if (mo > 12) { mo = 1; y++; } }
            }
            dspBegin();
            dspStrRight(0, idBuf);
            displayBatt();
            if (tinyGps.date.isValid() && tinyGps.time.isValid()) {
                char dateBuf[20], timeBuf[20];
                snprintf(dateBuf, sizeof(dateBuf), "DATE:%04d/%02d/%02d", y, mo, d);
                snprintf(timeBuf, sizeof(timeBuf), "TIME:%02d:%02d:%02d", h, mi, sc);
                dspStr(0, 14, dateBuf);
                dspStr(0, 26, timeBuf);
                dspStr(0, 38, "(GMT+8 / UTC+8)");
            } else {
                dspStrCenter(26, TXT_DATE_TIME);
                dspStrCenter(38, TXT_NO_SIGNAL);
            }
            dspEnd();
            delay(2500);
        } else {
            dspStrCenter(28, gpsModuleDetected ? TXT_GPS_MODULE_ACTIVE : TXT_GPS_NO_MODULE);
            dspStrCenter(40, gpsModuleDetected ? TXT_WAITING_SIGNAL_DOTS : "(Serial1)");
            dspEnd();
            delay(2500);
        }
        displayHome();
    }

    // ── USB discovery / incoming ───────────────────────────────────────────────
    {
        static unsigned long lastUsbAnnounceMs = 0;
        if (usbPhoneSeen && lastUsbRxMs > 0 && millis() - lastUsbRxMs > USB_IDLE_TIMEOUT_MS) {
            usbPhoneSeen             = false;
            lastUsbAnnounceMs        = 0;
            usbDisconnectDisplayPending = true;
        }
        if (!usbPhoneSeen && millis() - lastUsbAnnounceMs >= 3000UL) {
            Serial.println(String("CDK:ID,VALUE:") + DUCK_ID_BUF);
            sendBattery();
            lastBattMs        = millis();
            lastUsbAnnounceMs = millis();
        }
    }

    // ── BLE incoming (dispatched from ble_uart_rx_cb ISR-context) ──────────────
    if (bleRxPending) {
        String line = String(bleRxLine);
        bleRxPending = false;
        if (line.startsWith("CDK:")) handleFrame(line);
    }

    // BLE advertising watchdog — mirrors MeshCore's health-check logic.
    if (bleAdvertisingStarted && !Bluefruit.connected() &&
        millis() - lastBleHealthMs >= BLE_HEALTH_INTERVAL_MS) {
        lastBleHealthMs = millis();
        if (!bleIsAdvertising()) {
            Serial.println("[BLE] watchdog: restarting advertising"); Serial.flush();
            Bluefruit.Advertising.start(0);
        }
    }

    // Periodic fail-closed rejection-count report, every 5 min, only when
    // there's something to report (see SecurityEventCounters.h) -- lets an
    // operator tell "keys are fine, we're blocking forged traffic" apart
    // from "our own config/key is broken and legitimate traffic is being
    // silently dropped", without ever trusting the rejected data itself.
    // Uses the existing plaintext topics::status uplink (already relayed
    // and stored as-is by MeshBeacon Ops), so no new wire format or
    // server-side changes are needed.
    {
        static unsigned long lastSecEventsReportMs = 0;
        if (millis() - lastSecEventsReportMs >= 300000UL) {
            lastSecEventsReportMs = millis();
            if (securityevents::hasEvents()) {
                sendUplink(topics::status, securityevents::summary());
                securityevents::reset();
            }
        }
    }

    while (Serial.available()) {
        lastUsbRxMs = millis();
        char c = Serial.read();
        if (c == '\n') {
            if (usbInBuf.startsWith("CDK:")) {
                if (!usbPhoneSeen) usbConnectDisplayPending = true;
                usbPhoneSeen = true;
            }
            handleFrame(usbInBuf);
            usbInBuf = "";
        } else if (c != '\r') {
            usbInBuf += c;
        }
    }

    // Periodic battery update every 60 s.
    if (millis() - lastBattMs >= 60000UL) {
        sendBattery();
        lastBattMs = millis();
        if (displayEnabled) displayHome();
    }

    // Periodic identity re-announce every 5 min (broadcast), so a peer that
    // missed the one-time announceIdentity() in setup() -- e.g. it booted
    // later, or was out of LoRa range at the time, or the broadcast packet
    // was simply lost (not uncommon over LoRa) -- eventually learns this
    // Duck's public key too. Without this, on a build/runtime where MTALK
    // encryption is enabled, sendMamaLink() would keep returning non-zero
    // against that one peer indefinitely until both sides have mutually
    // exchanged identities at least once. Runs unconditionally (harmless
    // when encryption is disabled -- sendMamaLink() falls back to plain
    // sendData() regardless of whether identities were exchanged).
    if (millis() - lastIdentityAnnounceMs >= 300000UL) {
        duck.announceIdentity();
        lastIdentityAnnounceMs = millis();
    }

    // Feed GPS NMEA into TinyGPSPlus.
    while (Serial1.available()) {
        char c = Serial1.read();
        if (!gpsModuleDetected) {
            gpsModuleDetected = true;
            Serial.println("[GPS] Module detected");
        }
        tinyGps.encode(c);
    }
    if (!gpsFix && tinyGps.location.isValid()) {
        gpsFix = true;
        Serial.printf("[GPS] Fix: lat=%.6f lng=%.6f sats=%u\n",
                      tinyGps.location.lat(), tinyGps.location.lng(),
                      tinyGps.satellites.value());
    }

    duck.run();

    // Execute any beep deferred from duck.run() callbacks (avoids recursive duck.run()).
    if (gBeepReq.times > 0) {
        int t = gBeepReq.times, on = gBeepReq.onMs, off = gBeepReq.offMs;
        gBeepReq = {};
        beepBuzzer(t, on, off);
    }

    // ── Deferred GPS LoRa TX ──────────────────────────────────────────────────
    if (flushDeferredGpsTx(gpsTxPending, gpsTxPayload, &gpsLoraOk)) {
        Serial.printf("[GPS] Deferred TX %s (%u bytes)\n", gpsLoraOk ? "OK" : "FAILED", (unsigned)gpsTxPayload.size());
    }

    // ── Deferred BEACON_ACK TX ────────────────────────────────────────────────
    armBeaconAckIfPending(beaconAckPending, beaconAckDeferMs);
    if (beaconAckReadyToSend(beaconAckDeferMs, beaconAckPending, gpsTxPending)) {
        std::string beaconAckWire = meshgroupconfig::isConfigured()
            ? encryptBeaconPayload(TOPIC_BEACON_ACK, beaconAckPayload)
            : std::string(beaconAckPayload);
        duck.sendData(TOPIC_BEACON_ACK, beaconAckWire, BROADCAST_DUID);
        Serial.printf("[BEACON] ACK TX: %s\n", beaconAckPayload);
    }

    // ── Deferred display clear after GPS request ──────────────────────────────
    if (gpsDisplayClearMs > 0 && millis() >= gpsDisplayClearMs) {
        gpsDisplayClearMs = 0;
        dspPowerSave(1);
    }

    // ── Deferred SOS: waiting (non-blocking) for a phone GPS reply ───────────
    // Triggered from BTN_HOLD_2S above when no local fix was available yet.
    // Cancelled by a single click (see BTN_SINGLE handling above).
    if (sosPending) {
        if (phoneGpsLatBuf[0] != '\0') {
            sosPending = false;
            dspBegin();
            dspStrRight(0, idBuf);
            displayBatt();
            dspStrCenter(22, TXT_SENDING);
            dspStrCenter(34, TXT_EMERGENCY_SIGNAL_DOTS);
            dspEnd();
            sendEmergency(String(phoneGpsLatBuf), String(phoneGpsLngBuf),
                          phoneGpsAltBuf[0] ? String(phoneGpsAltBuf) : "",
                          phoneGpsSpdBuf[0] ? String(phoneGpsSpdBuf) : "",
                          phoneGpsHdgBuf[0] ? String(phoneGpsHdgBuf) : "",
                          /* gpsFromPhone= */ true);
        } else if (millis() - sosWaitStartMs >= SOS_GPS_WAIT_MS) {
            sosPending = false;
            dspBegin();
            dspStrRight(0, idBuf);
            displayBatt();
            dspStrCenter(22, TXT_SENDING);
            dspStrCenter(34, TXT_EMERGENCY_SIGNAL_DOTS);
            dspEnd();
            sendEmergency("", "", "", "", "", /* gpsFromPhone= */ false);
        }
    }

    // ── Deferred CDK:GPSREQ dispatch ─────────────────────────────────────────
    if (gpsReqDeferredSendMs > 0 && millis() >= gpsReqDeferredSendMs) {
        gpsReqDeferredSendMs = 0;
        if (isPhoneConnected() && phoneGpsLatBuf[0] == '\0') {
            broadcast("CDK:GPSREQ");
            gpsReqSentMs = millis();
        } else if (phoneGpsLatBuf[0] != '\0' && !gpsTxPending) {
            duckcdp_GpsReading reading = buildGpsReadingFix(duckcdp_GpsSource_GPS_SOURCE_PHONE,
                String(phoneGpsLatBuf), String(phoneGpsLngBuf), String(phoneGpsAltBuf),
                String(phoneGpsSpdBuf), String(phoneGpsHdgBuf),
                batteryPercent(readVbat()), currentRssiDbm());
            gpsTxPayload = duckpayload::encodeGps(reading);
            gpsTxPending = true;
        }
    }

    // GPS request timeout fallback.
    if (gpsReqSentMs > 0 && !gpsTxPending && millis() - gpsReqSentMs > 10000UL) {
        gpsReqSentMs = 0;
        duckcdp_GpsReading noGps = buildGpsReadingNoFix(duckcdp_GpsSource_GPS_SOURCE_NONE,
            duckcdp_GpsNoFixReason_GPS_REASON_NO_RESPONSE,
            batteryPercent(readVbat()), currentRssiDbm());
        std::vector<uint8_t> encoded = duckpayload::encodeGps(noGps);
        sendUplink(topics::gps, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
    }

    delay(5);
}

// Services a "CMD:GPS_REQUEST" received inside an authenticated
// encrypted_cmd payload (see topics::encrypted_cmd in
// handleDuckData() below). Deliberately NOT reachable from any plaintext
// topic -- this is the exact action a rogue "operator" would want to
// trigger to exfiltrate a duck's live location, so it must only run after
// duckcrypto::decryptFromPeer() has verified the request came from
// whoever holds OpenDMS's static private key.
void handleGpsRequestCommand() {
    if (tinyGps.location.isValid()) {
        float altM   = tinyGps.altitude.isValid() ? tinyGps.altitude.meters()  : 0.0f;
        float spdKh  = tinyGps.speed.isValid()    ? tinyGps.speed.kmph()        : 0.0f;
        float hdgDeg = tinyGps.course.isValid()   ? tinyGps.course.deg()        : 0.0f;
        duckcdp_GpsReading reading = duckcdp_GpsReading_init_zero;
        reading.has_fix = true;
        reading.source = duckcdp_GpsSource_GPS_SOURCE_DEVICE;
        reading.no_fix_reason = duckcdp_GpsNoFixReason_GPS_REASON_NONE;
        reading.lat_e7 = (int32_t)lround(tinyGps.location.lat() * 1e7);
        reading.lng_e7 = (int32_t)lround(tinyGps.location.lng() * 1e7);
        reading.alt_m = (int32_t)lround(altM);
        reading.spd_dkmh = (uint32_t)lround(spdKh * 10);
        reading.hdg_deg = (uint32_t)lround(hdgDeg);
        reading.sats = tinyGps.satellites.value();
        reading.batt_pct = batteryPercent(readVbat());
        reading.rssi_dbm = currentRssiDbm();
        std::vector<uint8_t> encoded = duckpayload::encodeGps(reading);
        dspPowerSave(0);
        dspBegin();
        dspStr(0, 0, (TXT_BATT_LABEL + String(batteryPercent(readVbat())) + "%").c_str());
        dspStrRight(0, idBuf);
        dspStr(0, 14, TXT_SENDING_GPS_DATA);
        dspStr(0, 28, ("LAT:" + String(tinyGps.location.lat(), 5)).c_str());
        dspStr(0, 40, ("LNG:" + String(tinyGps.location.lng(), 5)).c_str());
        dspEnd();
        sendUplink(topics::gps, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
        delay(3000);
        dspPowerSave(1);
    } else {
        bool phoneConnected = isPhoneConnected();
        dspPowerSave(0);
        dspBegin();
        dspStr(0, 0, (TXT_BATT_LABEL + String(batteryPercent(readVbat())) + "%").c_str());
        dspStrRight(0, idBuf);
        if (phoneConnected) {
            if (phoneGpsLatBuf[0] != '\0') {
                dspStr(0, 14, TXT_SENDING_GPS_DATA);
                dspStr(0, 28, ("LAT:" + String(phoneGpsLatBuf)).c_str());
                dspStr(0, 42, ("LNG:" + String(phoneGpsLngBuf)).c_str());
            } else {
                dspStrCenter(28, TXT_REQUESTING_GPS_DATA);
                dspStrCenter(40, TXT_FROM_PHONE_DOTS);
            }
            dspEnd();
            if (gpsReqDeferredSendMs == 0) gpsReqDeferredSendMs = millis() + 400;
        } else {
            dspStrCenter(28, TXT_NO_PHONE);
            dspStrCenter(40, TXT_NO_GPS_DATA);
            dspEnd();
            duckcdp_GpsReading noGps = duckcdp_GpsReading_init_zero;
            noGps.has_fix = false;
            noGps.source = duckcdp_GpsSource_GPS_SOURCE_NONE;
            noGps.no_fix_reason = duckcdp_GpsNoFixReason_GPS_REASON_NO_RESPONSE;
            noGps.batt_pct = batteryPercent(readVbat());
            noGps.rssi_dbm = currentRssiDbm();
            std::vector<uint8_t> encoded = duckpayload::encodeGps(noGps);
            sendUplink(topics::gps, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
        }
        gpsDisplayClearMs = millis() + 2000;
    }
}

// ── handleDuckData ────────────────────────────────────────────────────────────
void handleDuckData(CdpPacket packet) {
    bool isForMe    = (memcmp(packet.dduid.data(), duck.getDuckId().data(), 8) == 0);
    bool isBroadcast = (packet.dduid[0] == 0xFF);
    bool beaconAuthenticated = false;

    Serial.printf("[RX] topic=%u duckType=%u isForMe=%d src=%.8s\n",
                  packet.topic, (uint8_t)packet.duckType, (int)isForMe,
                  (char*)packet.sduid.data());

    // ── 1. Extract GPS from packet and update cache ───────────────────────────
    {
        String pdata;
        std::string decryptedBeacon;
        bool isBeaconTopic = (packet.topic == TOPIC_BEACON || packet.topic == TOPIC_BEACON_ACK);
        if (isBeaconTopic
            && decryptBeaconPayload(packet.topic, packet.sduid.data(), packet.data, decryptedBeacon)) {
            beaconAuthenticated = true;
            // std::string guarantees a null terminator via c_str(), unlike
            // Adafruit nRF52's String which has no (char*, len) ctor.
            pdata = String(decryptedBeacon.c_str());
        } else if (isBeaconTopic && meshgroupconfig::isConfigured()) {
            // A mesh group key IS provisioned (BEACON encryption is
            // explicitly enabled for this deployment) but decryption
            // failed here -- forged/corrupt, or a different deployment's
            // key. Never fall back to trusting the raw bytes as plaintext
            // GPS in that case: that would let anyone in LoRa range spoof
            // another duck's location with a bare, unencrypted TOPIC_BEACON
            // packet. Leave pdata empty so the LAT:/LNG: lookup below finds
            // nothing.
        } else {
            // Adafruit nRF52 String has no (char*, len) ctor: null-terminate manually.
            std::vector<uint8_t> tmp = packet.data;
            tmp.push_back(0);
            pdata = String((const char*)tmp.data());
        }
        int latIdx = pdata.indexOf("LAT:");
        int lngIdx = pdata.indexOf("LNG:");
        if (latIdx >= 0 && lngIdx >= 0) {
            int latEnd = pdata.indexOf(',', latIdx + 4);
            int lngEnd = pdata.indexOf(',', lngIdx + 4);
            float lat = pdata.substring(latIdx + 4, latEnd < 0 ? (int)pdata.length() : latEnd).toFloat();
            float lng = pdata.substring(lngIdx + 4, lngEnd < 0 ? (int)pdata.length() : lngEnd).toFloat();
            if (!(lat == 0.0f && lng == 0.0f)) {
                char _buf9[9]; memcpy(_buf9, packet.sduid.data(), 8); _buf9[8] = 0;
                String sid(_buf9);
                duckGpsCache[sid] = { lat, lng, millis() };
            }
        }
    }

    // ── 2. Emit CDK:SEEN ─────────────────────────────────────────────────────
    {
        const char* typeStr = "UNKN";
        switch ((uint8_t)packet.duckType) {
            case DuckType::MAMA:     typeStr = "MAMA"; break;
            case DuckType::LINK:     typeStr = "LINK"; break;
            case DuckType::PAPA:     typeStr = "PAPA"; break;
            case DuckType::DETECTOR: typeStr = "DETC"; break;
            default: break;
        }
        char _ssidbuf[9]; memcpy(_ssidbuf, packet.sduid.data(), 8); _ssidbuf[8] = 0;
        String sid(_ssidbuf);
        sid.trim();
        if (sid.length() > 0 && sid != String((const char*)duck.getDuckId().data())) {
            auto gpsIt = duckGpsCache.find(sid);
            if (gpsIt != duckGpsCache.end() && millis() - gpsIt->second.tsMs < DUCK_GPS_TTL_MS) {
                char seenBuf[80];
                snprintf(seenBuf, sizeof(seenBuf), "CDK:SEEN,ID:%.8s,TYPE:%s,LAT:%.6f,LNG:%.6f",
                         (char*)packet.sduid.data(), typeStr,
                         gpsIt->second.lat, gpsIt->second.lng);
                broadcast(seenBuf);
            } else {
                char seenBuf[48];
                snprintf(seenBuf, sizeof(seenBuf), "CDK:SEEN,ID:%.8s,TYPE:%s",
                         (char*)packet.sduid.data(), typeStr);
                broadcast(seenBuf);
            }
        }
    }

    if (!isForMe && !isBroadcast
        && packet.topic != TOPIC_BEACON && packet.topic != TOPIC_BEACON_ACK) return;

    String message;
    {
        std::vector<uint8_t> tmp = packet.data;
        tmp.push_back(0);
        message = String((const char*)tmp.data());
    }

    switch (packet.topic) {
        // encrypted_cmd (topic 8) is OpenDMS's decrypted, AUTHENTICATED
        // operator downlink -- MamaDuck.h's encrypted_cmd handler leaves
        // packet.topic set to 8 after successful decryptFromPeer() (unlike
        // encrypted_data, which restores the real app topic), so it must
        // be handled here explicitly or the decrypted command is silently
        // dropped. Privileged actions that imply an operator actually saw/
        // acknowledged something (SOS_ACK) or that make this duck act on a
        // remote request (GPS_REQUEST) are handled ONLY here, never under
        // the plaintext `case 22:` below -- anyone in LoRa range can forge
        // a plaintext dcmd, but only OpenDMS's static private key can
        // produce a packet that decrypts successfully here (see
        // docs/crypto-design.tex).
        case topics::encrypted_cmd:
            if (duckpayload::isProtobuf(packet.data.data(), packet.data.size())) {
              duckcdp_OpText opText = duckcdp_OpText_init_zero;
              if (!duckpayload::decodeOpText(packet.data.data(), packet.data.size(), opText)) {
                logerr_ln("[MSG] ERROR: failed to decode OpText payload.");
                break;
              }
              message = String(opText.text);
            }
            if (message.indexOf("SOS DITERIMA") >= 0) {
                static unsigned long lastSosAckMs = 0;
                if (millis() - lastSosAckMs < 5000UL) break;
                lastSosAckMs         = millis();
                sosAckDisplayPending = true;
                gBeepReq = {1, 500, 0};  // deferred: 1 long beep = SOS acknowledged (relief); no-op on this board
                broadcast("CDK:SOS_ACK,TEXT:SOS DITERIMA");
                break;
            }
            if (message.indexOf("CMD:GPS_REQUEST") >= 0) {
                handleGpsRequestCommand();
                break;
            }
            dspPowerSave(0);
            beepBuzzer(1, 150, 0);     // immediate: beep before message appears (no-op on this board)
            displayMessage(message);
            emergencyDisplayPending = true;
            displayEnabled          = true;
            {
              duckcdp_OpText ack = buildOpText("MSG_READ:TEXT:" + message);
              std::vector<uint8_t> encoded = duckpayload::encodeOpText(ack);
              sendUplink(22, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
            }
            blinkLed(1);
            broadcast(String("CDK:MSG,TEXT:") + message);
            break;

        // Plaintext dcmd (topic 22 = 0x16) -- NOT authenticated. Unlike
        // encrypted_cmd above, this can be forged by anyone in LoRa range,
        // so it must never trigger a privileged/operator-implying action
        // (no SOS_ACK, no GPS_REQUEST) -- display-only, so unencrypted
        // fleets (no OpenDMS key provisioned) still see operator messages,
        // but a forged one can't falsely tell a user their SOS was
        // acknowledged, nor force this duck to broadcast its live GPS.
        //
        // Once this device has a real OpenDMS key pinned
        // (opendmsconfig::isConfigured()), the deployment has explicitly
        // opted into authenticated operator commands -- silently accepting
        // a plaintext fallback at that point would mask a broken/
        // misconfigured OpenDMS side (e.g. an invalid duck_crypto keypair
        // causing sendEncryptedCommand() to silently fall back to
        // plaintext) and would still let anyone in LoRa range forge
        // operator messages even though the operator believes the channel
        // is encrypted. So once configured, reject plaintext dcmd outright
        // instead of displaying it. Devices that have no OpenDMS key
        // provisioned yet are unaffected and keep accepting it.
        case 22:
            if (opendmsconfig::isConfigured()) {
                logerr_ln("Plaintext dcmd received but OpenDMS key is configured (encryption required), dropping.");
                break;
            }
            if (duckpayload::isProtobuf(packet.data.data(), packet.data.size())) {
              duckcdp_OpText opText = duckcdp_OpText_init_zero;
              if (!duckpayload::decodeOpText(packet.data.data(), packet.data.size(), opText)) {
                logerr_ln("[MSG] ERROR: failed to decode OpText payload.");
                break;
              }
              message = String(opText.text);
            }
            dspPowerSave(0);
            beepBuzzer(1, 150, 0);
            displayMessage(message);
            emergencyDisplayPending = true;
            displayEnabled          = true;
            {
              duckcdp_OpText ack = buildOpText("MSG_READ:TEXT:" + message);
              std::vector<uint8_t> encoded = duckpayload::encodeOpText(ack);
              sendUplink(22, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
            }
            blinkLed(1);
            broadcast(String("CDK:MSG,TEXT:") + message);
            break;

        // Plaintext ALERT (topic 23) -- NOT authenticated, same forgeable
        // class as plaintext dcmd above. Not currently sent by OpenDMS/
        // Laravel (no server-side caller), but still reachable by anyone
        // sending a raw topic-23 packet over LoRa directly, bypassing
        // MQTT/Laravel entirely. Reject once the device has opted into
        // authenticated operator commands.
        case 23:
            if (opendmsconfig::isConfigured()) {
                logerr_ln("Plaintext ALERT (topic 23) received but OpenDMS key is configured (encryption required), dropping.");
                break;
            }
            if (duckpayload::isProtobuf(packet.data.data(), packet.data.size())) {
              duckcdp_OpText opText = duckcdp_OpText_init_zero;
              if (duckpayload::decodeOpText(packet.data.data(), packet.data.size(), opText)) {
                message = String(opText.text);
              }
            }
            beepBuzzer(3, 80, 80);     // immediate: alert before anything else (no-op on this board)
            flashLED();
            broadcast(String("CDK:MSG,TEXT:") + message);
            {
              duckcdp_OpText ack = buildOpText("ALERT_ACK");
              std::vector<uint8_t> encoded = duckpayload::encodeOpText(ack);
              sendUplink(23, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
            }
            break;

        // Plaintext BROADCAST/announcement (topic 24) -- authenticated (not
        // encrypted) with the mesh group key when configured (see
        // verifyBroadcastMac() above and
        // DuckCryptoService::authenticateGroupBroadcast() on the Laravel
        // side). Deliberately readable by anyone in range -- only forgery
        // is prevented, not confidentiality, since a life-safety alert is
        // meant to be understood even by devices without the group key.
        // encrypted_cmd (topic 8) can't be used instead: it's a
        // point-to-point channel (a different shared secret per Duck) and
        // can't produce a single tag every Duck in a deployment can
        // verify, so the mesh group key is the only broadcast-capable
        // authenticated channel this firmware has.
        //
        // Anyone in LoRa range could otherwise forge an "emergency
        // broadcast" -- once this Duck has a real mesh group key
        // provisioned (meshgroupconfig::isConfigured()), the deployment has
        // opted into authenticated broadcasts, so reject anything that
        // doesn't verify instead of trusting the raw bytes. Devices with
        // no mesh group key provisioned yet fall back to accepting
        // unauthenticated plaintext, same as before this scheme existed.
        case 24: {
            std::string verifiedBroadcast;
            bool broadcastAuthenticated = verifyBroadcastMac(24, packet.data, verifiedBroadcast);
            if (meshgroupconfig::isConfigured() && !broadcastAuthenticated) {
                logerr_ln("BROADCAST (topic 24) received but mesh group key is configured (authentication required) and MAC verification failed, dropping.");
                securityevents::recordBroadcastRejected();
                break;
            }
            String broadcastText;
            if (broadcastAuthenticated) {
                broadcastText = String(verifiedBroadcast.c_str());
            } else {
                broadcastText = message;
                if (duckpayload::isProtobuf(packet.data.data(), packet.data.size())) {
                  duckcdp_OpText opText = duckcdp_OpText_init_zero;
                  if (duckpayload::decodeOpText(packet.data.data(), packet.data.size(), opText)) {
                    broadcastText = String(opText.text);
                  }
                }
            }
            beepBuzzer(3, 80, 80);     // rapid triple = emergency alert (no-op on this board)
            displayAnnouncement(broadcastText);
            blinkLed(1);
            broadcast(String("CDK:BCAST,TEXT:") + broadcastText);
            break;
        }

        // Plaintext PMSG (topic 25) -- NOT authenticated, same forgeable
        // class as plaintext dcmd above. Not currently sent by OpenDMS/
        // Laravel (no server-side caller), but still reachable by anyone
        // sending a raw topic-25 packet over LoRa directly. Reject once
        // the device has opted into authenticated operator commands.
        case 25:
            if (opendmsconfig::isConfigured()) {
                logerr_ln("Plaintext PMSG (topic 25) received but OpenDMS key is configured (encryption required), dropping.");
                break;
            }
            if (duckpayload::isProtobuf(packet.data.data(), packet.data.size())) {
              duckcdp_OpText opText = duckcdp_OpText_init_zero;
              if (duckpayload::decodeOpText(packet.data.data(), packet.data.size(), opText)) {
                message = String(opText.text);
              }
            }
            broadcast(String("CDK:PMSG,TEXT:") + message);
            break;

        // NOTE: bare topic 234 (GPS location request) is intentionally NOT
        // handled here anymore -- it used to be a plaintext, unauthenticated
        // topic that anyone in LoRa range could send to force this duck to
        // broadcast its live location. It's now only reachable via a
        // "CMD:GPS_REQUEST" payload inside an authenticated encrypted_cmd
        // (see the topics::encrypted_cmd case above and
        // handleGpsRequestCommand() below). A bare topic-234 packet simply
        // falls through to the default: case (ignored) below.

        case 26:  // MamaDuck-to-MamaDuck (MTALK)
            if (duck.isMamaLinkEncryptionEnabled() && !packet.wasAuthenticated) {
                logerr_ln("MTALK (topic 26) received but not authenticated (encrypted_data), dropping -- encryption is enabled for this build.");
                break;
            }
            if (duckpayload::isProtobuf(packet.data.data(), packet.data.size())) {
              // ── Protobuf-encoded MTalk (see duck_payloads.proto) ─────────
              duckcdp_MTalk mtalk = duckcdp_MTalk_init_zero;
              if (!duckpayload::decodeMTalk(packet.data.data(), packet.data.size(), mtalk)) {
                break;
              }
              String senderId;
              {
                  char buf9[9]; memcpy(buf9, packet.sduid.data(), 8); buf9[8] = 0;
                  senderId = String(buf9);
              }
              String mid = String(mtalk.mid);
              if (mtalk.kind == duckcdp_MTalkKind_MTALK_ACK) {
                // Delivery receipt coming back to the original sender.
                broadcast("CDK:MACK,ID:" + mid + ",FROM:" + senderId);
              } else {
                // Incoming message.
                String text = String(mtalk.text);
                String frameOut = "CDK:MTALK,TEXT:" + text + ",FROM:" + senderId;
                if (mid.length() > 0) frameOut += ",MID:" + mid;
                broadcast(frameOut);
                // Send targeted delivery receipt back to the original sender.
                if (mid.length() > 0) {
                  std::array<uint8_t, 8> senderDuid;
                  for (int i = 0; i < 8; i++) senderDuid[i] = packet.sduid[i];
                  duckcdp_MTalk ack = buildMTalk(duckcdp_MTalkKind_MTALK_ACK, mid, "");
                  std::vector<uint8_t> encoded = duckpayload::encodeMTalk(ack);
                  if (!encoded.empty()) {
                    std::string encodedStr(reinterpret_cast<const char*>(encoded.data()), encoded.size());
                    sendMamaLink(encodedStr, senderDuid);
                  }
                }
              }
              break;
            }
            // ── Legacy plain-text MTALK (pre-protobuf firmware) ──────────────
            if (message.startsWith("[MACK:")) {
                int   end      = message.indexOf(']', 6);
                String ackId   = (end > 6) ? message.substring(6, end) : "";
                String senderId;
                {
                    char buf9[9]; memcpy(buf9, packet.sduid.data(), 8); buf9[8] = 0;
                    senderId = String(buf9);
                }
                broadcast("CDK:MACK,ID:" + ackId + ",FROM:" + senderId);
            } else {
                String text = message;
                String mid  = "";
                int midIdx  = message.lastIndexOf(",MID:");
                if (midIdx >= 0 && (int)(message.length() - midIdx) == 9) {
                    mid  = message.substring(midIdx + 5);
                    text = message.substring(0, midIdx);
                }
                String senderId;
                {
                    char buf9[9]; memcpy(buf9, packet.sduid.data(), 8); buf9[8] = 0;
                    senderId = String(buf9);
                }
                String frameOut = "CDK:MTALK,TEXT:" + text + ",FROM:" + senderId;
                if (mid.length() > 0) frameOut += ",MID:" + mid;
                broadcast(frameOut);
                if (mid.length() > 0) {
                    std::array<uint8_t, 8> senderDuid;
                    for (int i = 0; i < 8; i++) senderDuid[i] = packet.sduid[i];
                    sendMamaLink(std::string(("[MACK:" + mid + "]").c_str()), senderDuid);
                }
            }
            break;

        case TOPIC_BEACON: {
            if (meshgroupconfig::isConfigured() && !beaconAuthenticated) {
                logerr_ln("TOPIC_BEACON received but mesh group key is configured (encryption required), dropping.");
                securityevents::recordBeaconRejected();
                break;
            }
            if (memcmp(packet.sduid.data(), duck.getDuckId().data(), 8) == 0) break;
            if (!beaconAckPending && !gpsTxPending) {
                char ownGps[80] = {};
                if (tinyGps.location.isValid() && tinyGps.location.age() < 30000) {
                    snprintf(ownGps, sizeof(ownGps), "GPS,LAT:%.6f,LNG:%.6f",
                             tinyGps.location.lat(), tinyGps.location.lng());
                } else if (phoneGpsLatBuf[0] != '\0') {
                    snprintf(ownGps, sizeof(ownGps), "GPS,LAT:%s,LNG:%s",
                             phoneGpsLatBuf, phoneGpsLngBuf);
                } else {
                    strncpy(ownGps, "GPS,FIX:0", sizeof(ownGps) - 1);
                    if (isPhoneConnected() && gpsReqDeferredSendMs == 0)
                        gpsReqDeferredSendMs = millis() + 300;
                }
                strncpy(beaconAckPayload, ownGps, sizeof(beaconAckPayload) - 1);
                beaconAckPending = true;
            }
            break;
        }

        case TOPIC_BEACON_ACK:
            if (meshgroupconfig::isConfigured() && !beaconAuthenticated) {
                logerr_ln("TOPIC_BEACON_ACK received but mesh group key is configured (encryption required), dropping.");
                securityevents::recordBeaconRejected();
                break;
            }
            if (memcmp(packet.sduid.data(), duck.getDuckId().data(), 8) == 0) break;
            // GPS extracted in section 1; CDK:SEEN emitted in section 2.
            break;
    }
}

// ── Display helpers ───────────────────────────────────────────────────────────
void displayHome() {
    gDisplay.begin();
    gDisplay.drawStr(0, 0, (TXT_BATT_LABEL + String(batteryPercent(readVbat())) + "%").c_str());
    gDisplay.drawStrRight(0, idBuf);
    // Signal / TX status (no percentage — keeps the line short)
    String sigStr = signalQualityLabel(lastSignalPct, lastTxResult);
    gDisplay.drawStrCenter(13, sigStr.c_str());
    // GPS status
    char gpsLine[22];
    if (!gpsModuleDetected) {
        strncpy(gpsLine, TXT_GPS_NO_MODULE, sizeof(gpsLine));
    } else if (tinyGps.location.isValid() && tinyGps.location.age() < 5000UL) {
        snprintf(gpsLine, sizeof(gpsLine), TXT_GPS_FIX_FMT,
                 (unsigned)(tinyGps.satellites.isValid() ? tinyGps.satellites.value() : 0));
    } else {
        snprintf(gpsLine, sizeof(gpsLine), TXT_GPS_SEARCH_FMT,
                 (unsigned)(tinyGps.satellites.isValid() ? tinyGps.satellites.value() : 0));
    }
    gDisplay.drawStrCenter(26, gpsLine);
    gDisplay.drawStrCenter(40, TXT_PRESS_BUTTON_ABOVE);
    gDisplay.drawStrCenter(53, TXT_2SEC_EMERGENCY);
    gDisplay.end();
}

void displayID() {
    gDisplay.drawStrRight(0, idBuf);
    // Note: caller must wrap with gDisplay.begin()/end() if needed.
}

void displayBatt() {
    gDisplay.drawStr(0, 0, (TXT_BATT_LABEL + String(batteryPercent(readVbat())) + "%").c_str());
}

void displayMessage(String msg) {
    msg.toUpperCase();
    gDisplay.begin();
    displayID();
    gDisplay.drawStrMaxWidth(0, 12, 128, msg.c_str());
    gDisplay.end();
    messagePending  = true;
    displayEnabled  = true;
}

void displayAnnouncement(const String& msg) {
    String upper = msg;
    upper.toUpperCase();
    gDisplay.powerSave(false);
    gDisplay.begin();
    displayID();
    displayBatt();
    gDisplay.drawStrCenter(12, TXT_EMERGENCY_MESSAGE_HEADER);
    gDisplay.drawStrMaxWidth(0, 26, 128, upper.c_str());
    gDisplay.end();
    emergencyDisplayPending = true;
    displayEnabled          = true;
}

// ── LED ───────────────────────────────────────────────────────────────────────
void flashLED() {
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED1, HIGH);
        delay(200);
        digitalWrite(PIN_LED1, LOW);
        delay(200);
    }
}

void blinkLed(int times) {
    for (int n = 0; n < times; n++) {
        digitalWrite(PIN_LED1, HIGH);
        { unsigned long t = millis(); while (millis() - t < 200) { duck.run(); delay(5); } }
        digitalWrite(PIN_LED1, LOW);
        { unsigned long t = millis(); while (millis() - t < 200) { duck.run(); delay(5); } }
    }
}

// No buzzer hardware on this board (per Seeed's spec sheet: power/reset/
// user-defined buttons only, no buzzer) -- unlike WioTrackerL1's D12
// passive buzzer. No-op so every call site elsewhere in this file (SOS
// hold feedback, click ticks, alert tones, etc.) compiles unchanged;
// visual feedback is still provided via blinkLed()/flashLED() at the same
// call sites.
void beepBuzzer(int /*times*/, int /*onMs*/, int /*offMs*/) {}

// ── Battery ───────────────────────────────────────────────────────────────────
void sendBattery() {
    int pct = batteryPercent(readVbat());
    broadcast("CDK:BATT,LEVEL:" + String(pct));
}

// ── SOS ───────────────────────────────────────────────────────────────────────
bool sendEmergency(String lat, String lng, String alt, String spd, String hdg, bool gpsFromPhone) {
    bool hasGps  = (lat.length() > 0 && lng.length() > 0);
    int  battPct = batteryPercent(readVbat());

    // Protobuf-encode the alert (see duck_payloads.proto: SosAlert) --
    // DeviceID is carried in the MQTT envelope by the gateway so we don't
    // need to repeat it in the payload bytes.
    duckcdp_SosAlert alertMsg = buildSosAlert(duckcdp_SosOrigin_SOS_ORIGIN_DEVICE,
        gpsFromPhone ? duckcdp_GpsSource_GPS_SOURCE_PHONE : duckcdp_GpsSource_GPS_SOURCE_DEVICE,
        lat, lng, alt, spd, hdg, battPct, currentRssiDbm());
    // Satellite count only comes from the onboard GPS module -- phone-relayed
    // fixes (gpsFromPhone == true) don't carry a sat count, so sats stays 0.
    if (!gpsFromPhone) {
        alertMsg.sats = tinyGps.satellites.isValid() ? tinyGps.satellites.value() : 0;
    }
    std::vector<uint8_t> encoded = duckpayload::encodeSos(alertMsg);

    // Fail-safe (not fail-closed) for SOS: falls back to cleartext as a last
    // resort if sealing fails, since dropping an emergency alert is worse
    // than leaking location for this specific flow.
    int failure = sendUplinkSos(topics::alert, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
    lastTxResult = failure;
    lastTxMs     = millis();

    if (!failure) {
        counter++;
        dspPowerSave(0);
        blinkLed(3);
        String sosFrame = String("CDK:SOS,SRC:DEVICE,ID:") + DUCK_ID_BUF +
                                 ",LAT:" + (hasGps ? lat : "none") +
                          ",LNG:" + (hasGps ? lng : "none");
        if (hasGps && alt.length() > 0) sosFrame += ",ALT:" + alt;
        if (hasGps && spd.length() > 0) sosFrame += ",SPD:" + spd;
        if (hasGps && hdg.length() > 0) sosFrame += ",HDG:" + hdg;
        if (hasGps && gpsFromPhone)      sosFrame += ",GPS:PHONE";
        sosFrame += ",BATT:" + String(battPct);
        broadcast(sosFrame);
        dspBegin();
        dspStrRight(0, idBuf);
        displayBatt();
        dspStrCenter(22, TXT_SEND_OK);
        dspStrCenter(34, TXT_EMERGENCY_SIGNAL);
        dspStrCenter(46, hasGps ? TXT_WITH_GPS : TXT_WITHOUT_GPS);
        dspEnd();
        blinkLed(2);
        // (no "sent, but no location" buzzer warning -- no buzzer hardware;
        // the blinkLed(2) above and lack of LAT/LNG in the broadcast frame
        // are the only feedback available on this board)
        {
            unsigned long endMs = millis() + 2000UL;
            while (millis() < endMs) { duck.run(); delay(10); }
        }
        displayHome();
    } else {
        dspBegin();
        dspStrRight(0, idBuf);
        displayBatt();
        dspStrCenter(22, TXT_SOS_ERR_CANNOT);
        dspStrCenter(34, TXT_SEND_SIGNAL);
        dspStrCenter(46, TXT_EMERGENCY);
        dspEnd();
        beepBuzzer(2, 60, 60);      // SOS failed — no-op on this board
    }
    return true;
}

// ── Phone connection detection ────────────────────────────────────────────────
static bool isPhoneConnected() {
    return usbPhoneSeen || blePhoneSeen;
}

// ── Broadcast on all active channels ─────────────────────────────────────────
void broadcast(const String& frame) {
    String payload = frame.endsWith("\n") ? frame : frame + "\n";
    Serial.print(payload);
    bleSendLine(payload);
}

// ── Frame dispatcher ──────────────────────────────────────────────────────────
void handleFrame(const String& line) {
    if (!line.startsWith("CDK:")) return;
    broadcast(String("CDK:ID,VALUE:") + DUCK_ID_BUF);
    String body   = line.substring(4);
    int    comma  = body.indexOf(',');
    String type   = (comma == -1) ? body : body.substring(0, comma);

    enum FrameType { FT_UNKNOWN, FT_SOS, FT_MSG, FT_PING, FT_MTALK, FT_GPS, FT_BYE, FT_SCAN, FT_RADIOREGION };
    FrameType ft = FT_UNKNOWN;
    if      (type == "SOS")   ft = FT_SOS;
    else if (type == "MSG")   ft = FT_MSG;
    else if (type == "PING")  ft = FT_PING;
    else if (type == "MTALK") ft = FT_MTALK;
    else if (type == "GPS")   ft = FT_GPS;
    else if (type == "BYE")   ft = FT_BYE;
    else if (type == "SCAN")  ft = FT_SCAN;
    else if (type == "RADIOREGION") ft = FT_RADIOREGION;

    switch (ft) {
        case FT_SOS:   handleSOS(body);   break;
        case FT_MSG:   handleMsg(body);   break;
        case FT_PING:  /* ID already broadcast */ break;
        case FT_MTALK: handleMamaTalk(body); break;
        case FT_GPS:   handleGps(body);   break;
        case FT_RADIOREGION: handleRadioRegion(body); break;
        case FT_SCAN: {
            char gpsPayload[80] = {};
            if (tinyGps.location.isValid() && tinyGps.location.age() < 30000) {
                snprintf(gpsPayload, sizeof(gpsPayload), "GPS,LAT:%.6f,LNG:%.6f",
                         tinyGps.location.lat(), tinyGps.location.lng());
            } else if (phoneGpsLatBuf[0] != '\0') {
                snprintf(gpsPayload, sizeof(gpsPayload), "GPS,LAT:%s,LNG:%s",
                         phoneGpsLatBuf, phoneGpsLngBuf);
            } else {
                strncpy(gpsPayload, "GPS,FIX:0", sizeof(gpsPayload) - 1);
                if (isPhoneConnected() && gpsReqDeferredSendMs == 0 && gpsReqSentMs == 0)
                    gpsReqDeferredSendMs = millis() + 300;
            }
            std::string beaconWire = meshgroupconfig::isConfigured()
                ? encryptBeaconPayload(TOPIC_BEACON, gpsPayload)
                : std::string(gpsPayload);
            int beaconResult = duck.sendData(TOPIC_BEACON, beaconWire, BROADCAST_DUID);
            broadcast(beaconResult == 0 ? "CDK:STATUS,SCAN:ping_sent" : "CDK:STATUS,SCAN:ping_failed");
            broadcast("CDK:SCAN_ACK");
            break;
        }
        case FT_BYE:
            usbPhoneSeen = false; usbDisconnectDisplayPending = true;
            break;
        default: break;
    }
}

// ── SOS from phone ────────────────────────────────────────────────────────────
void handleSOS(const String& body) {
    String lat = extractField(body, "LAT");
    String lng = extractField(body, "LNG");
    String alt = extractField(body, "ALT");
    String spd = extractField(body, "SPD");
    String hdg = extractField(body, "HDG");
    int battPct = batteryPercent(readVbat());

    dspPowerSave(0);
    dspBegin();
    dspStrRight(0, idBuf);
    displayBatt();
    dspStrCenter(22, TXT_SENDING);
    dspStrCenter(34, TXT_EMERGENCY_SIGNAL_DOTS);
    dspEnd();

    // Protobuf-encode the alert (see duck_payloads.proto: SosAlert, wrapped
    // in a StatusReport on the `status` topic).
    duckcdp_SosAlert alertMsg = buildSosAlert(duckcdp_SosOrigin_SOS_ORIGIN_PHONE,
        duckcdp_GpsSource_GPS_SOURCE_PHONE, lat, lng, alt, spd, hdg,
        battPct, currentRssiDbm());
    std::vector<uint8_t> encoded = duckpayload::encodeStatusReportSos(alertMsg);

    int failure = sendUplinkSos(topics::status, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
    blinkLed(3);
    broadcast("CDK:ACK,ID:SOS");

    dspPowerSave(0);
    dspBegin();
    dspStrRight(0, idBuf);
    displayBatt();
    dspStrCenter(22, TXT_SEND_OK);
    dspStrCenter(34, TXT_EMERGENCY_SIGNAL);
    dspStrCenter(46, TXT_WITH_GPS);
    dspEnd();
    emergencyDisplayPending = true;
    displayEnabled          = true;
}

// ── Message from phone ────────────────────────────────────────────────────────
void handleMsg(const String& body) {
    String urgency = extractField(body, "URGENCY");
    String lat     = extractField(body, "LAT");
    String lng     = extractField(body, "LNG");
    String text    = extractField(body, "TEXT");

    // Protobuf-encode the message (see duck_payloads.proto: StatusMsg,
    // wrapped in a StatusReport on the `status` topic).
    duckcdp_StatusMsg statusMsg = buildStatusMsg(urgency, lat, lng, text);

    dspPowerSave(0);
    dspBegin();
    dspStrRight(0, idBuf);
    displayBatt();
    dspStrCenter(22, TXT_MSG_SENT);
    dspEnd();
    blinkLed(1);
    displayHome();

    std::vector<uint8_t> encoded = duckpayload::encodeStatusReportMsg(statusMsg);
    int failure = sendUplink(topics::status, std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size()));
    if (!failure) broadcast("CDK:ACK,ID:MSG");
}

// ── MamaDuck-to-MamaDuck talk ─────────────────────────────────────────────────
bool sendMamaTalk(const String& targetId, const String& msg, const String& mid) {
    if (targetId.length() != 8) return false;
    std::array<uint8_t, 8> targetDuid =
        duckutils::stringToArray<uint8_t, 8>(std::string(targetId.c_str()));
    // Protobuf-encode the chat message (see duck_payloads.proto: MTalk). The
    // receiver echoes `mid` back as a targeted delivery receipt (MTALK_ACK on
    // topic 26) when one is present.
    duckcdp_MTalk mtalk = buildMTalk(duckcdp_MTalkKind_MTALK_MSG, mid, msg);
    std::vector<uint8_t> encoded = duckpayload::encodeMTalk(mtalk);
    if (encoded.empty()) return false;
    std::string encodedStr(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    int failure = sendMamaLink(encodedStr, targetDuid);
    if (!failure) broadcast("CDK:ACK,ID:MTALK,TARGET:" + targetId);
    return !failure;
}

void handleMamaTalk(const String& body) {
    String target = extractField(body, "TARGET");
    String text   = extractField(body, "TEXT");
    String mid    = extractField(body, "MID");
    if (target.length() == 0) return;
    sendMamaTalk(target, text, mid);
}

// ── GPS from phone ────────────────────────────────────────────────────────────
void handleGps(const String& body) {
    gpsReqSentMs = 0;
    PhoneGpsParseResult result = parsePhoneGpsFrame(body,
        phoneGpsLatBuf, sizeof(phoneGpsLatBuf), phoneGpsLngBuf, sizeof(phoneGpsLngBuf),
        phoneGpsAltBuf, sizeof(phoneGpsAltBuf), phoneGpsSpdBuf, sizeof(phoneGpsSpdBuf),
        phoneGpsHdgBuf, sizeof(phoneGpsHdgBuf),
        batteryPercent(readVbat()), currentRssiDbm());
    phoneGpsNoFix          = !result.hasFix;
    phoneGpsDisplayPending = true;
    gpsTxPayload = duckpayload::encodeGps(result.reading);
    gpsTxPending = true;
}

// Handles CDK:RADIOREGION frames from the app (mobile-app settings screen):
// with a VALUE field, sets/persists the LoRa region preset via
// RadioRegionConfig; without one, reports the currently active region. A
// region change only takes effect on air after the radio is
// re-initialized with the new band, so a successful write's ACK includes
// REBOOT_REQUIRED:1 and the device auto-reboots shortly after sending it.
void handleRadioRegion(const String& body) {
    String value = extractField(body, "VALUE");
    if (value.length() == 0) {
        broadcast(String("CDK:RADIOREGION,VALUE:") +
                  radioregionconfig::regionName(radioregionconfig::getCurrentRegion()));
        return;
    }
    radioregionconfig::RadioRegion region;
    if (!radioregionconfig::regionFromName(value.c_str(), &region)) {
        broadcast("CDK:RADIOREGION,ERROR:unknown_region");
        return;
    }
    int rc = radioregionconfig::setRegion(region);
    if (rc == DUCK_ERR_NONE) {
        broadcast(String("CDK:RADIOREGION,VALUE:") + radioregionconfig::regionName(region) +
                  ",STATUS:ok,REBOOT_REQUIRED:1");
        // Diagnostic only: 2 LED blinks confirms we reached this point without
        // touching Serial -- Serial.println()/flush() here can block forever if
        // the USB CDC TX buffer is full and unread (see setup()'s rationale for
        // why success-path debug prints were removed there too).
        BLINK_LED(2);
        delay(300);  // let the BLE notification/serial line flush before resetting
        duckesp::restartDuck();
    } else {
        broadcast("CDK:RADIOREGION,ERROR:write_failed");
    }
}

// extractField() now lives in examples/Basic-Ducks/common/CdkFrame.h
// (shared with Heltec and future boards).
