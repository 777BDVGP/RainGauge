#pragma once

// =============================================================================
// 15_About.h — OTA Update Service & About Page
// =============================================================================
// Includes ArduinoOTA and declares the OTA state variables exported from
// 15_About.cpp.
// =============================================================================

#include <ArduinoOTA.h>

// ─── OTA State (defined in 15_About.cpp) ─────────────────────────────────────
extern bool OTAInProgress;       // true while an OTA upload is in progress
extern int  OTAPercentComplete;  // Upload progress 0–100 %
