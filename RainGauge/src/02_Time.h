#pragma once

// =============================================================================
// 02_Time.h — Software Timer Engine & Date/Time Globals
// =============================================================================
// Defines the TimerStruct type, all TIM_* index constants, and the Timer[]
// array that drives all non-blocking timeouts across the firmware.
// Also declares global date/time strings updated each second by CompileDateTime().
//
// Timer API (implemented in 02_Time.cpp):
//   activateTimer(idx)    — Start or restart a timer
//   deactivateTimer(idx)  — Cancel a running timer
//   TimerExpired(idx)     — Returns true when the timer is not running (expired)
//   RemainingSec(idx)     — Seconds remaining on a running timer
//   RemainingMin(idx)     — Minutes remaining on a running timer
// =============================================================================

// ─── NTP Server ───────────────────────────────────────────────────────────────
inline const char *ntpServer = "za.pool.ntp.org";   // South Africa NTP pool (UTC+2)

// =============================================================================
// Software Timer Engine
// =============================================================================

// ─── TimerStruct ─────────────────────────────────────────────────────────────
// Each entry in Timer[] holds the configuration and live state of one timer.
struct TimerStruct
{
	bool           Status;       // true = timer is running; false = expired or not started
	unsigned int   Units;        // Duration in the specified TimerUnits
	unsigned int   TimerUnits;   // Time base: MS (1), SEC (1000), or MIN (60000)
	unsigned long  ExpireTime;   // millis() value at which the timer expires
};

// ─── Time-Base Multipliers ────────────────────────────────────────────────────
#define MS    1        // Milliseconds multiplier
#define SEC   1000     // Seconds multiplier  (Units × 1000 ms)
#define MIN   60000    // Minutes multiplier  (Units × 60000 ms)

// ─── Timer Index Definitions ──────────────────────────────────────────────────
// Each TIM_* constant is an index into Timer[].  Keep in ascending order and
// update TIM_TOTAL when adding new entries.
#define TIM_LED_FLASHING_SPEED   0   // Heartbeat LED toggle interval             (600 ms)
#define TIM_UPDATE_SYSTEM_TIME   1   // Date/time string refresh & WebSocket push (1 s)
#define TIM_STABILIZE_DELAY      2   // Post-boot stabilisation wait              (5 s)
#define TIM_RESET_OPTION_MESS    3   // WebSocket status message display time     (2 s)
#define TIM_CHECK_HEAP_AN_STACK  4   // Heap monitor — reboot if < MIN_FREE_HEAP  (15 s)
#define TIM_WIFI_RECONNECT       5   // Reboot if WiFi absent for this long       (60 s)
#define TIM_HEAP_FRAGMENTED      6   // Reboot if heap stays fragmented this long (2 min)

#define TIM_TOTAL   7   // Total number of timer slots — must match the array size below

// ─── Timer Array Initialisation ──────────────────────────────────────────────
// All timers start inactive (Status = false).  Call activateTimer() to start one.
// Columns: Status   Units   TimerUnits   ExpireTime
inline TimerStruct Timer[TIM_TOTAL] =
{
	{ false,  600,   MS,   0 },   // TIM_LED_FLASHING_SPEED   — 600 ms LED heartbeat
	{ false,    1,  SEC,   0 },   // TIM_UPDATE_SYSTEM_TIME   — 1 s clock refresh
	{ false,    5,  SEC,   0 },   // TIM_STABILIZE_DELAY      — 5 s stabilisation
	{ false,    2,  SEC,   0 },   // TIM_RESET_OPTION_MESS    — 2 s message display
	{ false,   15,  SEC,   0 },   // TIM_CHECK_HEAP_AN_STACK  — 15 s heap monitor
	{ false,   60,  SEC,   0 },   // TIM_WIFI_RECONNECT       — 60 s WiFi-loss reboot
	{ false,    2,  MIN,   0 },   // TIM_HEAP_FRAGMENTED      — 2 min sustained fragmentation reboot
};

// =============================================================================
// Date / Time Globals
// =============================================================================

// Month and weekday name lookup tables used by CompileDateTime()
inline const char *YR_MONTH[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
inline const char *WEEK_DAY[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

inline String NoNetwork = F("OFFLINE  NO  TIME   ");  // Shown before NTP sync succeeds
inline String DateTime  = NoNetwork;                   // Current date/time display string (updated every second)

inline int MonthDay = -1;  // Last seen day-of-month; triggers log trim when it changes
inline int WeekDay  = 1;   // Current weekday: 1 = Sunday … 7 = Saturday

