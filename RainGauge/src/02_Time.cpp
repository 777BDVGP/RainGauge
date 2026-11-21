// =============================================================================
// 02_Time.cpp — Timer Engine, SNTP Setup & Date/Time Utilities
// =============================================================================
// Implements the non-blocking software timer API, SNTP initialisation,
// log-file trimming, the heap guard, and a set of date/time helper
// functions used throughout the firmware.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// MaintainHeapGuard  (called every 10 ms from loop())
// Reboot if free heap drops below MIN_FREE_HEAP (10 KB), or if the heap is so
// fragmented that no block large enough to serve a page exists
// (maxBlk < HEAP_MAXBLK_MIN) continuously for TIM_HEAP_FRAGMENTED.  Both are
// internally rate-limited by TIM_CHECK_HEAP_AN_STACK.
// -----------------------------------------------------------------------------
void MaintainHeapGuard() {
	if (OTAInProgress) return;              // never reboot mid-flash on a transient dip
	if (!TimerExpired(TIM_CHECK_HEAP_AN_STACK)) return;
	activateTimer(TIM_CHECK_HEAP_AN_STACK);

	uint32_t freeHeap = ESP.getFreeHeap();
	if (freeHeap < MIN_FREE_HEAP) {
		SaveGenReport("REBOOT - Low heap: " + String(freeHeap) + " B");
		delay(100);
		ESP.restart();
	}

	// Fragmentation never self-heals without a reboot, so if it persists for
	// TIM_HEAP_FRAGMENTED, force one instead of serving 503s until the next
	// scheduled reboot slot.
	uint32_t maxAlloc = ESP.getMaxAllocHeap();
	if (maxAlloc < HEAP_MAXBLK_MIN) {
		if (!Timer[TIM_HEAP_FRAGMENTED].Status) {
			activateTimer(TIM_HEAP_FRAGMENTED);
		} else if (TimerExpired(TIM_HEAP_FRAGMENTED)) {
			SaveGenReport("REBOOT - Heap fragmented: maxBlk=" + String(maxAlloc) + " B");
			delay(100);
			ESP.restart();
		}
	} else {
		deactivateTimer(TIM_HEAP_FRAGMENTED);
	}
}

// -----------------------------------------------------------------------------
// SetupTime
// Starts the SNTP client with the South Africa UTC+2 timezone, then builds the
// initial DateTime string.  Uses the Arduino core configTzTime() — the old
// ESPPerfectTime wrapper drove the raw esp SNTP API and asserts under
// arduino-esp32 3.x / IDF 5 lwIP core-locking.  Only second resolution is used
// (CompileDateTime -> time()/localtime()), so nothing is lost.
// -----------------------------------------------------------------------------
void SetupTime(){
	configTzTime("SAST-2", ntpServer);   // UTC+2 — South Africa Standard Time

	CompileDateTime();
	DEBUG_PRINTLN(DateTime);
}

// -----------------------------------------------------------------------------
// MaintainScheduledReboot  (called every 10 ms from loop())
// One configurable reboot slot (Params->RebootTime1 / RebootEnable1).
// Guarded by REBOOT_MIN_UPTIME_MS so a device that happens to
// boot up already at/near a configured time won't immediately reboot again —
// by the time it re-reaches the required uptime, the clock has moved past
// that minute.
// -----------------------------------------------------------------------------
#define REBOOT_MIN_UPTIME_MS (5UL * 60UL * 1000UL)   // 5 minutes

void MaintainScheduledReboot() {
	if (OTAInProgress) return;              // don't reboot in the middle of a flash
	if (millis() < REBOOT_MIN_UPTIME_MS) return;

	if (Params->RebootEnable1 && GetCurrentTime() == Params->RebootTime1) {
		SaveGenReport("REBOOT - Scheduled reboot (" + Params->RebootTime1 + ")");
		delay(100);
		ESP.restart();
	}
}

// -----------------------------------------------------------------------------
// MaintainTimers  (called every 10 ms from loop())
// Scans all active timers and clears the Status flag of any that have expired.
// Timer expiry is detected by comparing millis() against ExpireTime.
// -----------------------------------------------------------------------------
void MaintainTimers(){
	unsigned long Misec = millis();
	for (int i = 0; i < TIM_TOTAL; i++){
		if (Timer[i].Status){
			if (Misec > Timer[i].ExpireTime){
				Timer[i].Status = false;   // Timer has elapsed — mark as expired
			}
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateWebPageTime  (called every 10 ms from loop())
// Refreshes the displayed date/time string once per second and pushes it to
// all connected WebSocket clients via the H02 element.
// -----------------------------------------------------------------------------
void UpdateWebPageTime(){
	if(TimerExpired(TIM_UPDATE_SYSTEM_TIME)){
		activateTimer(TIM_UPDATE_SYSTEM_TIME);
		CompileDateTime();
		webParamUpdate(DateTime, "H02", "");
	}
}

// -----------------------------------------------------------------------------
// CompileDateTime
// Reads the current SNTP time, formats it into the global DateTime string
// ("DD Mon YYYY HH:MM:SS Day"), and updates WeekDay and InternetSec.
// Also triggers daily log-file trimming when the day-of-month changes.
// -----------------------------------------------------------------------------
void CompileDateTime(){
	time_t now = time(nullptr);
	tm* timeInfo = localtime(&now);

	char buf[25];
	sprintf(buf, "%02d %s %04d %02d:%02d:%02d %s",
	        timeInfo->tm_mday, YR_MONTH[timeInfo->tm_mon],
	        timeInfo->tm_year + 1900,
	        timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec,
	        WEEK_DAY[timeInfo->tm_wday]);
	DateTime = String(buf);

	InternetSec = timeInfo->tm_sec;
	WeekDay     = timeInfo->tm_wday + 1;   // Convert 0-based (Sun=0) to 1-based (Sun=1 … Sat=7)

	// Trim log files once per day when the calendar day rolls over
	if (MonthDay != timeInfo->tm_mday){
		MonthDay = timeInfo->tm_mday;
		reduce_file("GeneralLog.txt", 200);
		reduce_file("TempHistory.txt",  TEMP_HIST_DAYS);   // ~3 years of daily lines
		reduce_file("WaterBalance.txt", TEMP_HIST_DAYS);
	}
}

// -----------------------------------------------------------------------------
// reduce_file
// Trims a SPIFFS text file to at most 'Entries' lines by writing all lines
// beyond the excess to a temporary file, then replacing the original.
// Called daily from CompileDateTime() to prevent log files growing unbounded.
// Parameters:
//   filename — SPIFFS filename without leading slash (e.g. "GeneralLog.txt")
//   Entries  — Maximum number of lines to retain (oldest are discarded)
// -----------------------------------------------------------------------------
void reduce_file(String filename, int Entries)
{
	int Records = 0;

	f = SPIFFS.open("/" + filename, "r");
	if (!f) return;   // File does not exist — nothing to trim

	// Count total lines in the file
	while (f.available()){
		Records++;
		line = f.readStringUntil('\n');
	}
	f.close();

	// Only rewrite if the file exceeds the maximum line count
	if (Records > Entries){
		int RecDel = Records - Entries;   // Number of oldest lines to discard
		Records = 0;

		f = SPIFFS.open("/" + filename, "r");
		if (!f) return;

		ftemp = SPIFFS.open("/temp.txt", "w");
		if (!ftemp){ f.close(); return; }

		// Copy only the lines we want to keep into the temporary file
		while (f.available()){
			line = f.readStringUntil('\n');
			Records++;
			if (Records > RecDel){
				ftemp.println(line);
			}
		}
		f.close();
		ftemp.close();

		// Atomically replace the original file with the trimmed copy
		SPIFFS.remove("/" + filename);
		SPIFFS.rename("/temp.txt", "/" + filename);
	}
}

// =============================================================================
// Software Timer API
// =============================================================================

// -----------------------------------------------------------------------------
// activateTimer
// Starts (or restarts) the timer at the given index.  The expiry time is
// calculated as: now + (Units × TimerUnits) milliseconds.
// -----------------------------------------------------------------------------
void activateTimer(int index){
	Timer[index].ExpireTime = millis() + (long(Timer[index].Units) * long(Timer[index].TimerUnits));
	Timer[index].Status = true;
}

// -----------------------------------------------------------------------------
// deactivateTimer
// Cancels the timer at the given index without waiting for it to expire.
// -----------------------------------------------------------------------------
void deactivateTimer(int index){
	Timer[index].Status = false;
}

// -----------------------------------------------------------------------------
// TimerExpired
// Returns true when the timer is NOT running (i.e. has expired or was never
// started).  This is the primary polling function for non-blocking delays.
// -----------------------------------------------------------------------------
bool TimerExpired(int index){
	return !Timer[index].Status;
}

// -----------------------------------------------------------------------------
// SetValueAndAtivate
// Updates the timer's Units and TimerUnits from string parameters, then
// immediately activates it.  Used to apply user-editable timeout values.
// -----------------------------------------------------------------------------
void SetValueAndAtivate(String Units, int TimerUnits, int index){
	Timer[index].Units     = Units.toInt();
	Timer[index].TimerUnits = TimerUnits;
	activateTimer(index);
}

// -----------------------------------------------------------------------------
// SetTimerValue
// Updates the Units field of a timer without activating it.  Call
// activateTimer() separately when the timer should start.
// -----------------------------------------------------------------------------
void SetTimerValue(int Units, int index){
	Timer[index].Units = Units;
}

// -----------------------------------------------------------------------------
// RemainingSec
// Returns the number of whole seconds remaining on an active timer, or 0 if
// the timer has expired or is not running.
// -----------------------------------------------------------------------------
int RemainingSec(int index){
	unsigned long Misec = millis();
	if (Timer[index].Status){
		if (Misec > Timer[index].ExpireTime) return 0;
		return (Timer[index].ExpireTime - Misec) / SEC;
	}
	return 0;
}

// -----------------------------------------------------------------------------
// RemainingMin
// Returns the number of whole minutes remaining on an active timer, or 0 if
// the timer has expired or is not running.
// -----------------------------------------------------------------------------
int RemainingMin(int index){
	unsigned long Misec = millis();
	if (Timer[index].Status){
		if (Misec > Timer[index].ExpireTime) return 0;
		return (Timer[index].ExpireTime - Misec) / MIN;
	}
	return 0;
}

// =============================================================================
// Date / Time Helper Functions
// =============================================================================

// -----------------------------------------------------------------------------
// GetCurrentDateAndTime
// Returns the current date and time as "YYYY-MM-DD HH:MM" (no seconds).
// -----------------------------------------------------------------------------
String GetCurrentDateAndTime(){
	time_t now = time(nullptr);
	tm* t = localtime(&now);
	char buf[20];
	sprintf(buf, "%04d-%02d-%02d %02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min);
	return String(buf);
}

// -----------------------------------------------------------------------------
// GetCurrentDate
// Returns the date offset by daysOffset days from today as "YYYY-MM-DD".
// Pass 0 for today, 1 for tomorrow, -1 for yesterday, etc.
// -----------------------------------------------------------------------------
String GetCurrentDate(int daysOffset){
	time_t now = time(nullptr) + (long(daysOffset) * 86400L);
	tm* t = localtime(&now);
	char buf[12];
	sprintf(buf, "%04d-%02d-%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
	return String(buf);
}

// -----------------------------------------------------------------------------
// GetCurrentTime
// Returns the current time as "HH:MM".
// -----------------------------------------------------------------------------
String GetCurrentTime(){
	time_t now = time(nullptr);
	tm* t = localtime(&now);
	char buf[8];
	sprintf(buf, "%02d:%02d", t->tm_hour, t->tm_min);
	return String(buf);
}

// -----------------------------------------------------------------------------
// IncrementDateWithOneDay
// Advances the given "YYYY-MM-DD" date string by exactly one calendar day,
// correctly handling month-end and year-end rollovers via mktime().
// -----------------------------------------------------------------------------
String IncrementDateWithOneDay(String date){
	tm t = {0};
	t.tm_year = date.substring(0, 4).toInt() - 1900;
	t.tm_mon  = date.substring(5, 7).toInt() - 1;
	t.tm_mday = date.substring(8, 10).toInt() + 1;  // mktime normalises overflow
	mktime(&t);
	char buf[12];
	sprintf(buf, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
	return String(buf);
}

// -----------------------------------------------------------------------------
// AddTimeToCurrentDateTime
// Adds a duration to a base date/time string.
// Parameters:
//   baseDateTime — "YYYY-MM-DD HH:MM"
//   period       — "HH:MM" duration to add
// Returns the resulting date/time as "YYYY-MM-DD HH:MM".
// mktime() normalises any hour/minute overflow into the correct date.
// -----------------------------------------------------------------------------
String AddTimeToCurrentDateTime(String baseDateTime, String period){
	tm t = {0};
	t.tm_year = baseDateTime.substring(0,  4).toInt() - 1900;
	t.tm_mon  = baseDateTime.substring(5,  7).toInt() - 1;
	t.tm_mday = baseDateTime.substring(8, 10).toInt();
	
	// Parse period (HH:MM) - find the colon to handle variable-length formats
	int colonPos = period.indexOf(':');
	int hours = 0, minutes = 0;
	if (colonPos >= 0) {
		hours = period.substring(0, colonPos).toInt();
		minutes = period.substring(colonPos + 1).toInt();
	} else {
		// Fallback: assume fixed positions if no colon found
		hours = period.substring(0, 2).toInt();
		minutes = period.substring(3, 5).toInt();
	}
	
	t.tm_hour = baseDateTime.substring(11, 13).toInt() + hours;
	t.tm_min  = baseDateTime.substring(14, 16).toInt() + minutes;
	mktime(&t);   // Normalise — handles overflow (e.g. 63 minutes → +1 hour, 3 minutes)
	char buf[20];
	sprintf(buf, "%04d-%02d-%02d %02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
	return String(buf);
}


