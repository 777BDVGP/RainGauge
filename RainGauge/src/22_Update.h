#pragma once

// =============================================================================
// 22_Update.h — Firmware update from GitHub (ABOUT)
// =============================================================================
// ABOUT page, "FIRMWARE UPDATE":
//
//   CHECK FOR UPDATE  reads FW_UPDATE_URL "version.json" from GitHub Pages:
//                     {"version":"1.8.0","bin":"firmware/raingauge.bin","notes":"..."}
//   INSTALL           downloads that .bin into the spare OTA slot and restarts.
//   (Uploading a .bin from a PC / phone — POST /update — was removed in 1.12.1:
//   firmware only comes from GitHub, or from a PC with PlatformIO / USB.)
//
// INSTALL writes the NEW firmware into the spare app partition (2 x 1.5 MB,
// partitions_ota.csv); the gauge only switches to it when the whole image
// arrived and verified.  A broken download / wrong file leaves the running
// firmware untouched.  Settings, calibration and the rain history (SPIFFS)
// are never touched.  A file must be an ESP32-S2 image AND contain
// FW_MARKER, so firmware of another project is refused.
//
// HTTPS: only the roots in GH_ROOT_CERTS are trusted (Let's Encrypt ISRG X1/X2
// for github.io, DigiCert G2 + USERTrust as spares) — the certificate IS
// checked.  The clock must be set (NTP).  mbedTLS buffers go to PSRAM while
// downloading (internal heap is too small).
//
// The GitHub check / install run in loop() and BLOCK it (HTTPClient) — the
// one accepted exception to "no blocking network calls": started by hand,
// takes ~20-60 s, tips in that time are not counted.  The watchdog is fed.
// =============================================================================

	#define FW_UPDATE_URL   "https://777bdvgp.github.io/RainGauge/"   // GitHub Pages of the public repo
	#define FW_MARKER       "#RAINGAUGE-S2-FIRMWARE"                   // must be in every rain gauge .bin ('#' once)

	inline String FwLatest   = "-";     // version on GitHub (after CHECK)
	inline String FwBinPath  = "";      // "firmware/raingauge.bin" from version.json
	inline String FwNotes    = "";
	inline String FwStatus   = "-";     // shown live on ABOUT
	inline volatile bool FwBusy = false;  // an update (GitHub or upload) is running
