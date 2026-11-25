// =============================================================================
// 17_Temperature.cpp — DS18S20 Internal / External Temperatures
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire           owExt(IPIN_ONEWIRE_EXT);
static OneWire           owInt(IPIN_ONEWIRE_INT);
static DallasTemperature dsExt(&owExt);
static DallasTemperature dsInt(&owInt);

static void startBus(DallasTemperature& ds){
	ds.begin();                         // bus search — finds the sensor if present
	ds.setWaitForConversion(false);     // requestTemperatures() returns at once
	ds.requestTemperatures();
}

// Read the finished conversion; re-search the bus if the sensor is missing
// (so a sensor plugged in later is picked up), then start the next one.
static float readBus(DallasTemperature& ds){
	float c = NAN;
	if (ds.getDeviceCount() == 0) {
		ds.begin();
		ds.setWaitForConversion(false);
	} else {
		float t = ds.getTempCByIndex(0);
		if (t != DEVICE_DISCONNECTED_C) c = t;
	}
	ds.requestTemperatures();
	return c;
}

void SetupTemperature(){
	startBus(dsExt);
	startBus(dsInt);
	DEBUG_PRINTF("Temperature: external %d sensor(s), internal %d sensor(s)\n",
	             dsExt.getDeviceCount(), dsInt.getDeviceCount());
}

// Called every 10 ms from loop(); does real work once per TEMP_PERIOD_MS.
void MaintainTemperature(){
	static uint32_t last = 0;
	if (millis() - last < TEMP_PERIOD_MS) return;
	last = millis();
	if (SimActive){                          // 18_Simulation.cpp — sensors ignored
		TempExternal = SimOutsideTemp();
		TempInternal = Sim->TempIn;
	} else {
		TempExternal = readBus(dsExt);
		TempInternal = readBus(dsInt);
	}
	if (RainTimeValid()){
		WaterBalanceTrackTemp(TempExternal);                 // daily Tmin/Tmax for ET0  (20_WaterBalance.cpp)
		TempHistoryTrack(TempExternal, TempInternal);        // daily history + records  (21_TempHistory.cpp)
	}
	CheckTempAlerts(TempExternal);                           // NTFY high / low outside alerts (13_Ntfy.cpp)
}

// ── Display unit (SETTINGS "Temperature Unit") ─────────────────────────────
// Everything is measured and STORED in °C (history files, water balance, NTFY
// limits, /api/irrigate).  Only what a person reads is converted.
bool TempIsF(){ return Params && Params->TempUnit == "Fahrenheit"; }
const char* TempUnitSym(){ return TempIsF() ? "°F" : "°C"; }
// °C -> display unit (a difference, e.g. a 1 °C hysteresis, uses TempDeltaDisp).
float TempDisp(float c){ return TempIsF() ? c * 9.0f / 5.0f + 32.0f : c; }
float TempDeltaDisp(float dc){ return TempIsF() ? dc * 9.0f / 5.0f : dc; }
// display unit -> °C (for values typed on a page)
float TempFromDisp(float v){ return TempIsF() ? (v - 32.0f) * 5.0f / 9.0f : v; }
// "23.5" in the display unit, "" for NAN
String TempNum(float c){ return isnan(c) ? String("") : String(TempDisp(c), 1); }
// A °C value kept as text in a log line ("23.5", or "" = none) -> display unit
String TempNumStr(const String& cText){ return cText.length() ? String(TempDisp(cText.toFloat()), 1) : cText; }

// "23.5 °C" / "74.3 °F" or "--" when the sensor isn't there.
String TempText(float c){
	if (isnan(c)) return "--";
	return TempNum(c) + " " + TempUnitSym();
}
