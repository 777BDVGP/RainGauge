#pragma once

// =============================================================================
// 16_Rain.h — Tipping-Bucket Counter, Calibration & Rainfall Log
// =============================================================================
// TIP DETECTION
//   The bucket has two equal sides.  With one side down the IR beam is
//   broken, with the other side down it is open — so EVERY tip flips the
//   pickup.  Each debounced change of the pickup (either direction) = 1 tip,
//   as in the old firmware.  Which side breaks the beam does not matter.
//
// CALIBRATION  (Settings -> CALIBRATION, /calibrate)
//   Pour Params->CalWaterMl (500 ml) slowly into the funnel; the page counts
//   the tips (no rain is logged meanwhile).  CALCULATE gives
//       ml per tip = water ml / tips          (needs >= CAL_MIN_TIPS tips)
//   and SAVE stores it in Params->MlPerTip.  mm per tip follows from the
//   funnel area, so a funnel change does not need a new calibration:
//       mm per tip = ml per tip x 1000 / (pi x (FunnelDiaMm / 2)^2)
//   146 mm funnel: 1 mm rain = 16.74 ml.
//   The two sides average out — a pour always tips them alternately.
//
// RAINFALL LOG  (/RainfallLog.txt — same format as the old ESP8266 firmware,
//   so the old "RainfallLog (MASTER).txt" can be uploaded and kept)
//   Tips are gathered over RAIN_CAPTURE_SEC windows; a window with rain
//   appends one fixed-length record:
//       YYYYMMDDHH + mm in "%8.3f"  ->  18 chars + CR/LF = 20 bytes
//       e.g. "2026100114   0.299"
// =============================================================================

	#define RAIN_LOG_FILE        "/RainfallLog.txt"
	#define RAIN_CAPTURE_SEC     30        // tips are logged per 30 s window
	#define CAL_MIN_TIPS         50        // fewer tips -> calibration refused
	#define CAL_TIMEOUT_MS       (3UL * 60UL * 1000UL)   // calibration ends 3 min after the last tip

// ─── Live rain state (read by Home / Reports) ────────────────────────────────
	inline uint32_t RainTipsSinceBoot = 0;
	inline uint32_t RainPendingTips   = 0;     // tips in the current window, not yet logged
	inline float    RainTodayLogged   = 0;     // mm in the log for today
	inline float    RainMonthLogged   = 0;     // mm in the log for this month
	inline String   RainLastTip       = "-";   // "YYYY-MM-DD HH:MM" of the last tip
	inline uint32_t RainLastTipMs     = 0;     // millis() of the last tip (IR self-test waits for a still bucket)

// ─── Calibration state ───────────────────────────────────────────────────────
	inline bool     CalActive  = false;
	inline volatile bool CalStopRequested = false;  // POST /calstop (leaving the CALIBRATION page; async_tcp -> loop)
	inline uint32_t CalTips    = 0;
	inline float    CalResult  = 0;            // ml per tip from the last CALCULATE (0 = none)
	inline String   CalStatus  = "Idle";
