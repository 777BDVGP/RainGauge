#pragma once

// =============================================================================
// 05_WiFi.h — WiFi join (saved config / known networks) + setup hotspot
// =============================================================================
// SetupWiFi() (called once from setup()):
//
//   1. Nothing saved and no built-in networks (public firmware, new gauge)
//      -> setup hotspot "RainGauge-Setup" (open, http://192.168.4.1/, no
//      time limit).  A phone shows the setup page by itself (captive DNS).
//   2. /WiFiConfig.json holds a LIST of networks (max 8, newest first):
//      scan, try the saved ones in range (strongest first), then the rest.
//   3. Private firmware only: scan for the KNOWN_NETS from wifi_secrets.h,
//      join the strongest on its fixed IP and ADD it to /WiFiConfig.json.
//   4. Steps 2-3 are tried WIFI_ROUNDS times; if nothing joins, the setup
//      hotspot opens for 5 minutes (maybe the WiFi password changed), then
//      the gauge reboots and tries again.
//
// The setup hotspot ADDS the network it is given (same SSID = replaced).
// SETTINGS "WiFi Networks" lists the saved networks (never the passwords),
// with DELETE and ADD.  There is no button to start the hotspot on purpose:
// it only opens when no saved network can be reached.
//
// The setup hotspot always ends in a reboot (save, cancel or timeout).  It
// uses DNSServer (which has its own small UDP task) — only while setting up,
// never in normal operation.
// =============================================================================

// Filled in by SetupWiFi() — shown on the ABOUT page.
	inline String WiFiSource = "";   // "saved" or "known network"
