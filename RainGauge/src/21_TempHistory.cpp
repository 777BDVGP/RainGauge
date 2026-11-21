// =============================================================================
// 21_TempHistory.cpp — Daily Min/Max Temperature History & Records
// =============================================================================
// See 21_TempHistory.h.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

static bool thDirty = false;

const char* TempHistFile()          { return SimActive ? SIM_TEMP_HIST_FILE  : TEMP_HIST_FILE; }
static const char* tempStateFile()  { return SimActive ? SIM_TEMP_STATE_FILE : TEMP_STATE_FILE; }

static String fmt1(float c){ return isnan(c) ? String("") : String(c, 1); }

// =============================================================================
// Persistence
// =============================================================================

static void putRec(JsonObject o, const char* k, const TempRecord& r){
	if (isnan(r.value)) return;
	JsonObject x = o.createNestedObject(k);
	x["v"] = r.value;
	x["t"] = r.when;
}

static void getRec(JsonObject o, const char* k, TempRecord& r){
	if (o[k].isNull()) return;
	r.value = o[k]["v"].as<float>();
	r.when  = o[k]["t"].as<String>();
}

void TempHistorySave(){
	DynamicJsonDocument doc(768);
	doc["dayKey"] = TH.dayKey;
	doc["since"]  = TH.since;
	if (!isnan(TH.out.lo)){ doc["outLo"] = TH.out.lo; doc["outHi"] = TH.out.hi; }
	if (!isnan(TH.in.lo)) { doc["inLo"]  = TH.in.lo;  doc["inHi"]  = TH.in.hi;  }
	JsonObject rec = doc.createNestedObject("records");
	putRec(rec, "outLo", TH.outLo);
	putRec(rec, "outHi", TH.outHi);
	putRec(rec, "inLo",  TH.inLo);
	putRec(rec, "inHi",  TH.inHi);
	File fw = SPIFFS.open(tempStateFile(), "w");
	if (fw){ serializeJsonPretty(doc, fw); fw.close(); }
	thDirty = false;
}

void TempHistoryLoad(){
	TH = TempHistoryState();
	File fr = SPIFFS.open(tempStateFile(), "r");
	if (!fr) return;
	DynamicJsonDocument doc(768);
	if (!deserializeJson(doc, fr)){
		TH.dayKey = doc["dayKey"] | "";
		TH.since  = doc["since"]  | "";
		if (!doc["outLo"].isNull()){ TH.out.lo = doc["outLo"]; TH.out.hi = doc["outHi"]; }
		if (!doc["inLo"].isNull()) { TH.in.lo  = doc["inLo"];  TH.in.hi  = doc["inHi"];  }
		JsonObject rec = doc["records"];
		if (!rec.isNull()){
			getRec(rec, "outLo", TH.outLo);
			getRec(rec, "outHi", TH.outHi);
			getRec(rec, "inLo",  TH.inLo);
			getRec(rec, "inHi",  TH.inHi);
		}
	}
	fr.close();
}

// =============================================================================
// Tracking  (from MaintainTemperature, every 10 s, clock valid)
// =============================================================================

static void recordLow(TempRecord& r, float c, const String& now){
	if (isnan(c)) return;
	if (isnan(r.value) || c < r.value){ r.value = c; r.when = now; thDirty = true; }
}

static void recordHigh(TempRecord& r, float c, const String& now){
	if (isnan(c)) return;
	if (isnan(r.value) || c > r.value){ r.value = c; r.when = now; thDirty = true; }
}

void TempHistoryTrack(float outside, float inside){
	String today = GetCurrentDate(0);

	// ── New day: write yesterday's line (only if it had any reading).
	if (TH.dayKey != today){
		if (TH.dayKey.length() == 10 && (!isnan(TH.out.lo) || !isnan(TH.in.lo))){
			String line = TH.dayKey + "," + fmt1(TH.out.lo) + "," + fmt1(TH.out.hi) + "," +
			              fmt1(TH.in.lo) + "," + fmt1(TH.in.hi);
			File lf = SPIFFS.open(TempHistFile(), "a");
			if (lf){ lf.println(line); lf.close(); }
		}
		TH.dayKey = today;
		TH.out = MinMax();
		TH.in  = MinMax();
		if (TH.since.length() == 0) TH.since = today;
		TempHistorySave();
	}

	TH.out.add(outside);
	TH.in.add(inside);

	String now = GetCurrentDateAndTime();
	recordLow (TH.outLo, outside, now);
	recordHigh(TH.outHi, outside, now);
	recordLow (TH.inLo,  inside,  now);
	recordHigh(TH.inHi,  inside,  now);

	// Save a new record within a minute (on the first warm days nearly every
	// reading is one — don't rewrite flash every 10 s), else every 15 min so
	// a reboot loses little of today's min/max.
	static uint32_t lastSave = 0;
	uint32_t since = millis() - lastSave;
	if ((thDirty && since >= 60000UL) || since >= 15UL * 60UL * 1000UL){
		lastSave = millis();
		TempHistorySave();
	}
}

// GET /resettemprecords — from the REPORTS page.  Records restart from the
// current readings; the daily history file is kept.
// Runs in async_tcp — only raises a flag; MaintainTempRecordsReset() (loop)
// does the reset.  The redirected REPORTS page may still show the old
// records if it loads within the same 10 ms.
static volatile bool thResetRequested = false;

void handleResetTempRecords(AsyncWebServerRequest *request){
	thResetRequested = true;
	request->redirect("/reports");
}

// Called every 10 ms from loop().
void MaintainTempRecordsReset(){
	if (!thResetRequested) return;
	thResetRequested = false;
	TH.outLo = TempRecord(); TH.outHi = TempRecord();
	TH.inLo  = TempRecord(); TH.inHi  = TempRecord();
	TH.since = RainTimeValid() ? GetCurrentDate(0) : String("");
	thDirty  = true;   // saved by the next TempHistoryTrack()
	SaveGenReport F("Temperature records reset");
}

// Today's min/max for HOME: "12.3 / 26.0 °C"
String TempTodayText(const MinMax& m){
	if (isnan(m.lo)) return "--";
	return String(m.lo, 1) + " / " + String(m.hi, 1) + " °C";
}

// =============================================================================
// Simulation — GENERATE HISTORY: 90 days around the simulated min/max
// =============================================================================
void TempHistoryGenerate(int days){
	if (!SimActive || !RainTimeValid()) return;
	File fw = SPIFFS.open(SIM_TEMP_HIST_FILE, "w");
	if (!fw) return;
	time_t now = time(nullptr);
	uint32_t seed = 12345;
	auto rnd = [&seed](){ seed = seed * 1664525UL + 1013904223UL; return ((seed >> 8) & 0xFFFF) / 65535.0f - 0.5f; };
	TH = TempHistoryState();
	for (int d = days; d >= 1; d--){
		time_t day = now - (time_t)d * 86400;
		tm* t = localtime(&day);
		char date[12];
		snprintf(date, sizeof(date), "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
		float oLo = Sim->TempOutMin + 6 * rnd(), oHi = Sim->TempOutMax + 6 * rnd();
		float iLo = Sim->TempIn - 1.5f + rnd(),  iHi = Sim->TempIn + 1.5f + rnd();
		fw.printf("%s,%.1f,%.1f,%.1f,%.1f\n", date, oLo, oHi, iLo, iHi);
		String at = String(date) + " 15:00";
		recordLow (TH.outLo, oLo, String(date) + " 03:00");
		recordHigh(TH.outHi, oHi, at);
		recordLow (TH.inLo,  iLo, String(date) + " 03:00");
		recordHigh(TH.inHi,  iHi, at);
		if (TH.since.length() == 0) TH.since = date;
	}
	fw.close();
	TH.dayKey = GetCurrentDate(0);
	TempHistorySave();
}

// =============================================================================
// REPORTS — records, last 31 days, monthly summary (outside)
// =============================================================================

static void cell(String& row, const String& v){ row += "<td>" + (v.length() ? v : String("-")) + "</td>"; }

void appendTempHistoryTables(){
	const int N = 31, M = 12;
	String days[N];
	int count = 0;

	struct Month { char key[8]; float lo, hi, sumLo, sumHi; int n; };
	static Month months[M];
	int mCount = 0;

	File lf = SPIFFS.open(TempHistFile(), "r");
	if (lf){
		while (lf.available()){
			String s = lf.readStringUntil('\n');
			s.trim();
			if (s.length() < 11) continue;
			days[count++ % N] = s;

			// date,OutMin,OutMax,InMin,InMax — monthly outside summary
			int c1 = s.indexOf(','), c2 = s.indexOf(',', c1 + 1), c3 = s.indexOf(',', c2 + 1);
			String lo = s.substring(c1 + 1, c2), hi = s.substring(c2 + 1, c3);
			if (lo.length() == 0 || hi.length() == 0) continue;
			Month* m = mCount ? &months[(mCount - 1) % M] : nullptr;
			if (!m || strncmp(m->key, s.c_str(), 7) != 0){
				m = &months[mCount++ % M];
				memcpy(m->key, s.c_str(), 7); m->key[7] = '\0';
				m->lo = 999; m->hi = -999; m->sumLo = m->sumHi = 0; m->n = 0;
			}
			float l = lo.toFloat(), h = hi.toFloat();
			if (l < m->lo) m->lo = l;
			if (h > m->hi) m->hi = h;
			m->sumLo += l; m->sumHi += h; m->n++;
		}
		lf.close();
	}

	// ── Records
	appendHtmlStr("<div style='padding:10px 8px 0'><b style='font-size:13px;color:#0f172a'>Temperature Records</b>"
	              "<span style='font-size:11px;color:#64748b'> &mdash; since ");
	appendHtmlStr(TH.since.length() ? TH.since.c_str() : "-");
	appendHtmlStr("</span></div><table class='compact grid'><tr class='hd'><th></th><th>Lowest</th><th>Highest</th></tr>");
	const TempRecord* recs[2][2] = { { &TH.outLo, &TH.outHi }, { &TH.inLo, &TH.inHi } };
	const char* names[2] = { "Outside", "Inside" };
	const int   rows     = Params->ShowInsideTemp ? 2 : 1;   // inside only with SETTINGS "Show Inside Temperature"
	for (int r = 0; r < rows; r++){
		String row = "<tr><td>" + String(names[r]) + "</td>";
		for (int k = 0; k < 2; k++){
			const TempRecord* x = recs[r][k];
			cell(row, isnan(x->value) ? String("") : String(x->value, 1) + " °C<br><span style='font-size:10px;color:#64748b'>" + x->when + "</span>");
		}
		row += "</tr>";
		appendHtmlStr(row.c_str());
	}
	appendHtmlStr("</table><div class='btns'><a class='btn btn-rd' href='/resettemprecords' "
	              "onclick=\"return confirm('Reset the TEMPERATURE records (lowest / highest above)?\\n\\nOnly these values start again from now.\\nRainfall and the daily temperature history are NOT touched.')\">RESET TEMPERATURE RECORDS</a></div>"
	              "<p style='text-align:center;color:#64748b;font-size:11px;margin:0 8px 6px'>"
	              "Only clears the lowest / highest temperatures above. Rainfall is never touched.</p>");

	// ── Last 31 days — one table for outside, one for inside.
	//    Line format: date,OutMin,OutMax,InMin,InMax  (fields 1-2 / 3-4)
	int shown = count < N ? count : N;
	const char* dayTitle[2] = { "Outside Temp per Day (°C)", "Inside Temp per Day (°C)" };
	for (int t = 0; t < rows; t++){
		appendHtmlStr("<div style='padding:6px 8px 0'><b style='font-size:13px;color:#0f172a'>");
		appendHtmlStr(dayTitle[t]);
		appendHtmlStr("</b></div><table class='compact grid'><tr class='hd'><th>Date</th><th>Min</th><th>Max</th></tr>");
		for (int k = 0; k < shown; k++){
			String s = days[(count - 1 - k) % N];
			String f[5];
			int from = 0;
			for (int i = 0; i < 5; i++){
				int c = s.indexOf(',', from);
				f[i] = c < 0 ? s.substring(from) : s.substring(from, c);
				if (c < 0) break;
				from = c + 1;
			}
			String row = "<tr><td>" + f[0].substring(5) + "</td>";
			cell(row, f[1 + 2 * t]);
			cell(row, f[2 + 2 * t]);
			row += "</tr>";
			appendHtmlStr(row.c_str());
		}
		if (shown == 0) appendHtmlStr("<tr><td colspan='3'>(no finished days yet)</td></tr>");
		appendHtmlStr("</table>");
	}

	// ── Monthly (outside)
	appendHtmlStr("<div style='padding:6px 8px 0'><b style='font-size:13px;color:#0f172a'>Outside per Month (°C)</b></div>"
	              "<table class='compact grid'><tr class='hd'><th>Month</th><th>Lowest</th><th>Avg Min</th><th>Avg Max</th><th>Highest</th></tr>");
	int mShown = mCount < M ? mCount : M;
	for (int k = 0; k < mShown; k++){
		const Month& m = months[(mCount - 1 - k) % M];
		char row[200];
		snprintf(row, sizeof(row), "<tr><td>%s</td><td>%.1f</td><td>%.1f</td><td>%.1f</td><td>%.1f</td></tr>",
		         m.key, m.lo, m.sumLo / m.n, m.sumHi / m.n, m.hi);
		appendHtmlStr(row);
	}
	if (mShown == 0) appendHtmlStr("<tr><td colspan='5'>(no data yet)</td></tr>");
	appendHtmlStr("</table>");
}
