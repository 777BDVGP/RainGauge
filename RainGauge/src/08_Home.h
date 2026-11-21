#pragma once

// =============================================================================
// 08_Home.h — Home Page Globals & Display Constants
// =============================================================================

// ─── Home Page Row Index Constants ───────────────────────────────────────────
// Live values pushed over the WebSocket whenever they change.
// (IR self-test result is on the NTFY page, network + signal on ABOUT.)
	#define HOME_RAIN_TODAY    0   // mm today (logged + pending)
	#define HOME_RAIN_MONTH    1   // mm this month
	#define HOME_RAIN_LASTTIP  2   // time of the last tip
	#define HOME_RAIN_TIPS     3   // tips since boot
	#define HOME_RAIN_MMTIP    4   // mm per tip (calibration)
	#define HOME_TEMP_EXT      5   // external DS18S20
	#define HOME_TEMP_INT      6   // internal DS18S20
	#define HOME_IRRIGATE      7   // "Irrigate today?" answer given to the irrigation controller
	#define HOME_SOIL          8   // soil water state (water balance)
	#define HOME_ET0           9   // ET0 of the last finished day
	#define HOME_TODAY_T      10   // outside min / max so far today
	#define HOME_TODAY_TIN    11   // inside min / max so far today
	#define HOME_RAIN_BEAM    12   // live bucket position: beam BROKEN (red) / OPEN (green)

	#define PAGE_VAR_HOME     13   // Total number of home page PageVar entries

	inline bool     HomePageReady = false;
	inline PageVar* HomeList      = nullptr;
	inline int      InternetSec   = 0;   // Current seconds value from SNTP (updated by CompileDateTime)

// ─── Home page live values (bound to HomeList[] PageElements) ───────────────
	inline String HomeIrrigate  = "";
	inline String HomeIrriStyle = "color: black; font-size:14px";
	inline String HomeSoil      = "";
	inline String HomeEt0       = "";
	inline String HomeTodayT    = "";
	inline String HomeTodayTIn  = "";
	inline String HomeRainToday = "";
	inline String HomeRainMonth = "";
	inline String HomeLastTip   = "";
	inline int    HomeTips      = 0;
	inline String HomeMmPerTip  = "";
	inline String HomeTempExt   = "";
	inline String HomeTempInt   = "";
	inline String HomeBeam      = "";   // "BROKEN" / "OPEN" — refreshed every 50 ms
	inline String HomeBeamStyle = "color: black; font-size:14px";
	inline String HomeStyle    = "color: black; font-size:14px";

// ─── Background Colour Attribute Strings ─────────────────────────────────────
	#define ATT_RED    "color:white;background-color:red;padding:4px 10px;border-radius:6px"
	#define ATT_WHITE  "color:black;background-color:white"
	#define ATT_ORANGE "color:black;background-color:orange;padding:4px 10px;border-radius:6px"
	#define ATT_YELLOW "color:black;background-color:yellow;padding:4px 10px;border-radius:6px"
	#define ATT_GREEN  "color:white;background-color:green;padding:4px 10px;border-radius:6px"
