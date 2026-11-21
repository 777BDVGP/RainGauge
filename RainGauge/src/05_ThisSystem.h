#pragma once

// =============================================================================
// 05_ThisSystem.h — Hardware I/O Map, Debounce Engine & Navigation Constants
// =============================================================================
// RainGauge — WEMOS LOLIN S2 Mini (ESP32-S2)
//
// Pin map for PCB "RAINMETER_V1", taken from the Eagle schematic + board
// (../PCB/RAINMETER_V1.sch / .brd — module pad number = GPIO, per the S2 Mini
// silk).  The board runs from 5 VDC (5VDC_INPUT) with an AMS1117 3.3 V
// regulator; the IR LED is powered straight from the 5 V rail.
//
// | Peripheral                 | S2 Pin  | Direction | Notes                         |
// | -------------------------- | ------- | --------- | ----------------------------- |
// | IR LED drive (T1 2N2222)   | GPIO 2  | OUTPUT    | via R2 10k to base, HIGH = on |
// |                            |         |           | LED: 5V -> R7 220R -> LED -> T1 |
// | DS18S20 "INTERNAL" DQ      | GPIO 1  | 1-Wire    | R1 4k7 pull-up — hand-wired   |
// | DS18S20 "EXTERNAL" DQ      | GPIO 6  | 1-Wire    | R3 4k7 pull-up to 3.3V        |
// | On-board LED (heartbeat)   | GPIO 15 | OUTPUT    | LEDOUT in 01_Main.h           |
// | IR_PICKUP                  | GPIO 17 | INPUT     | R8 10k pull-up to 3.3V,       |
// |                            |         |           | pickup pulls LOW when lit     |
//
// !! DS18S20 "INTERNAL" — on RAINMETER_V1 its DQ (net N$1) only went to R1
//    and was not routed to any GPIO.  Fixed by hand: wired to GPIO 1
//    (1/10/2026).  Update the schematic/board for the next PCB revision.
//
// None of the pins used is an ESP32-S2 strapping pin (0, 45, 46).
// =============================================================================

	#if defined(ARDUINO_ARCH_ESP32)

// =============================================================================
// Output Pin Numbers  (OPIN_*)
// =============================================================================
	#define OPIN_IR_LED      2   // IR emitter via 2N2222 (T1) — HIGH = LED on

// =============================================================================
// Input Pin Numbers  (IPIN_*)
// =============================================================================
	#define IPIN_IR_PICKUP  17   // IR pickup, 10k external pull-up — LOW = light received
	#define IPIN_ONEWIRE_EXT 6   // DS18S20 "EXTERNAL" 1-Wire (R3 4k7 pull-up)
	#define IPIN_ONEWIRE_INT 1   // DS18S20 "INTERNAL" 1-Wire (R1 4k7 pull-up, hand-wired)

// =============================================================================
// Output Array Index Aliases  (OIDX_*)  — index into OutPort[]
// =============================================================================
	#define OIDX_IR_LED      0

// =============================================================================
// Input Array Index Aliases  (IIDX_*)  — index into InPort[]
// =============================================================================
	#define IIDX_IR_PICKUP   0

	#endif   // ARDUINO_ARCH_ESP32

// =============================================================================
// Input Debounce Engine
// InPort[].Input is the debounced pin level (true = HIGH).  The IR pickup has
// its own external pull-up (R8); the internal pull-up is enabled as well so a
// board without the PCB doesn't count noise on a floating pin as tips.
// The steady time comes from Settings (Params->TipDebounceMs, applied by
// ApplyRainSettings()): the bucket rests in one position between tips, so a
// few hundred ms filters any bounce as it lands without missing real tips.
// =============================================================================
	inline int DebounceSteadyMs = 250;

// =============================================================================
// IR Beam Self-Test  (MaintainIrSelfTest, 05_ThisSystem.cpp)
// Every Params->IrTestMin minutes, while the bucket is still, the IR LED is
// switched off for IR_TEST_OFF_MS: the pickup must go dark, then see light
// again once the LED is back on.  PinDebounce is frozen meanwhile, so the
// test can never count as a tip.  Only possible while the beam is OPEN — with
// the bucket resting on the beam-blocking side the pickup is dark either way.
// =============================================================================
	#define IR_TEST_OFF_MS       50                 // LED off this long (and settle after)
	#define IR_TEST_FIRST_MS     60000UL            // first test 1 min after boot
	#define IR_TEST_RETRY_MS     60000UL            // retest after a fail / busy bucket
	#define IR_TEST_QUIET_MS     60000UL            // no tip in this long = bucket is still
	#define IR_TEST_FAILS_ALERT  2                  // consecutive fails before the alert

	inline bool   IrTestBusy  = false;              // LED is being toggled — debounce frozen
	inline bool   IrTestFault = false;              // alert sent, not yet recovered
	inline String IrTestText  = "Not run yet";      // last result (HOME / NTFY page)
	inline volatile bool IrTestNowRequested = false; // NTFY page "TEST IR SENSOR NOW" (POST /irtest, async_tcp -> loop)

	struct DebounceStruct
	{
		const int    PinIO;
		bool         MemoryH;
		bool         MemoryL;
		bool         Input;
		unsigned long Time;
		bool         Status;
		String       Desc;
	};

	#define InputPorts   1

	inline DebounceStruct InPort[InputPorts] =
	{
		{ IPIN_IR_PICKUP, false, false, false, 0, false, "IR PICKUP" },
	};

// =============================================================================
// Output Port Structure
// =============================================================================
	struct OutPortStruct
	{
		int    Port;
		String Desc;
		bool   OUTStat;
	};

	#define OutPorts   1

	inline OutPortStruct OutPort[OutPorts] =
	{
		{ OPIN_IR_LED, "IR_LED", false },
	};

// =============================================================================
// Navigation Page Index Constants  (PS_*)  — tab strip in appendMainPageSel()
// =============================================================================
	#define PS_HOME       1
	#define PS_SETTINGS   2
	#define PS_REPORTS    3
	#define PS_FILES      4
	#define PS_NTFY       5
	#define PS_ABOUT      6

// =============================================================================
// WT_* Table-Builder Type Constants
// =============================================================================
	#define WT_LABEL   0
	#define WT_INPUT   1
	#define WT_FIRST   2
	#define WT_NEXT    3
	#define WT_MORE    5
	#define WT_LAST    4
