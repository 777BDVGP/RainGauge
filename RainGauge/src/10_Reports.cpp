// =============================================================================
// 10_Reports.cpp — Reports Page
// =============================================================================
// Rainfall summaries built from /RainfallLog.txt (newest first), as on the
// old firmware's "Rain History" (per month) and "Rain Detail" (per hour)
// pages, plus a per-day table.  The General Log is on the FILES page.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// Rainfall summaries
// =============================================================================
// One forward pass over the log.  Records are "YYYYMMDDHH" + mm; the file is
// chronological, so consecutive records with the same key prefix are summed
// into one bucket and the last N buckets are kept in a ring.

#define RPT_MONTHS   36    // months shown
#define RPT_DAYS     31    // most recent days with rain
#define RPT_HOURS    24    // most recent hours with rain

struct RptBucket { char key[11]; float mm; };

struct RptRing {
	RptBucket* b;
	int        size;
	int        count = 0;     // buckets ever started
	RptRing(RptBucket* buf, int n) : b(buf), size(n) {}
	// Add mm under key (first keyLen chars); new key -> new bucket.
	void add(const String& rec, int keyLen, float mm){
		RptBucket* cur = count ? &b[(count - 1) % size] : nullptr;
		if (!cur || strncmp(cur->key, rec.c_str(), keyLen) != 0){
			cur = &b[count % size];
			memcpy(cur->key, rec.c_str(), keyLen);
			cur->key[keyLen] = '\0';
			cur->mm = 0;
			count++;
		}
		cur->mm += mm;
	}
	int shown() const { return count < size ? count : size; }
	const RptBucket& newest(int k) const { return b[(count - 1 - k) % size]; }   // k = 0 is newest
};

static const char* MONTH_NAME[] = {"January","February","March","April","May","June",
                                   "July","August","September","October","November","December"};

static void tableStart(const char* title, const char* col1){
	appendHtmlStr("<div style='padding:10px 8px 0'><b style='font-size:13px;color:#0f172a'>");
	appendHtmlStr(title);
	appendHtmlStr("</b></div><table class='compact'><tr class='hd'><th>");
	appendHtmlStr(col1);
	appendHtmlStr("</th><th style='white-space:nowrap'>Rainfall</th></tr>");
}

static void tableRow(const char* label, float mm){
	char buf[160];
	snprintf(buf, sizeof(buf), "<tr><td>%s</td><td>%.1f mm</td></tr>", label, mm);
	appendHtmlStr(buf);
}

static void tableEnd(int rows){
	if (rows == 0) appendHtmlStr("<tr><td colspan='2' style='text-align:center'>(no rain logged)</td></tr>");
	appendHtmlStr("</table>");
}

static void appendRainfallTables(){
	static RptBucket monthBuf[RPT_MONTHS], dayBuf[RPT_DAYS], hourBuf[RPT_HOURS];
	RptRing months(monthBuf, RPT_MONTHS), days(dayBuf, RPT_DAYS), hours(hourBuf, RPT_HOURS);
	float total = 0;

	File lf = SPIFFS.open(RainLogFile(), "r");
	if (lf){
		while (lf.available()){
			String s = lf.readStringUntil('\n');
			if (s.length() < 11) continue;
			float mm = s.substring(10).toFloat();
			months.add(s, 6,  mm);
			days.add  (s, 8,  mm);
			hours.add (s, 10, mm);
			total += mm;
		}
		lf.close();
	}

	char label[48];

	tableStart("Rainfall per Month", "Month");
	for (int k = 0; k < months.shown(); k++){
		const RptBucket& m = months.newest(k);
		int mon = atoi(String(m.key).substring(4, 6).c_str());
		snprintf(label, sizeof(label), "%s %.4s", (mon >= 1 && mon <= 12) ? MONTH_NAME[mon - 1] : "?", m.key);   // "November 2026"
		tableRow(label, m.mm);
	}
	tableEnd(months.shown());

	tableStart("Rainfall per Day", "Date");
	for (int k = 0; k < days.shown(); k++){
		const RptBucket& d = days.newest(k);
		int mon = atoi(String(d.key).substring(4, 6).c_str());
		snprintf(label, sizeof(label), "%d %s %.4s", atoi(String(d.key).substring(6, 8).c_str()),
		         (mon >= 1 && mon <= 12) ? MONTH_NAME[mon - 1] : "?", d.key);                           // "20 November 2026"
		tableRow(label, d.mm);
	}
	tableEnd(days.shown());

	tableStart("Rainfall per Hour", "Date - Hour");
	for (int k = 0; k < hours.shown(); k++){
		const RptBucket& h = hours.newest(k);
		int mon = atoi(String(h.key).substring(4, 6).c_str());
		snprintf(label, sizeof(label), "%d %.3s %.4s - %.2sh00", atoi(String(h.key).substring(6, 8).c_str()),
		         (mon >= 1 && mon <= 12) ? MONTH_NAME[mon - 1] : "?", h.key, h.key + 8);              // "20 Nov 2026 - 14h00"
		tableRow(label, h.mm);
	}
	tableEnd(hours.shown());

	snprintf(label, sizeof(label), "%.1f mm", total);
	appendHtmlStr("<p style='color:#475569;font-size:11px;padding:4px 8px'>Total in ");
	appendHtmlStr(RainLogFile() + 1);   // file name without the leading '/'
	appendHtmlStr(": ");
	appendHtmlStr(label);
	appendHtmlStr(" &mdash; months older than the last 36 are in the file (FILES page).</p>");
}

// -----------------------------------------------------------------------------
// handleReports  (GET /reports)
// -----------------------------------------------------------------------------
void handleReports(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_08);
	appendMainPageSel(PS_REPORTS);
	appendHtmlStr(webScriptTag);

	if (!IrrigationOff()) appendWaterBalanceTable();   // 20_WaterBalance.cpp — hidden for "No Irrigation"
	appendRainfallTables();
	appendTempHistoryTables();   // 21_TempHistory.cpp
	// General Log is not shown here any more — it's on the FILES page (GeneralLog.txt).

	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}
