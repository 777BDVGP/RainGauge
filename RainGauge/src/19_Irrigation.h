#pragma once

// =============================================================================
// 19_Irrigation.h — "Irrigate today?" Answer for an irrigation controller
// =============================================================================
// GET /api/irrigate  ->  {"irrigate":true|false,"reason":"...","rain_mm":x,
//                         "hours":y,"limit_mm":z,"sim":false,"time":"..."}
//
// The irrigation controller polls this (e.g. every hour) and copies "irrigate" into its own
// RAIN_OK_DO_IRRI flag, which its scheduler checks for zones set to
// "Respect Rainfall = Yes".
//
// Current rule (same as the old ESP8266 firmware's ActiveRule):
//   rain in the last Params->IrriSkipHours hours >= Params->IrriSkipMm
//   -> irrigate = false  (skip), otherwise true.
// Settings "Irrigation Answer Method" chooses between this Simple Rule and
// the FAO-56 water balance (20_WaterBalance.cpp — the default).
// =============================================================================

struct IrriAnswer {
	bool   irrigate = true;
	float  rainMm   = 0;     // rain in the look-back window
	String reason   = "";
};
