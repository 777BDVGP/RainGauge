#pragma once

// =============================================================================
// 17_Temperature.h — DS18S20 Internal / External Temperatures
// =============================================================================
// Two separate 1-Wire buses, one sensor each:
//   EXTERNAL  GPIO 6  (R3 4k7)       INTERNAL  GPIO 1  (R1 4k7, hand-wired)
// Non-blocking: a conversion is requested, and read TEMP_PERIOD_MS later
// (the DS18S20 needs 750 ms), then the next one is requested.
// A missing / unplugged sensor reads NAN and shows "--" on the pages.
// =============================================================================

	#define TEMP_PERIOD_MS   10000UL   // read both sensors every 10 s

	inline float TempExternal = NAN;   // °C
	inline float TempInternal = NAN;   // °C
