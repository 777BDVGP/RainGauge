#pragma once

// =============================================================================
// 03_Files.h — SPIFFS File System Globals & Web File-Manager Definitions
// =============================================================================

// ─── File Action Status Message ───────────────────────────────────────────────
	inline int    SetMess    = 0;
	inline String SpesialStr = "";

// ─── Global SPIFFS File Handles ───────────────────────────────────────────────
	inline File   f;
	inline File   ftemp;
	inline String line;

// ─── SPIFFS Filename Constants ────────────────────────────────────────────────
	inline String StrFormatComplete = F("/FormatComplete.txt");
	inline String StrGeneralLog     = F("/GeneralLog.txt");
	inline String StrWiFiDefault    = F("/WiFiDefault.json");   // OLD last-good-network file (removed once /WiFiConfig.json exists)
	inline String StrWiFiConfig     = F("/WiFiConfig.json");    // WiFi network + password + IP (05_WiFi.cpp) — never shown on FILES

	// The WiFi password file is never listed, viewed or downloaded.
	inline bool IsProtectedFile(String path){ if (!path.startsWith("/")) path = "/" + path; return path == StrWiFiConfig || path == "/Access.json"; }
