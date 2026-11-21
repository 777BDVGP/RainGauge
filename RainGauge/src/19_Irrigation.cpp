// =============================================================================
// 19_Irrigation.cpp — "Irrigate today?" Answer for an irrigation controller
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// RainInLastHours — mm logged in the last `hours` whole hours (by the log's
// YYYYMMDDHH key) plus the tips still pending in the open 30 s window.
// -----------------------------------------------------------------------------
float RainInLastHours(int hours){
	if (!RainTimeValid()) return RainPendingTips * RainMmPerTip();

	time_t from = time(nullptr) - (time_t)hours * 3600;
	tm* t = localtime(&from);
	char fromKey[12];
	snprintf(fromKey, sizeof(fromKey), "%04d%02d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour);

	float mm = 0;
	File lf = SPIFFS.open(RainLogFile(), "r");
	if (lf){
		while (lf.available()){
			String s = lf.readStringUntil('\n');
			if (s.length() < 11) continue;
			if (strncmp(s.c_str(), fromKey, 10) >= 0) mm += s.substring(10).toFloat();   // key >= from
		}
		lf.close();
	}
	return mm + RainPendingTips * RainMmPerTip();
}

// -----------------------------------------------------------------------------
// IrrigationDecision — the current answer (see 19_Irrigation.h for the rule).
// -----------------------------------------------------------------------------
IrriAnswer IrrigationDecision(){
	if (IrrigationOff()){
		// Plain rain meter: no answer.  A controller that still asks is told to
		// irrigate, so it waters as if there were no rain gauge.
		IrriAnswer a;
		a.irrigate = true;
		a.rainMm   = RainInLastHours(Params->IrriSkipHours);
		a.reason   = "Irrigation answer is switched off on the rain gauge (No Irrigation)";
		return a;
	}
	if (Params->IrriMode == IRRI_MODE_WB) return WaterBalanceAnswer();   // 20_WaterBalance.cpp

	IrriAnswer a;
	a.rainMm = RainInLastHours(Params->IrriSkipHours);
	if (a.rainMm >= Params->IrriSkipMm){
		a.irrigate = false;
		a.reason   = "Skip - " + String(a.rainMm, 1) + " mm rain in last " + String(Params->IrriSkipHours) +
		             " h (limit " + String(Params->IrriSkipMm, 1) + " mm)";
	} else {
		a.irrigate = true;
		a.reason   = "Irrigate - only " + String(a.rainMm, 1) + " mm rain in last " + String(Params->IrriSkipHours) +
		             " h (limit " + String(Params->IrriSkipMm, 1) + " mm)";
	}
	return a;
}

// -----------------------------------------------------------------------------
// handleApiIrrigate  (GET /api/irrigate) — polled (e.g. hourly) by the irrigation controller.
// -----------------------------------------------------------------------------
void handleApiIrrigate(AsyncWebServerRequest *request){
	IrriAnswer a = IrrigationDecision();

	StaticJsonDocument<640> doc;
	doc["irrigate"] = a.irrigate;
	doc["reason"]   = a.reason;
	doc["rain_mm"]  = serialized(String(a.rainMm, 2));
	doc["hours"]    = Params->IrriSkipHours;
	doc["limit_mm"] = Params->IrriSkipMm;
	doc["mode"]     = Params->IrriMode;
	if (Params->IrriMode == IRRI_MODE_WB){
		doc["soil_used_mm"]   = serialized(String(WB_Dr(), 1));
		doc["irrigate_at_mm"] = serialized(String(WB_RAW(), 1));
		doc["soil_holds_mm"]  = serialized(String(WB_TAW(), 1));
		if (!isnan(WB.lastEt0)) doc["et0_mm"] = serialized(String(WB.lastEt0, 2));
	}
	doc["sim"]      = SimActive;
	doc["time"]     = RainTimeValid() ? GetCurrentDateAndTime() : String("clock not set");

	String json;
	serializeJson(doc, json);
	request->send(200, "application/json", json);
}
