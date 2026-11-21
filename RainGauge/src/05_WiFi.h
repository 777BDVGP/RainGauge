#pragma once

// =============================================================================
// 05_WiFi.h — WiFi join (saved config / known networks) + setup hotspot
// =============================================================================
// SetupWiFi() (called once from setup()):
//
//   0. /WiFiSetup.flag present (ABOUT "WIFI SETUP" button) -> setup hotspot.
//   1. Nothing saved and no built-in networks (public firmware, new gauge)
//      -> setup hotspot "RainGauge-Setup" (open, http://192.168.4.1/, no
//      time limit).  A phone shows the setup page by itself (captive DNS).
//   2. Join the network in /WiFiConfig.json (DHCP or fixed IP).
//   3. Private firmware only: scan for the KNOWN_NETS from wifi_secrets.h,
//      join the strongest on its fixed IP and save it in /WiFiConfig.json.
//   4. Steps 2-3 are tried WIFI_ROUNDS times; if nothing joins, the setup
//      hotspot opens for 5 minutes (maybe the WiFi password changed), then
//      the gauge reboots and tries again.
//
// The setup hotspot always ends in a reboot (save, cancel or timeout).  It
// uses DNSServer (which has its own small UDP task) — only while setting up,
// never in normal operation.
// =============================================================================

// Filled in by SetupWiFi() — shown on the ABOUT page.
	inline String WiFiSource = "";   // "saved" or "known network"
