// =============================================================================
// 18_Simulation.cpp — Simulation Mode
// =============================================================================
// See 18_Simulation.h.  The web side (page, WebSocket commands) runs in the
// AsyncTCP task and only sets flags/counters; all tip generation and file
// work happens in loop() via MaintainSimulation() (called from MaintainRain).
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// Persistence
// =============================================================================

void SimParams::WriteData(){
	StaticJsonDocument<384> doc;
	doc["Enabled"]    = Enabled;
	doc["RainRate"]   = RainRate;
	doc["MlPerTip"]   = MlPerTip;
	doc["TempOutMin"] = TempOutMin;
	doc["TempOutMax"] = TempOutMax;
	doc["TempIn"]     = TempIn;
	File fw = SPIFFS.open(SIM_FILE, "w");
	if (fw){ serializeJsonPretty(doc, fw); fw.close(); }
}

void SimParams::ReadData(){
	File fr = SPIFFS.open(SIM_FILE, "r");
	if (!fr) return;
	StaticJsonDocument<384> doc;
	if (!deserializeJson(doc, fr)){
		if (!doc["Enabled"].isNull())    Enabled    = doc["Enabled"].as<bool>();
		if (!doc["RainRate"].isNull())   RainRate   = doc["RainRate"].as<float>();
		if (!doc["MlPerTip"].isNull())   MlPerTip   = doc["MlPerTip"].as<float>();
		if (!doc["TempOutMin"].isNull()) TempOutMin = doc["TempOutMin"].as<float>();
		if (!doc["TempOutMax"].isNull()) TempOutMax = doc["TempOutMax"].as<float>();
		if (!doc["TempIn"].isNull())     TempIn     = doc["TempIn"].as<float>();
	}
	fr.close();
}

void CreateSimulation(){
	Sim = new SimParams;
	Sim->ReadData();

	SimList = new PageVar[PAGE_VAR_SIM];
	for (int i = 0; i < PAGE_VAR_SIM; i++){
		switch (i){
			case SIMROW_ENABLED:
				SimList[i].Var = new PageElement(&Sim->Enabled,    F("Simulation Mode"),              "", &SetStyle, i+ID_SIM);
				break;
			case SIMROW_RAINRATE:
				SimList[i].Var = new PageElement(&Sim->RainRate,   F("Simulated Rain (mm/h, 0 = dry)"), "", &SetStyle, i+ID_SIM);
				SimList[i].Var->SetRange(0, 200, 0.1);
				break;
			case SIMROW_MLPERTIP:
				SimList[i].Var = new PageElement(&Sim->MlPerTip,   F("Simulated Bucket (ml per tip)"), "", &SetStyle, i+ID_SIM);
				SimList[i].Var->SetRange(0.5, 50, 0.01);
				break;
			case SIMROW_TOUTMIN:
				SimList[i].Var = new PageElement(&Sim->TempOutMin, F("Outside Min (°C, at 03:00)"),   "", &SetStyle, i+ID_SIM);
				SimList[i].Var->SetRange(-20, 50, 0.1);
				break;
			case SIMROW_TOUTMAX:
				SimList[i].Var = new PageElement(&Sim->TempOutMax, F("Outside Max (°C, at 15:00)"),   "", &SetStyle, i+ID_SIM);
				SimList[i].Var->SetRange(-20, 50, 0.1);
				break;
			case SIMROW_TIN:
				SimList[i].Var = new PageElement(&Sim->TempIn,     F("Inside (°C)"),                  "", &SetStyle, i+ID_SIM);
				SimList[i].Var->SetRange(-20, 60, 0.1);
				break;
		}
	}
}

// Which rainfall log is live — the simulation never touches the real one.
const char* RainLogFile(){
	return SimActive ? SIM_RAIN_LOG_FILE : RAIN_LOG_FILE;
}

// =============================================================================
// Simulated temperatures — outside: cosine day curve, min 03:00 / max 15:00
// =============================================================================

float SimOutsideTemp(){
	time_t now = time(nullptr);
	tm* t = localtime(&now);
	float h   = t->tm_hour + t->tm_min / 60.0f;
	float mid = (Sim->TempOutMax + Sim->TempOutMin) / 2.0f;
	float amp = (Sim->TempOutMax - Sim->TempOutMin) / 2.0f;
	return mid + amp * cosf(2.0f * PI * (h - 15.0f) / 24.0f);
}

// =============================================================================
// GENERATE HISTORY — 90 days of made-up rain into the simulation log.
// Afternoon thunderstorms (Highveld style): ~30 % of days, 1–35 mm, spread
// over 1–4 hours from 13:00–19:00, rounded to whole tips.  Replaces the log.
// =============================================================================

static uint32_t lcg(uint32_t& s){ s = s * 1664525UL + 1013904223UL; return s >> 8; }
static float    rnd(uint32_t& s){ return (lcg(s) & 0xFFFF) / 65535.0f; }

static void generateHistory(){
	if (!RainTimeValid()){ SimStatus = "Clock not set yet - try again shortly"; return; }

	float mmTip = RainMmPerTip();
	if (mmTip <= 0){ SimStatus = "mm per tip is 0 - check Settings"; return; }

	File fw = SPIFFS.open(SIM_RAIN_LOG_FILE, "w");
	if (!fw){ SimStatus = "Could not write the simulation log"; return; }

	time_t now = time(nullptr);
	int records = 0, rainDays = 0;
	float total = 0;
	for (int d = SIM_HISTORY_DAYS; d >= 1; d--){
		time_t day = now - (time_t)d * 86400;
		tm* t = localtime(&day);
		uint32_t seed = (t->tm_year + 1900) * 1000U + t->tm_yday;   // same day -> same rain
		lcg(seed);
		if (rnd(seed) > 0.30f) continue;

		float dayMm  = 1.0f + 34.0f * rnd(seed) * rnd(seed);       // mostly light, some heavy
		int   hours  = 1 + (int)(rnd(seed) * 4);
		int   start  = 13 + (int)(rnd(seed) * 6);
		rainDays++;
		for (int h = 0; h < hours && start + h < 24; h++){
			int tips = (int)lroundf(dayMm / hours / mmTip);
			if (tips <= 0) continue;
			char rec[24];
			snprintf(rec, sizeof(rec), "%04d%02d%02d%02d%8.3f",
			         t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, start + h, tips * mmTip);
			fw.println(rec);
			records++;
			total += tips * mmTip;
		}
	}
	fw.close();
	RainForceRecalc();
	TempHistoryGenerate(SIM_HISTORY_DAYS);  // 90 days of min/max around the sim values (21_TempHistory.cpp)
	WaterBalanceReplay(SIM_HISTORY_DAYS);   // soil bucket over the same 90 days (20_WaterBalance.cpp)
	SimStatus = "Generated " + String(SIM_HISTORY_DAYS) + " days: " + String(rainDays) + " rain days, " +
	            String(total, 1) + " mm (" + String(records) + " records)";
	SaveGenReport("Simulation: " + SimStatus);
}

// =============================================================================
// MaintainSimulation  (loop context — called from MaintainRain every 10 ms)
// Returns via SimFireTip() the tips to count; mode switching is done by
// MaintainRain so the open rain window is flushed to the right file first.
// =============================================================================

// Returns true when one simulated tip should be counted now.
bool SimFireTip(){
	if (!SimActive) return false;

	// ── Queued tips (buttons / pour), one every SIM_TIP_GAP_MS
	static uint32_t lastQueued = 0;
	if (SimQueuedTips > 0 && millis() - lastQueued >= SIM_TIP_GAP_MS){
		lastQueued = millis();
		SimQueuedTips--;
		return true;
	}

	// ── Continuous rain: water into the funnel = rate x area; one tip per
	//    simulated bucket-full.  The logged mm then use the CALIBRATED ml/tip,
	//    so a bad calibration shows up as a wrong total — like the real thing.
	static float    waterMl = 0;
	static uint32_t lastMs  = millis();
	uint32_t now = millis();
	float dtH = (now - lastMs) / 3600000.0f;
	lastMs = now;
	if (Sim->RainRate > 0 && Sim->MlPerTip > 0){
		float r = Params->FunnelDiaMm / 2.0f;
		waterMl += Sim->RainRate * (PI * r * r / 1000.0f) * dtH;
		if (waterMl >= Sim->MlPerTip){
			waterMl -= Sim->MlPerTip;
			return true;
		}
	} else {
		waterMl = 0;
	}
	return false;
}

// File jobs requested from the web page — run in loop context.
void MaintainSimulation(){
	if (SimGenHistory){ SimGenHistory = false; if (SimActive) generateHistory(); }
	if (SimClearLog){
		SimClearLog = false;
		SPIFFS.remove(SIM_RAIN_LOG_FILE);
		SPIFFS.remove(SIM_WB_LOG_FILE);
		SPIFFS.remove(SIM_WB_STATE_FILE);
		SPIFFS.remove(SIM_TEMP_HIST_FILE);
		SPIFFS.remove(SIM_TEMP_STATE_FILE);
		if (SimActive){ WB = WaterBalanceState(); TH = TempHistoryState(); }
		RainForceRecalc();
		SimStatus = "Simulation log cleared";
	}
}

// =============================================================================
// Web — page, WebSocket commands, live values
// =============================================================================

void SimCommand(uint32_t clientId, const String& cmd){
	if (!SimActive && cmd != "show"){
		SimStatus = "Switch Simulation Mode on (and UPDATE) first";
	}
	else if (cmd == "tip")    { SimQueuedTips += 1;  SimStatus = "1 tip"; }
	else if (cmd == "tip10")  { SimQueuedTips += 10; SimStatus = "10 tips queued"; }
	else if (cmd == "pour")   {
		uint32_t n = (Sim->MlPerTip > 0) ? (uint32_t)lroundf(Params->CalWaterMl / Sim->MlPerTip) : 0;
		SimQueuedTips += n;
		SimStatus = "Pouring " + String(Params->CalWaterMl) + " ml = " + String(n) + " tips (open CALIBRATION and press START first)";
	}
	else if (cmd == "history"){ SimGenHistory = true; SimStatus = "Generating..."; }
	else if (cmd == "clear")  { SimClearLog   = true; SimStatus = "Clearing..."; }
	SendSimulationValues(true);
}

void UpdateSimPageValues(uint32_t clientId, JsonObject *root){
	for (JsonPair kv : *root){
		for (int i = 0; i < PAGE_VAR_SIM; i++){
			if (SimList[i].Var->GetID("") == kv.key().c_str())
				SimList[i].Var->SetName(String(kv.value().as<const char*>()));
		}
	}
	bool wasOn = SimActive;
	Sim->WriteData();
	WebMessUpdate(clientId, Sim->Enabled ? "Simulation settings saved - ON" : "Simulation settings saved - OFF", "color: red", true);
	// Switched on/off: reload so the red banner and the enabled/greyed buttons
	// match at once (MaintainRain applies Sim->Enabled within 10 ms; the
	// browser waits 1.5 s before reloading).
	if (Sim->Enabled != wasOn) ws.text(clientId, "{\"type\":\"reload\"}");
}

// Push changed values (force = all) — every 500 ms from MaintainWebService.
void SendSimulationValues(bool force){
	if (!Sim) return;
	for (int i = 0; i < PAGE_VAR_SIM; i++){
		if (force || SimList[i].Var->HasChanged()){
			webParamUpdate(SimList[i].Var->GetName(), SimList[i].Var->GetID(""), SimList[i].Var->GetVarStyle());
			SimList[i].Var->UpdateLast();
		}
	}
	static String   lastStat;
	static uint32_t lastQ = UINT32_MAX;
	static String   lastLive;
	String live = SimActive ? ("ON - outside " + TempNum(SimOutsideTemp()) + " " + TempUnitSym() + ", today " +
	                           String(RainTodayMm(), 2) + " mm, tips since boot " + String(RainTipsSinceBoot))
	                        : String("OFF - real sensors in use");
	if (force || SimStatus != lastStat){ webParamUpdate(SimStatus,              "SIMSTAT", ""); lastStat = SimStatus; }
	if (force || SimQueuedTips != lastQ){ webParamUpdate(String(SimQueuedTips), "SIMQ",    ""); lastQ    = SimQueuedTips; }
	if (force || live != lastLive)      { webParamUpdate(live,                  "SIMLIVE", ""); lastLive = live; }
}

static void WT_SimPage(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("Simulation Setting", "Value", "");
	for (int i = 0; i < PAGE_VAR_SIM; i++){
		if (i == SIMROW_TIN && !Params->ShowInsideTemp) continue;   // inside hidden in SETTINGS
		if (i == SIMROW_ENABLED) WT_RightCheckRow(&SimList[i]);
		else                     WT_EditRow(&SimList[i]);
	}
	WT_EndPage();
}

// -----------------------------------------------------------------------------
// handleSimulate  (GET /simulate — opened from the Settings page)
// -----------------------------------------------------------------------------
void handleSimulate(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	WT_SimPage();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendMainPageSel(PS_SETTINGS);
	appendHtmlStr(webScriptTag);

	appendHtmlStr("<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:8px 0 4px'>SIMULATION</div>"
	              "<p style='color:#475569;font-size:12px;padding:0 8px 4px'>Try the gauge without the PCB. "
	              "Tick <b>Simulation Mode</b> and press <b>UPDATE</b>: the IR pickup and temperature sensors are "
	              "ignored, and rain goes to <b>SimRainfallLog.txt</b> &mdash; your real rainfall log is never touched.</p>");

	appendHtmlBuf(PageBuf, getPageBufLen());
	appendHtmlStr(webUpateButton);

	appendHtmlStr("<table style='margin-top:10px'>"
	              "<tr><td>Live</td><td><span id='SIMLIVE' style='font-size:12px'>-</span></td></tr>"
	              "<tr><td>Tips waiting</td><td><span id='SIMQ'>0</span></td></tr>"
	              "<tr><td>Last action</td><td><span id='SIMSTAT' style='font-size:12px'>-</span></td></tr>"
	              "</table>");

	// Simulation off: the action buttons do nothing, so grey them out and say why.
	const char* dis = SimActive ? "" : " disabled style='opacity:.35;cursor:not-allowed'";
	if (!SimActive)
		appendHtmlStr("<p style='text-align:center;color:#dc2626;font-weight:700;font-size:13px;margin:10px 8px 0'>"
		              "Simulation is OFF &mdash; tick <u>Simulation Mode</u> and press <u>UPDATE</u> to use these buttons.</p>");
	String b;
	b.reserve(900);
	b += "<div class='btns'>";
	b += "<button class='btn btn-bl'"; b += dis; b += " onclick=\"SimCmd('tip')\">1 TIP</button>";
	b += "<button class='btn btn-bl'"; b += dis; b += " onclick=\"SimCmd('tip10')\">10 TIPS</button>";
	b += "<button class='btn btn-gr'"; b += dis; b += " onclick=\"SimCmd('pour')\">POUR CALIBRATION WATER</button>";
	b += "</div><div class='btns'>";
	b += "<button class='btn btn-or'"; b += dis; b += " onclick=\"SimCmd('history')\">GENERATE 90 DAYS HISTORY</button>";
	b += "<button class='btn btn-rd'"; b += dis; b += " onclick=\"if(confirm('Delete SimRainfallLog.txt?'))SimCmd('clear')\">CLEAR SIM LOG</button>";
	b += "</div>";
	appendHtmlStr(b.c_str());
	appendHtmlStr(
	              "<div class='btns'><a class='btn btn-bl' href='/calibrate'>CALIBRATION</a>"
	              "<a class='btn btn-bl' href='/settings'>&#8592; BACK TO SETTINGS</a></div>");

	appendHtmlStr("<script>setTimeout(function(){SimCmd('show');},800);</script>");
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}
