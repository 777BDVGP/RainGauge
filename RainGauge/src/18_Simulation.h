#pragma once

// =============================================================================
// 18_Simulation.h — Simulation Mode (play with the gauge without hardware)
// =============================================================================
// SETTINGS -> SIMULATION (/simulate).  When "Simulation Mode" is on:
//   * the real IR pickup is ignored — tips come from the simulator:
//       - TIP / 10 TIPS buttons
//       - continuous rain at "Simulated Rain (mm/h)"
//       - POUR CALIBRATION WATER: Settings "Calibration Water (ml)" divided by
//         the simulated bucket size, one tip every SIM_TIP_GAP_MS — so the
//         Calibration page can be tried end-to-end
//   * the temperature sensors are ignored — outside follows a day curve
//     between the simulated min (03:00) and max (15:00); inside is fixed
//   * rain is logged to /SimRainfallLog.txt, NOT the real RainfallLog.txt;
//     HOME and REPORTS show the simulation log.  GENERATE HISTORY fills it
//     with 90 days of made-up rain to try the reports.
//   * every page shows a red SIMULATION banner.
// The settings persist in /Simulation.json (survive a reboot).
// =============================================================================

	#define SIM_FILE           "/Simulation.json"
	#define SIM_RAIN_LOG_FILE  "/SimRainfallLog.txt"
	#define SIM_TIP_GAP_MS     250        // spacing of queued (button / pour) tips
	#define SIM_HISTORY_DAYS   90

	#define ID_SIM             150        // WebSocket IDs of the Simulation page (150+)

class SimParams
{
public:
	void WriteData();
	void ReadData();

	bool  Enabled     = false;   // Simulation Mode on/off (requested — applied in loop)
	float RainRate    = 0;       // continuous simulated rain, mm/h (0 = dry)
	float MlPerTip    = 5.6;     // the simulated bucket's real size (ml per tip)
	float TempOutMin  = 12;      // outside minimum (°C) — at 03:00
	float TempOutMax  = 27;      // outside maximum (°C) — at 15:00
	float TempIn      = 22;      // inside (°C)
};

	inline SimParams* Sim          = nullptr;
	inline bool       SimActive    = false;   // simulation actually in force (loop context)
	inline uint32_t   SimQueuedTips = 0;      // tips waiting to be fired (buttons / pour)
	inline bool       SimGenHistory = false;  // GENERATE HISTORY requested
	inline bool       SimClearLog   = false;  // CLEAR SIM LOG requested
	inline String     SimStatus     = "-";

// ─── Simulation Page Row Index Constants ─────────────────────────────────────
	#define SIMROW_ENABLED     0
	#define SIMROW_RAINRATE    1
	#define SIMROW_MLPERTIP    2
	#define SIMROW_TOUTMIN     3
	#define SIMROW_TOUTMAX     4
	#define SIMROW_TIN         5
	#define PAGE_VAR_SIM       6

	inline PageVar* SimList = nullptr;
