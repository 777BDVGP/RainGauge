#pragma once

// =============================================================================
// 21_TempHistory.h — Daily Min/Max Temperature History & Records
// =============================================================================
// Every reading (10 s, 17_Temperature.cpp) updates today's min/max for the
// outside (EXTERNAL) and inside (INTERNAL) DS18S20, and the all-time records.
// When the day changes, one line is appended to /TempHistory.txt:
//     YYYY-MM-DD,OutMin,OutMax,InMin,InMax      (blank = no reading that day)
// The file is trimmed to TEMP_HIST_DAYS lines at midnight (02_Time.cpp).
//
// Records (highest / lowest ever, with date) live in /TempHistory.json with
// today's running values, and can be reset from the REPORTS page — like the
// old firmware's RESET TEMPERATURES button.
//
// Simulation uses /SimTempHistory.txt and /SimTempHistory.json, so playing
// never touches the real history.
// =============================================================================

	#define TEMP_HIST_FILE       "/TempHistory.txt"
	#define TEMP_STATE_FILE      "/TempHistory.json"
	#define SIM_TEMP_HIST_FILE   "/SimTempHistory.txt"
	#define SIM_TEMP_STATE_FILE  "/SimTempHistory.json"
	#define TEMP_HIST_DAYS       1100     // ~3 years of daily lines

struct MinMax {
	float  lo = NAN, hi = NAN;
	void add(float c){
		if (isnan(c)) return;
		if (isnan(lo) || c < lo) lo = c;
		if (isnan(hi) || c > hi) hi = c;
	}
};

struct TempRecord {
	float  value = NAN;
	String when  = "";      // "YYYY-MM-DD HH:MM"
};

struct TempHistoryState {
	String     dayKey = "";  // YYYY-MM-DD being collected
	MinMax     out, in;      // today so far
	TempRecord outLo, outHi, inLo, inHi;
	String     since  = "";  // records kept since (date of last reset)
};

	inline TempHistoryState TH;
