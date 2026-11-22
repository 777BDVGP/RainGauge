#pragma once

// =============================================================================
// wifi_secrets.example.h — OPTIONAL template for wifi_secrets.h
// =============================================================================
// You do NOT need this file: without wifi_secrets.h the gauge starts the
// "RainGauge-Setup" WiFi hotspot and you choose your network on a phone.
//
// Only if you want networks built into the firmware (e.g. you build it
// yourself for a few known places): copy this file to wifi_secrets.h (same
// folder) and fill it in.  wifi_secrets.h is in .gitignore, so it never goes
// into Git.  Each entry: SSID, password, fixed IP the gauge takes there.
// =============================================================================

	#define WIFI_KNOWN_NETS \
		{ "MyHomeWiFi",  "my-password",  { 192, 168, 1, 124 } }, \
		{ "OtherWiFi",   "other-pass",   { 192, 168, 0,  50 } },

	// Optional: start on South African time instead of Greenwich (GMT) when no
	// time zone has been chosen yet on the SETTINGS page.
	// #define PRIVATE_DEFAULT_TZ_SAST
