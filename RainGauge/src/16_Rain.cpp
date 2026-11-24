// =============================================================================
// 16_Rain.cpp — Tipping-Bucket Counter, Calibration & Rainfall Log
// =============================================================================
// See 16_Rain.h for how tips are detected, calibrated and logged.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

static uint32_t calLastTipMs = 0;
static uint32_t calStartMs   = 0;     // when START was pressed (banner running time)
static String   totalsDayKey = "";     // YYYYMMDD the logged totals belong to
static String   pendingKey   = "";     // YYYYMMDDHH of the first tip in the open window

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------

// Clock is valid once SNTP has set it (anything after 2024).
bool RainTimeValid(){
	return time(nullptr) > 1704067200;   // 2024-01-01
}

// Current time as the log key "YYYYMMDDHH" (len 10), "YYYYMMDD" (8) or "YYYYMM" (6).
static String timeKey(int len){
	time_t now = time(nullptr);
	tm* t = localtime(&now);
	char buf[12];
	snprintf(buf, sizeof(buf), "%04d%02d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour);
	buf[len] = '\0';
	return String(buf);
}

// Funnel catch area in mm².
static float funnelAreaMm2(){
	float r = Params->FunnelDiaMm / 2.0f;
	return PI * r * r;
}

float RainMmPerTip(){
	float area = funnelAreaMm2();
	if (area <= 0) return 0;
	return Params->MlPerTip * 1000.0f / area;    // 1 ml = 1000 mm³
}

float RainTodayMm(){ return RainTodayLogged + RainPendingTips * RainMmPerTip(); }
float RainMonthMm(){ return RainMonthLogged + RainPendingTips * RainMmPerTip(); }

// -----------------------------------------------------------------------------
// ApplyRainSettings — push Settings-page values into the live counter.
// Called at boot (CreateAllParams) and whenever Settings is saved.
// -----------------------------------------------------------------------------
void ApplyRainSettings(){
	int ms = Params->TipDebounceMs;
	if (ms < 20)   ms = 20;
	if (ms > 2000) ms = 2000;
	DebounceSteadyMs = ms;
}

// -----------------------------------------------------------------------------
// RainRecalcTotals — sum today's and this month's logged rain from the file.
// Run when the calendar day changes (and once the clock is first valid).
// -----------------------------------------------------------------------------
static void RainRecalcTotals(const String& dayKey){
	String monthKey = dayKey.substring(0, 6);
	float day = 0, month = 0;

	File lf = SPIFFS.open(RainLogFile(), "r");
	if (lf){
		while (lf.available()){
			String s = lf.readStringUntil('\n');
			if (s.length() < 11) continue;
			if (!s.startsWith(monthKey)) continue;
			float mm = s.substring(10).toFloat();
			month += mm;
			if (s.startsWith(dayKey)) day += mm;
		}
		lf.close();
	}
	RainTodayLogged = day;
	RainMonthLogged = month;
	totalsDayKey    = dayKey;
}

// Make the next once-a-second pass re-total today/month from the (other) log.
void RainForceRecalc(){
	totalsDayKey = "";
}

// -----------------------------------------------------------------------------
// flushWindow — append the pending tips as one log record.
// -----------------------------------------------------------------------------
static void flushWindow(){
	if (RainPendingTips == 0) return;
	float mm = RainPendingTips * RainMmPerTip();

	// Log under the hour of the window's first tip, so a window that spans
	// midnight still lands on the right day.
	String key = pendingKey.length() == 10 ? pendingKey : timeKey(10);
	char rec[24];
	snprintf(rec, sizeof(rec), "%s%8.3f", key.c_str(), mm);
	File lf = SPIFFS.open(RainLogFile(), "a");
	if (!lf){ SaveGenReport F("Rain: could not open RainfallLog.txt"); return; }
	lf.println(rec);
	lf.close();

	if (key.startsWith(totalsDayKey))                RainTodayLogged += mm;
	if (key.startsWith(totalsDayKey.substring(0, 6))) RainMonthLogged += mm;
	RainPendingTips = 0;
	pendingKey      = "";
}

// -----------------------------------------------------------------------------
// onTip — one bucket tip (one debounced change of the IR pickup).
// -----------------------------------------------------------------------------
static void onTip(){
	RainLastTipMs = millis();
	if (CalActive){
		CalTips++;
		calLastTipMs = millis();
		CalStatus = "Counting - pour slowly";
		return;
	}
	if (RainPendingTips == 0 && RainTimeValid()) pendingKey = timeKey(10);
	RainTipsSinceBoot++;
	RainPendingTips++;
	RainLastTip = RainTimeValid() ? GetCurrentDateAndTime() : String("(clock not set)");
}

// -----------------------------------------------------------------------------
// MaintainRain  (called every 10 ms from loop(), after PinDebounce)
// -----------------------------------------------------------------------------
void MaintainRain(){
	static bool known = false;
	static bool prev  = false;

	// Left the CALIBRATION page while calibrating -> stop (tips are kept,
	// so CALCULATE still works after coming back).
	if (CalStopRequested){
		CalStopRequested = false;
		if (CalActive){
			CalCommand(0, "stop");
			CalStatus = "Stopped (page left) - " + String(CalTips) + " tips";
			SendCalibrationValues(true);
		}
	}

	// ── Simulation on/off (18_Simulation.cpp).  Flush the open window to the
	//    log it belongs to BEFORE switching files, then re-total from the
	//    other log and re-learn the pickup's resting state.
	if (Sim && Sim->Enabled != SimActive){
		flushWindow();
		WaterBalanceSave();                  // keep this mode's soil bucket / temperature state...
		TempHistorySave();
		SimActive     = Sim->Enabled;
		WaterBalanceLoad();                  // ...and pick up the other mode's
		TempHistoryLoad();
		SimQueuedTips = 0;
		known         = false;
		RainForceRecalc();
		SaveGenReport(SimActive ? F("SIMULATION mode ON") : F("SIMULATION mode OFF - real sensors"));
	}

	if (SimActive){
		// ── Simulated tips — the real pickup is ignored.
		if (SimFireTip()) onTip();
		MaintainSimulation();
	} else {
		// ── Tip detection: every debounced change of the pickup is a tip.  The
		//    first 2 s after boot (or after leaving simulation) only establish
		//    the resting state (old firmware: Init_IN_TIP) — no phantom tip.
		static uint32_t settleFrom = 0;
		bool level = InPort[IIDX_IR_PICKUP].Input;
		if (!known){
			if (settleFrom == 0) settleFrom = millis();
			if (millis() - settleFrom > 2000UL + (unsigned long)DebounceSteadyMs){
				known = true; prev = level; settleFrom = 0;
			}
		} else if (level != prev){
			prev = level;
			onTip();
		}
	}

	// ── Calibration timeout: 3 min after the last tip (or the start).
	if (CalActive && millis() - calLastTipMs > CAL_TIMEOUT_MS){
		CalActive = false;
		CalStatus = "Timed out - " + String(CalTips) + " tips";
	}

	// ── Once a second: day rollover + log the window that just closed.
	static uint32_t lastSec = 0;
	if (millis() - lastSec < 1000) return;
	lastSec = millis();
	if (!RainTimeValid()) return;            // keep tips pending until the clock is set

	String dayKey = timeKey(8);
	if (dayKey != totalsDayKey){
		flushWindow();                       // close out the old day's window first
		RainRecalcTotals(dayKey);            // ...then total the new day from the file
	}

	static time_t lastWindow = 0;
	time_t window = time(nullptr) / RAIN_CAPTURE_SEC;
	if (window != lastWindow){
		lastWindow = window;
		flushWindow();
	}
}

// =============================================================================
// Calibration — commands arrive over the WebSocket ({"CAL":"start"} ...)
// =============================================================================

void CalCommand(uint32_t clientId, const String& cmd){
	if (cmd == "start"){
		CalActive    = true;
		CalTips      = 0;
		CalResult    = 0;
		calLastTipMs = millis();
		calStartMs   = calLastTipMs;
		CalStatus    = "Started - pour " + String(Params->CalWaterMl) + " ml slowly";
	}
	else if (cmd == "stop"){
		CalActive = false;
		CalStatus = "Stopped - " + String(CalTips) + " tips";
	}
	else if (cmd == "calc"){
		if (CalTips < CAL_MIN_TIPS){
			CalStatus = "Need at least " + String(CAL_MIN_TIPS) + " tips (have " + String(CalTips) + ")";
			return;
		}
		CalActive = false;
		CalResult = (float)Params->CalWaterMl / CalTips;
		CalStatus = "Calculated - press SAVE to keep it";
	}
	else if (cmd == "save"){
		if (CalResult <= 0){ CalStatus = "Nothing to save - CALCULATE first"; return; }
		Params->MlPerTip = CalResult;
		Params->CalDate  = RainTimeValid() ? GetCurrentDateAndTime() : String("unknown");
		Params->WriteData();
		SaveGenReport("Rain: calibrated " + String(CalTips) + " tips for " + String(Params->CalWaterMl) +
		              " ml -> " + String(CalResult, 3) + " ml/tip, " + String(RainMmPerTip(), 4) + " mm/tip");
		CalStatus = "Saved";
		CalResult = 0;
		WebMessUpdate(clientId, "Calibration saved!", "color: red", true);
	}
	SendCalibrationValues(true);
}

static String calResultText(){
	if (CalResult <= 0) return "-";
	float area = funnelAreaMm2();
	float mm   = area > 0 ? CalResult * 1000.0f / area : 0;
	return String(CalResult, 2) + " ml per tip (bucket size)<br>= " + String(mm, 3) + " mm of rain per tip";
}

static String calCurrentText(){
	return String(Params->MlPerTip, 2) + " ml per tip (bucket size)<br>= " + String(RainMmPerTip(), 3) +
	       " mm of rain per tip<br><span style='color:#64748b'>calibrated: " + Params->CalDate + "</span>";
}

// Push the calibration fields that changed (force = all).  Every 500 ms from
// MaintainWebService, and right after each command.
void SendCalibrationValues(bool force){
	static uint32_t lastTips = UINT32_MAX;
	static String   lastStat, lastRes, lastCur;
	String res = calResultText();
	String cur = calCurrentText();
	if (force || CalTips != lastTips) { webParamUpdate(String(CalTips), "CALTIPS", ""); lastTips = CalTips; }
	if (force || CalStatus != lastStat){ webParamUpdate(CalStatus,      "CALSTAT", ""); lastStat = CalStatus; }
	if (force || res != lastRes)       { webParamUpdate(res,            "CALRES",  ""); lastRes  = res; }
	if (force || cur != lastCur)       { webParamUpdate(cur,            "CALCUR",  ""); lastCur  = cur; }
	// Banner: is a calibration running?  + estimated water poured so far
	// (tips x the CURRENT ml/tip — only a guide until the new value is saved).
	// Colours / pulse are CSS classes on the page (webParamUpdate max 256 bytes).
	static String lastBan, lastSub, lastCls, lastBar, lastBarTx;
	String ban, sub, cls, bar, barTx;
	if (CalActive){
		uint32_t run  = (millis() - calStartMs) / 1000;
		uint32_t idle = (millis() - calLastTipMs) / 1000;
		uint32_t left = idle >= CAL_TIMEOUT_MS / 1000 ? 0 : CAL_TIMEOUT_MS / 1000 - idle;
		char t[64];
		snprintf(t, sizeof(t), "&#9679; CALIBRATING &mdash; %lu:%02lu", (unsigned long)(run / 60), (unsigned long)(run % 60));
		ban = t;
		snprintf(t, sizeof(t), "auto-stop in %lu:%02lu without a tip", (unsigned long)(left / 60), (unsigned long)(left % 60));
		sub = t;
		cls = "cb cbrun";
	} else if (CalResult > 0){
		ban = "CALCULATED &mdash; press SAVE";  sub = "&nbsp;";  cls = "cb cbres";
	} else {
		ban = "NOT CALIBRATING";  sub = "press START to begin";  cls = "cb cbidle";
	}
	int pct = 0;
	if (CalActive || CalTips > 0){
		float ml = CalTips * Params->MlPerTip;
		pct = Params->CalWaterMl > 0 ? (int)(ml * 100.0f / Params->CalWaterMl) : 0;
		if (pct > 100) pct = 100;
		barTx = "&asymp; " + String((long)lroundf(ml)) + " of " + String(Params->CalWaterMl) + " ml through the bucket";
	} else {
		barTx = "&nbsp;";
	}
	bar = "width:" + String(pct) + "%";
	if (force || ban != lastBan || cls != lastCls){ webParamUpdate(ban, "CALBAN", cls, "class"); lastBan = ban; lastCls = cls; }
	if (force || sub != lastSub)    { webParamUpdate(sub,   "CALBANSUB", "");  lastSub   = sub; }
	if (force || bar != lastBar)    { webParamUpdate("",    "CALBAR",    bar); lastBar   = bar; }
	if (force || barTx != lastBarTx){ webParamUpdate(barTx, "CALBARTX",  "");  lastBarTx = barTx; }

	// Bucket position, same reading as HOME (HIGH = dark = beam broken)
	static int lastBeam = -1;
	int beam = InPort[IIDX_IR_PICKUP].Input ? 1 : 0;
	if (force || beam != lastBeam){
		webParamUpdate(beam ? "BROKEN" : "OPEN", "CALBEAM", beam ? ATT_RED : ATT_GREEN);
		lastBeam = beam;
	}
}

// -----------------------------------------------------------------------------
// handleCalibrate  (GET /calibrate — opened from the Settings page)
// -----------------------------------------------------------------------------
void handleCalibrate(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendMainPageSel(PS_SETTINGS);
	appendHtmlStr(webScriptTag);

	String water = String(Params->CalWaterMl);
	appendHtmlStr("<div style='max-width:460px;margin:8px auto;font-size:13px'>"
	              "<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:6px 0 8px'>CALIBRATION</div>"
	              "<ol style='margin:0 0 10px 22px;line-height:1.6;color:#334155'>"
	              "<li>Stand the gauge perfectly level; water must drain freely at the bottom.</li>"
	              "<li>Press <b>START</b> &mdash; no rainfall is logged while calibrating.</li>"
	              "<li>Pour exactly <b>");
	appendHtmlStr(water.c_str());
	appendHtmlStr(" ml</b> of water slowly into the funnel (take a few minutes).</li>"
	              "<li>When it has all drained, press <b>CALCULATE</b> (needs at least ");
	appendHtmlStr(String(CAL_MIN_TIPS).c_str());
	appendHtmlStr(" tips).</li>"
	              "<li>Press <b>SAVE</b> to keep the new value.</li></ol>"
	              "<p style='color:#64748b;font-size:11px;margin:0 0 8px 8px'>Counting stops by itself 3 minutes after the last tip. "
	              "The water amount is set on the Settings page.</p>");

	appendHtmlStr("<style>@keyframes calpulse{0%,100%{opacity:1}50%{opacity:.55}}"
	              ".cb{display:block;text-align:center;font-weight:800;font-size:17px;letter-spacing:1px;padding:10px 10px 2px;"
	              "border-radius:8px 8px 0 0;color:#fff}"
	              ".cbidle{background:#94a3b8}.cbres{background:#0284c7}"
	              ".cbrun{background:#ea580c;animation:calpulse 1.2s ease-in-out infinite}"
	              "#CALBANSUB{display:block;text-align:center;font-size:11px;color:#fff;background:#475569;"
	              "padding:3px;border-radius:0 0 8px 8px;margin-bottom:6px}"
	              "#CALBAR{display:block;height:14px;border-radius:7px;background:#ea580c;width:0;transition:width .4s}</style>"
	              "<span id='CALBAN' class='cb cbidle'>-</span><span id='CALBANSUB'>&nbsp;</span>"
	              "<div style='background:#e2e8f0;border-radius:7px;height:14px;margin:4px 0 2px'><span id='CALBAR'></span></div>"
	              "<div style='text-align:center;font-size:11px;color:#64748b;margin-bottom:6px'><span id='CALBARTX'>&nbsp;</span></div>");
	appendHtmlStr("<table>"
	              "<tr><td>Bucket Position (IR beam)</td><td><span id='CALBEAM'>-</span></td></tr>"
	              "<tr><td>Tips counted</td><td><span id='CALTIPS' style='font-size:20px;font-weight:800;color:#0284c7'>0</span></td></tr>"
	              "<tr><td>Status</td><td><span id='CALSTAT'>-</span></td></tr>"
	              "<tr><td>Result</td><td><span id='CALRES'>-</span></td></tr>"
	              "<tr><td>Current</td><td><span id='CALCUR' style='font-size:12px'>-</span></td></tr>"
	              "</table>");

	appendHtmlStr("<div class='btns'>"
	              "<button class='btn btn-gr' onclick=\"CalCmd('start')\">START</button>"
	              "<button class='btn btn-bl' onclick=\"CalCmd('calc')\">CALCULATE</button>"
	              "<button class='btn btn-or' onclick=\"CalCmd('save')\">SAVE</button>"
	              "<button class='btn btn-rd' onclick=\"CalCmd('stop')\">STOP</button>"
	              "</div>"
	              "<div class='btns'><a class='btn btn-bl' href='/settings'>&#8592; BACK TO SETTINGS</a></div>"
	              "</div>");

	// Fill the live fields as soon as the WebSocket is up.
	// Leaving the page (BACK, menu, closing the tab) stops a running calibration.
	// pagehide does not fire when a phone screen just turns off, so a pour in
	// progress is not interrupted by that.  sendBeacon still gets out while the
	// page unloads (a WebSocket message would not).
	appendHtmlStr("<script>setTimeout(function(){CalCmd('show');},800);"
	              "window.addEventListener('pagehide',function(){navigator.sendBeacon('/calstop');});</script>");

	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}
