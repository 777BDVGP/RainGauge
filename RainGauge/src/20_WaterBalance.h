#pragma once

// =============================================================================
// 20_WaterBalance.h — FAO-56 Daily Soil-Water Balance ("does the garden need water?")
// =============================================================================
// The root zone is treated as a bucket that holds TAW mm of plant-available
// water.  Dr = depletion: how many mm have been used since the soil was last
// full (0 = full, TAW = bone dry for the plants).  Each finished day:
//
//   ET0  = 0.0023 x (Tmean + 17.8) x sqrt(Tmax - Tmin) x Ra     (Hargreaves)
//          Ra = sun energy at the top of the atmosphere for the latitude and
//          day of year (FAO-56 eq. 21), in mm of evaporation per day.
//          Tmin/Tmax come from the EXTERNAL DS18S20.  A day without enough
//          readings uses the Gauteng monthly average ET0 instead.
//   ETc  = Kc x ET0                     garden use (Kc: beds ~0.6, lawn ~0.8)
//   Pe   = rain, if >= MinEffRainMm     (smaller showers only wet the leaves)
//   Irr  = IrriDepthMm if the controller was told to irrigate that day
//   Dr   = clamp(Dr - Pe - Irr + ETc, 0, TAW)   (excess rain drains away)
//
//   TAW  = soil water per metre (Sand 80 / Loam 150 / Clay 200 mm/m) x root depth
//   RAW  = DepletionPct % x TAW         readily available water
//
// The answer to the controller (19_Irrigation.cpp) is live all day:
//   projected = Dr + Kc x ET0(yesterday) - Pe(rain so far today)
//   irrigate  = projected >= RAW
// At "<Controller> Irrigates At" (default 10:00) today's answer is latched: if it
// was "irrigate", IrriDepthMm is credited to today's balance.
//
// One record per finished day goes to /WaterBalance.txt (simulation:
// /SimWaterBalance.txt):  date,Tmin,Tmax,ET0,ETc,Rain,Pe,Irr,Dr,TAW
// The running state is kept in /WaterBalance.json (/SimWaterBalance.json).
// =============================================================================

	#define WB_LOG_FILE         "/WaterBalance.txt"
	#define WB_STATE_FILE       "/WaterBalance.json"
	#define SIM_WB_LOG_FILE     "/SimWaterBalance.txt"
	#define SIM_WB_STATE_FILE   "/SimWaterBalance.json"

	#define WB_MIN_TEMP_SAMPLES 4320    // 10 s readings: >= half a day before the day's own Tmin/Tmax are trusted
	#define WB_MAX_GAP_DAYS     31      // days caught up after the unit was off

	#define IRRI_MODE_WB      "Water Balance"
	#define IRRI_MODE_SIMPLE  "Simple Rule"
	#define IRRI_MODE_NONE    "No Irrigation"   // rain meter only — no irrigation answer, sections hidden

	// true when the gauge is set to "No Irrigation" (plain rain meter)
	inline bool IrrigationOff(){ return Params && Params->IrriMode == IRRI_MODE_NONE; }

	#define SOIL_SAND   "Sand"
	#define SOIL_LOAM   "Loam"
	#define SOIL_CLAY   "Clay"

struct WaterBalanceState {
	String dayKey    = "";     // YYYYMMDD of the day being collected
	float  Dr        = -1;     // root-zone depletion, mm (-1 = not started: half of RAW)
	float  lastEt0   = NAN;    // ET0 of the last finished day (mm) — forecast for today
	float  tMin      = NAN;    // today's outside min / max so far (°C)
	float  tMax      = NAN;
	int    tSamples  = 0;      // readings behind tMin/tMax
	bool   latched   = false;  // today's answer frozen at IrriTime?
	bool   irrigated = false;  // ...and it was "irrigate"
};

	inline WaterBalanceState WB;
