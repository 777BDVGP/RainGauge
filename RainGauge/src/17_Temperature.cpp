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

// "23.5 °C" or "--" when the sensor isn't there.
String TempText(float c){
	if (isnan(c)) return "--";
	return String(c, 1) + " °C";
}
