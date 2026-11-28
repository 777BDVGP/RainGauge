#pragma once

// Firmware version — shown on the ABOUT page.  Bump it with every change you upload.
#define FW_VERSION "1.12.1"

// =============================================================================
// 01_Main.h — Global Definitions, System Libraries & Core State Variables
// =============================================================================

// ─── Runtime Object Presence Flags ───────────────────────────────────────────
	inline bool MainPageObject       = false;  // Home page PageVar list allocated

// ─── Board Revision ──────────────────────────────────────────────────────────
	#define BRD_VERSION 1

// ─── Debug / Production Switch ───────────────────────────────────────────────
// Set DEBUG 1 for development (Serial output enabled).
// Set DEBUG 0 for production  (all DEBUG_PRINT* calls compile to nothing).
	#define DEBUG 1

	// `if (Serial)` (USBCDC::operator bool) is false whenever no USB terminal
	// is attached.  Gating every write on it keeps a headless board (USB
	// powered, nothing listening) from blocking in the CDC write/flush and
	// stalling the 10 ms loop — heartbeat + web.
	#if DEBUG
		#define DEBUG_PRINT(x)          do { if (Serial) Serial.print(x);   } while (0)
		#define DEBUG_PRINTLN(x)        do { if (Serial) Serial.println(x); } while (0)
		#define DEBUG_PRINTF(x, ...)    do { if (Serial) Serial.printf(x, ##__VA_ARGS__); } while (0)
	#else
		#define DEBUG_PRINT(x)
		#define DEBUG_PRINTLN(x)
		#define DEBUG_PRINTF(x, ...)
	#endif

// ─── Third-Party Library Includes ────────────────────────────────────────────
	#include <WiFi.h>
	#include <WiFiUdp.h>
	#include <SPIFFS.h>
	#include <ESPAsyncWebServer.h>
	#include <ArduinoJson.h>
	#include <Ticker.h>

// ─── Board-Specific Pin Aliases ──────────────────────────────────────────────
	#if defined(ARDUINO_ARCH_ESP32)
		// S2 Mini on-board blue LED (GPIO 15, "OB_LED" on the PCB symbol),
		// active HIGH.  Used as the heartbeat: slow blink = running,
		// fast flicker = searching for / joining a WiFi network.
		#define LEDOUT   15
	#endif

	inline bool HeartbeatState = false;   // current heartbeat LED on/off state

// ─── Heap Monitor Thresholds ─────────────────────────────────────────────────
	#define MIN_FREE_HEAP    10000
	#define HEAP_FREE_MIN    18000
	#define REFUSE_REBOOT_MS 30000UL   // reboot when pages are refused (503 "Low memory") for 30 s in a row (1.12.0)
	#define REFUSE_GAP_MS     5000UL   // no refusal for 5 s = the streak is over
	inline volatile uint32_t Refuse503FirstMs = 0;   // start of the current refusal streak (0 = none) — set in async_tcp
	inline volatile uint32_t Refuse503LastMs  = 0;   // last refusal
	#define HEAP_MAXBLK_MIN   7000   // a page needs ~6 KB (send buffer; the page itself is in PSRAM) — was 10000 until 1.12.0

// ─── Gateway Reachability Guard (MaintainGatewayWatch, 01_Main.cpp) ─────────
// WiFi.status() alone can keep reporting WL_CONNECTED while the device has
// silently dropped off the network (no ping, no ARP reply). Pinging the
// router proves the link actually carries traffic; reboot if it goes silent.
// The ping ALSO acts as a keepalive so the AP/router doesn't age this client out.
	#define GW_PING_INTERVAL_MS   15000UL    // One ping to the gateway every 15 s
	#define GW_PING_TIMEOUT_MS     2000UL    // Per-ping reply timeout
	#define GW_UNREACHABLE_MS     60000UL    // Reboot after 1 min with no reply (was 5 min until 1.12.0)

// ─── Watchdog Variables ───────────────────────────────────────────────────────
	inline Ticker secondTick;
	inline volatile int watchdogCount = 0;
	inline String localIP;
	inline long int cycles = 0;
