// =============================================================================
// 20_WaterBalance.cpp — FAO-56 Daily Soil-Water Balance
// =============================================================================
// See 20_WaterBalance.h for the method.  Everything here runs in loop()
// context except WaterBalanceAnswer(), which only reads.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// Gauteng (Highveld) average reference evaporation, mm/day, Jan..Dec —
// used for a day without enough temperature readings.
static const float CLIM_ET0[12] = { 5.0, 4.6, 4.0, 3.2, 2.6, 2.1, 2.4, 3.2, 4.2, 4.8, 4.8, 5.0 };

static const char* wbLogFile()   { return SimActive ? SIM_WB_LOG_FILE   : WB_LOG_FILE; }
static const char* wbStateFile() { return SimActive ? SIM_WB_STATE_FILE : WB_STATE_FILE; }

// =============================================================================
// Soil & crop numbers from Settings
// =============================================================================

float WB_TAW(){
	float perM = 150;                                         // loam
	if (Params->SoilType == SOIL_SAND) perM = 80;
	if (Params->SoilType == SOIL_CLAY) perM = 200;
	return perM * Params->RootDepthMm / 1000.0f;
}

float WB_RAW(){
	return WB_TAW() * constrain(Params->DepletionPct, 10, 90) / 100.0f;
}

// Depletion right now; the very first start assumes half of RAW used.
float WB_Dr(){
	return WB.Dr < 0 ? WB_RAW() / 2 : WB.Dr;
}

// =============================================================================
// Hargreaves ET0
// =============================================================================

// Extraterrestrial radiation (FAO-56 eq. 21-25) as mm/day of evaporation.
static float raMm(float latDeg, int doy){
	const float Gsc = 0.0820f;                                   // MJ m-2 min-1
	float phi = latDeg * PI / 180.0f;
	float dr  = 1 + 0.033f * cosf(2 * PI * doy / 365.0f);
	float dec = 0.409f * sinf(2 * PI * doy / 365.0f - 1.39f);
	float x   = constrain(-tanf(phi) * tanf(dec), -1.0f, 1.0f);
	float ws  = acosf(x);
	float ra  = (24 * 60 / PI) * Gsc * dr *
	            (ws * sinf(phi) * sinf(dec) + cosf(phi) * cosf(dec) * sinf(ws));   // MJ m-2 day-1
	return 0.408f * ra;
}

static int dayOfYear(const String& key8){
	tm t = {};
	t.tm_year = key8.substring(0, 4).toInt() - 1900;
	t.tm_mon  = key8.substring(4, 6).toInt() - 1;
	t.tm_mday = key8.substring(6, 8).toInt();
	t.tm_hour = 12;
	mktime(&t);
	return t.tm_yday + 1;
}

// ET0 for a day: Hargreaves when Tmin/Tmax are usable, else the monthly average.
static float et0For(const String& key8, float tMin, float tMax, bool tempsOk, bool* fromTemps){
	if (tempsOk && !isnan(tMin) && !isnan(tMax) && tMax >= tMin){
		*fromTemps = true;
		float tMean = (tMin + tMax) / 2;
		float et0 = 0.0023f * (tMean + 17.8f) * sqrtf(tMax - tMin) * raMm(Params->Latitude, dayOfYear(key8));
		return et0 < 0 ? 0 : et0;
	}
	*fromTemps = false;
	int m = constrain(key8.substring(4, 6).toInt(), 1, 12);
	return CLIM_ET0[m - 1];
}

// =============================================================================
// Persistence
// =============================================================================

void WaterBalanceSave(){
	StaticJsonDocument<384> doc;
	doc["dayKey"]    = WB.dayKey;
	doc["Dr"]        = WB.Dr;
	if (!isnan(WB.lastEt0)) doc["lastEt0"] = WB.lastEt0;
	if (!isnan(WB.tMin))    doc["tMin"]    = WB.tMin;
	if (!isnan(WB.tMax))    doc["tMax"]    = WB.tMax;
	doc["tSamples"]  = WB.tSamples;
	doc["latched"]   = WB.latched;
	doc["irrigated"] = WB.irrigated;
	File fw = SPIFFS.open(wbStateFile(), "w");
	if (fw){ serializeJsonPretty(doc, fw); fw.close(); }
}

void WaterBalanceLoad(){
	WB = WaterBalanceState();
	File fr = SPIFFS.open(wbStateFile(), "r");
	if (!fr) return;
	StaticJsonDocument<384> doc;
	if (!deserializeJson(doc, fr)){
		WB.dayKey    = doc["dayKey"] | "";
		WB.Dr        = doc["Dr"]     | -1.0f;
		WB.lastEt0   = doc["lastEt0"].isNull() ? NAN : doc["lastEt0"].as<float>();
		WB.tMin      = doc["tMin"].isNull()    ? NAN : doc["tMin"].as<float>();
		WB.tMax      = doc["tMax"].isNull()    ? NAN : doc["tMax"].as<float>();
		WB.tSamples  = doc["tSamples"]  | 0;
		WB.latched   = doc["latched"]   | false;
		WB.irrigated = doc["irrigated"] | false;
	}
	fr.close();
}

// =============================================================================
// Daily bookkeeping
// =============================================================================

// Called from MaintainTemperature() with every valid outside reading.
void WaterBalanceTrackTemp(float c){
	if (isnan(c)) return;
	if (isnan(WB.tMin) || c < WB.tMin) WB.tMin = c;
	if (isnan(WB.tMax) || c > WB.tMax) WB.tMax = c;
	WB.tSamples++;
}

// Rain logged for one day (YYYYMMDD).
static float rainOnDay(const String& key8){
	float mm = 0;
	File lf = SPIFFS.open(RainLogFile(), "r");
	if (lf){
		while (lf.available()){
			String s = lf.readStringUntil('\n');
			if (s.length() >= 11 && s.startsWith(key8)) mm += s.substring(10).toFloat();
		}
		lf.close();
	}
	return mm;
}

static String nextDayKey(const String& key8){
	tm t = {};
	t.tm_year = key8.substring(0, 4).toInt() - 1900;
	t.tm_mon  = key8.substring(4, 6).toInt() - 1;
	t.tm_mday = key8.substring(6, 8).toInt() + 1;
	t.tm_hour = 12;
	mktime(&t);
	char buf[10];
	snprintf(buf, sizeof(buf), "%04d%02d%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	return String(buf);
}

// One day through the balance; appends its record.  `rain` < 0 = read the log.
static void finishDay(const String& key8, float tMin, float tMax, bool tempsOk, bool irrigated, float rain, File* out){
	bool  fromTemps;
	float et0 = et0For(key8, tMin, tMax, tempsOk, &fromTemps);
	float etc = Params->CropKc * et0;
	if (rain < 0) rain = rainOnDay(key8);
	float pe  = rain >= Params->MinEffRainMm ? rain : 0;
	float irr = irrigated ? Params->IrriDepthMm : 0;
	float dr  = constrain(WB_Dr() - pe - irr + etc, 0.0f, WB_TAW());

	char rec[120];
	snprintf(rec, sizeof(rec), "%.4s-%.2s-%.2s,%s,%s,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.1f%s",
	         key8.c_str(), key8.c_str() + 4, key8.c_str() + 6,
	         fromTemps ? String(tMin, 1).c_str() : "", fromTemps ? String(tMax, 1).c_str() : "",
	         et0, etc, rain, pe, irr, dr, WB_TAW(), fromTemps ? "" : ",avgET0");
	if (out) out->println(rec);
	else {
		File lf = SPIFFS.open(wbLogFile(), "a");
		if (lf){ lf.println(rec); lf.close(); }
	}

	WB.Dr      = dr;
	WB.lastEt0 = et0;
}

// =============================================================================
// MaintainWaterBalance  (loop, every 10 ms — real work once a second)
// =============================================================================
void MaintainWaterBalance(){
	static uint32_t lastSec = 0, lastSave = 0;
	if (millis() - lastSec < 1000) return;
	lastSec = millis();
	if (!RainTimeValid()) return;

	time_t now = time(nullptr);
	tm* t = localtime(&now);
	char today[10];
	snprintf(today, sizeof(today), "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
	int nowMin = t->tm_hour * 60 + t->tm_min;

	// ── New day: finish the collected day (and any days the unit was off).
	if (WB.dayKey.length() == 8 && WB.dayKey != today){
		String key = WB.dayKey;
		finishDay(key, WB.tMin, WB.tMax, WB.tSamples >= WB_MIN_TEMP_SAMPLES, WB.irrigated, -1, nullptr);
		int gap = 0;
		for (key = nextDayKey(key); key < String(today) && gap < WB_MAX_GAP_DAYS; key = nextDayKey(key), gap++)
			finishDay(key, NAN, NAN, false, false, -1, nullptr);   // off: average ET0, logged rain, no irrigation
		SaveGenReport("Water balance: day done - soil used " + String(WB_Dr(), 1) + " of " + String(WB_TAW(), 1) +
		              " mm (irrigate at " + String(WB_RAW(), 1) + " mm), ET0 " + String(WB.lastEt0, 2) + " mm");
	}
	if (WB.dayKey != today){
		WB.dayKey    = today;
		WB.tMin      = WB.tMax = NAN;
		WB.tSamples  = 0;
		WB.latched   = false;
		WB.irrigated = false;
		WaterBalanceSave();
	}

	// ── Latch today's answer when the controller irrigates.
	int hh = Params->IrriTime.substring(0, 2).toInt(), mm = Params->IrriTime.substring(3, 5).toInt();
	if (!WB.latched && IrrigationOff()){
		WB.latched = true;          // "No Irrigation": nothing is credited, nothing logged
		WB.irrigated = false;
		WaterBalanceSave();
	}
	if (!WB.latched && nowMin >= hh * 60 + mm){
		IrriAnswer a = IrrigationDecision();
		WB.latched   = true;
		WB.irrigated = a.irrigate;
		WaterBalanceSave();
		SaveGenReport("Irrigation " + Params->IrriTime + ": " + a.reason);
	}

	// ── Keep the running Tmin/Tmax safe across a reboot.
	if (millis() - lastSave >= 15UL * 60UL * 1000UL){
		lastSave = millis();
		WaterBalanceSave();
	}
}

// =============================================================================
// The live answer (water-balance mode) — read by 19_Irrigation.cpp
// =============================================================================
IrriAnswer WaterBalanceAnswer(){
	IrriAnswer a;
	float raw  = WB_RAW();
	float et0  = isnan(WB.lastEt0) ? CLIM_ET0[constrain(GetCurrentDate(0).substring(5, 7).toInt(), 1, 12) - 1] : WB.lastEt0;
	float etc  = Params->CropKc * et0;
	float rain = RainTodayMm();
	float pe   = rain >= Params->MinEffRainMm ? rain : 0;
	float proj = WB_Dr() + etc - pe;
	if (proj < 0) proj = 0;
	a.rainMm   = rain;
	a.irrigate = proj >= raw;

	String used = String(proj, 1) + " of " + String(raw, 1) + " mm used";
	if (a.irrigate) a.reason = "Irrigate - soil dry: " + used + " (garden uses " + String(etc, 1) + " mm/day)";
	else            a.reason = "Skip - soil still has water: " + used +
	                           (pe > 0 ? " (rain today " + String(rain, 1) + " mm)" : "");
	return a;
}

// Soil state for HOME: "62% full - 4.1 of 11.0 mm used"
String WaterBalanceText(){
	float taw = WB_TAW(), dr = WB_Dr();
	int full  = taw > 0 ? (int)lroundf(100.0f * (taw - dr) / taw) : 0;
	return String(full) + "% full - " + String(dr, 1) + " mm used (irrigate at " + String(WB_RAW(), 1) + ")";
}

// =============================================================================
// Simulation: replay the balance over the generated rain history
// =============================================================================
// Every day uses the simulated outside min/max, and "irrigated" follows the
// morning answer the gauge would have given — so the reports show how the
// soil bucket empties and fills.
void WaterBalanceReplay(int days){
	if (!SimActive || !RainTimeValid()) return;

	String keys[SIM_HISTORY_DAYS + 1];
	float  rain[SIM_HISTORY_DAYS + 1];
	days = constrain(days, 1, SIM_HISTORY_DAYS);
	time_t now = time(nullptr);
	for (int i = 0; i < days; i++){
		time_t d = now - (time_t)(days - i) * 86400;
		tm* t = localtime(&d);
		char buf[10];
		snprintf(buf, sizeof(buf), "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
		keys[i] = buf;
		rain[i] = 0;
	}
	File lf = SPIFFS.open(RainLogFile(), "r");
	if (lf){
		while (lf.available()){
			String s = lf.readStringUntil('\n');
			if (s.length() < 11) continue;
			for (int i = 0; i < days; i++)
				if (s.startsWith(keys[i])){ rain[i] += s.substring(10).toFloat(); break; }
		}
		lf.close();
	}

	File out = SPIFFS.open(wbLogFile(), "w");
	WB = WaterBalanceState();
	for (int i = 0; i < days; i++){
		// Morning answer: no rain yet that day.
		float et0 = isnan(WB.lastEt0) ? 3.5f : WB.lastEt0;
		bool  irrigate = WB_Dr() + Params->CropKc * et0 >= WB_RAW();
		finishDay(keys[i], Sim->TempOutMin, Sim->TempOutMax, true, irrigate, rain[i], out ? &out : nullptr);
	}
	if (out) out.close();

	char today[10];
	tm* t = localtime(&now);
	snprintf(today, sizeof(today), "%04d%02d%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
	WB.dayKey = today;
	WaterBalanceSave();
}

// =============================================================================
// WATER BALANCE page — last 14 finished days
// =============================================================================
void appendWaterBalanceTable(){
	const int N = 14;
	String ring[N];
	int count = 0;
	File lf = SPIFFS.open(wbLogFile(), "r");
	if (lf){
		while (lf.available()){
			String s = lf.readStringUntil('\n');
			s.trim();
			if (s.length() > 10) ring[count++ % N] = s;
		}
		lf.close();
	}

	appendHtmlStr("<div style='padding:10px 8px 0'><b style='font-size:13px;color:#0f172a'>Water Balance (last 14 days)</b>"
	              "<span style='font-size:11px;color:#64748b'> &mdash; mm; Soil = water used from the root zone</span></div>"
	              "<table class='compact grid'><tr class='hd'><th>Date</th><th>Min/Max ");
	appendHtmlStr(TempUnitSym());
	appendHtmlStr("</th><th>ET0</th><th>Rain</th><th>Irr</th><th>Soil</th></tr>");
	int shown = count < N ? count : N;
	for (int k = 0; k < shown; k++){
		// date,Tmin,Tmax,ET0,ETc,Rain,Pe,Irr,Dr,TAW[,avgET0]
		String f[11];
		String s = ring[(count - 1 - k) % N];
		int n = 0, from = 0;
		while (n < 11){
			int c = s.indexOf(',', from);
			f[n++] = c < 0 ? s.substring(from) : s.substring(from, c);
			if (c < 0) break;
			from = c + 1;
		}
		String temps = f[1].length() ? TempNumStr(f[1]) + " / " + TempNumStr(f[2]) : String("avg");   // stored in °C
		String row = "<tr><td>" + f[0].substring(5) + "</td><td>" + temps + "</td><td>" + f[3] +
		             "</td><td>" + f[5] + "</td><td>" + f[7] + "</td><td>" + f[8] + " / " + f[9] + "</td></tr>";
		appendHtmlStr(row.c_str());
	}
	if (shown == 0) appendHtmlStr("<tr><td colspan='6'>(no finished days yet)</td></tr>");
	appendHtmlStr("</table>");
}
