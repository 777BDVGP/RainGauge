// =============================================================================
// 05_ThisSystem.cpp — I/O Setup, Input Debounce & HTTP Routes
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// SetupSystemIOs
// Initialises every board I/O per the RAINMETER_V1 pin map (05_ThisSystem.h):
//   IR LED (T1 2N2222)  : OUTPUT, switched ON at boot and left on
//   IR pickup           : INPUT_PULLUP (R8 10k does the work on the PCB; the
//                         internal pull-up keeps a bare S2 Mini from reading
//                         a floating pin as a stream of phantom tips)
// -----------------------------------------------------------------------------
void SetupSystemIOs()
{
	for (int i = 0; i < OutPorts; i++){
		OutPort[i].OUTStat = false;
		pinMode(OutPort[i].Port, OUTPUT);
		digitalWrite(OutPort[i].Port, LOW);
	}

	for (int i = 0; i < InputPorts; i++){
		pinMode(InPort[i].PinIO, INPUT_PULLUP);
	}

	// The beam must be up before the first bucket tip can be seen.
	SetIrLed(true);
}

// -----------------------------------------------------------------------------
// IR emitter — GPIO 2 drives the 2N2222 base through 10k; the LED runs from
// +5V through 220R (~16 mA).  Kept switchable so a later self-test can turn
// it off and confirm the pickup goes dark (sensor health / ambient-light check).
// -----------------------------------------------------------------------------
void SetIrLed(bool on){
	digitalWrite(OPIN_IR_LED, on ? HIGH : LOW);
	OutPort[OIDX_IR_LED].OUTStat = on;
}

bool GetIrLed(){
	return OutPort[OIDX_IR_LED].OUTStat;
}

void PinDebounce() {
    if (IrTestBusy) return;          // IR self-test has the LED off — not a tip
    unsigned long now = millis();
    for (int i = 0; i < InputPorts; i++) {
        bool reading = digitalRead(InPort[i].PinIO);

        if (reading && !InPort[i].Input) {
            InPort[i].MemoryL = false;
            if (!InPort[i].MemoryH) {
                InPort[i].Time = now;
                InPort[i].MemoryH = true;
            } else if ((now - InPort[i].Time) >= (unsigned long)DebounceSteadyMs) {
                InPort[i].Input = true;
            }
        } else if (!reading && InPort[i].Input) {
            InPort[i].MemoryH = false;
            if (!InPort[i].MemoryL) {
                InPort[i].Time = now;
                InPort[i].MemoryL = true;
            } else if ((now - InPort[i].Time) >= (unsigned long)DebounceSteadyMs) {
                InPort[i].Input = false;
            }
        } else {
            InPort[i].MemoryH = InPort[i].MemoryL = false;
        }
    }
}

// -----------------------------------------------------------------------------
// MaintainIrSelfTest  (called every 10 ms from loop(), after PinDebounce)
// See 05_ThisSystem.h.  Pickup: LOW = light received, HIGH = dark.
//   beam open, LED off -> HIGH, LED on -> LOW   : OK
//   beam open, LED off -> still LOW             : FAIL (sunlight getting in,
//                                                 or pickup stuck LOW)
//   beam blocked (HIGH before the test)         : skipped, can't tell
//   level changed during the test               : bucket moved, retry
// IR_TEST_FAILS_ALERT fails in a row -> General Log + NTFY; another message
// when it passes again.
// -----------------------------------------------------------------------------
static void irTestAlert(const String& msg){
	SaveGenReport("IR self-test: " + msg);
	if (NtfyData && NtfyData->EnableIrFaultNotify)
		MessageNTFY(Params->SysName + " - IR rain sensor: " + msg);
}

void MaintainIrSelfTest(){
	enum { IRT_IDLE, IRT_DARK, IRT_RELIT };
	static uint8_t  state   = IRT_IDLE;
	static uint32_t t       = 0;
	static uint32_t nextDue = IR_TEST_FIRST_MS;
	static uint8_t  fails   = 0;
	static bool     before  = false, dark = false;
	static bool     manual  = false;      // the running test was started from the NTFY page
	static bool     manualRan = false;    // keep a manual result visible while the timed test is off
	uint32_t now = millis();

	switch (state){
	case IRT_IDLE: {
		// ── Manual test (NTFY page button): now, whatever the interval —
		//    but only when it can tell something, and say why not otherwise.
		if (IrTestNowRequested){
			IrTestNowRequested = false;
			manualRan = true;
			String when = RainTimeValid() ? GetCurrentDateAndTime() + " - " : String("");
			before = digitalRead(IPIN_IR_PICKUP);
			if (SimActive)  { IrTestText = when + "Manual test not possible - SIMULATION is on"; return; }
			if (CalActive)  { IrTestText = when + "Manual test not possible - CALIBRATION is running"; return; }
			if (before != InPort[IIDX_IR_PICKUP].Input || (RainLastTipMs && now - RainLastTipMs < 2000)){
				IrTestText = when + "Manual test not possible - bucket is moving, try again";
				return;
			}
			if (before){
				IrTestText = when + "Manual test not possible - beam is BROKEN (bucket blocking it). "
				                    "Tip the bucket so the beam is OPEN (green) and test again";
				return;
			}
			manual = true;
			IrTestBusy = true;
			SetIrLed(false);
			t = now; state = IRT_DARK;
			return;
		}
		if (Params->IrTestMin <= 0){ if (!manualRan) IrTestText = "Off (SETTINGS)"; return; }
		if ((int32_t)(now - nextDue) < 0) return;
		manual = false;
		// Only with the bucket still and the debounced level settled.
		before = digitalRead(IPIN_IR_PICKUP);
		if (SimActive || CalActive ||
		    (RainLastTipMs && now - RainLastTipMs < IR_TEST_QUIET_MS) ||
		    before != InPort[IIDX_IR_PICKUP].Input){
			nextDue = now + IR_TEST_RETRY_MS;
			return;
		}
		// Beam already blocked by the bucket: the pickup is dark with the LED
		// on or off, so blinking it proves nothing — skip without touching it.
		if (before){
			String when = RainTimeValid() ? GetCurrentDateAndTime() + " - " : String("");
			IrTestText = when + "Skipped - bucket is blocking the beam (LED can't be checked)";
			nextDue = now + (uint32_t)Params->IrTestMin * 60000UL;
			return;
		}
		IrTestBusy = true;
		SetIrLed(false);
		t = now; state = IRT_DARK;
		return;
	}

	case IRT_DARK:
		if (now - t < IR_TEST_OFF_MS) return;
		dark = digitalRead(IPIN_IR_PICKUP);
		SetIrLed(true);
		t = now; state = IRT_RELIT;
		return;

	case IRT_RELIT: {
		if (now - t < IR_TEST_OFF_MS) return;
		bool after = digitalRead(IPIN_IR_PICKUP);
		IrTestBusy = false;
		state = IRT_IDLE;

		uint32_t interval = (uint32_t)Params->IrTestMin * 60000UL;
		String   when     = (RainTimeValid() ? GetCurrentDateAndTime() + " - " : String("")) +
		                    (manual ? "Manual test: " : "");
		manual = false;

		if (after != before){
			IrTestText = when + "Retry - bucket moved during the test";
			nextDue = now + IR_TEST_RETRY_MS;
		}
		else if (!dark){
			if (fails < 255) fails++;
			IrTestText = when + "FAIL - pickup still sees light with the IR LED off";
			if (fails == IR_TEST_FAILS_ALERT && !IrTestFault){
				IrTestFault = true;
				irTestAlert("FAULT - pickup sees light with the IR LED off (sunlight getting in, or pickup faulty)");
			}
			nextDue = now + (IrTestFault ? interval : IR_TEST_RETRY_MS);
		}
		else {
			if (IrTestFault) irTestAlert("OK again - beam goes dark with the IR LED off");
			IrTestFault = false;
			fails       = 0;
			IrTestText  = when + "OK - beam goes dark with the IR LED off";
			nextDue = now + interval;
		}
		return;
	}
	}
}

// -----------------------------------------------------------------------------
// MaintainWebPageValues  (called every 50 ms from MaintainWebPage)
// -----------------------------------------------------------------------------
void MaintainWebPageValues(){
	SendSettingsPageValues();
	if (MainPageObject)  SendHomePageValues();
	if (NtfyPageObject)  SendNtfyPageValues();   // 13_Ntfy.cpp
	SendFwValues();                              // ABOUT firmware update status (22_Update.cpp)
}

// -----------------------------------------------------------------------------
// AddRoutes
// Registers all HTTP GET/POST route handlers with the AsyncWebServer.
// -----------------------------------------------------------------------------
void AddRoutes(){

	// ── Main Pages (tab strip order) ──────────────────────────────────────────
	server.on("/",         handleRoot);      // Home page        (08_Home.cpp)
	server.on("/settings", handleSettings);  // System settings  (12_Settings.cpp)
	server.on("/calibrate",handleCalibrate); // Rain calibration (16_Rain.cpp) — via SETTINGS
	// Leaving the CALIBRATION page stops a running calibration (sendBeacon on pagehide).
	// Only raises a flag — MaintainRain() does the stop in loop().
	server.on("/calstop", HTTP_POST, [](AsyncWebServerRequest *request){ CalStopRequested = true; request->send(204); });
	server.on("/simulate", handleSimulate);  // Simulation mode  (18_Simulation.cpp) — via SETTINGS
	server.on("/rain",         handleRainReport);     // REPORTS > RAIN          (10_Reports.cpp)
	server.on("/temperature",  handleTempReport);     // REPORTS > TEMPERATURE   (10_Reports.cpp)
	server.on("/waterbalance", handleWaterBalReport); // REPORTS > WATER BALANCE (10_Reports.cpp)
	server.on("/reports",      handleReports);        // old link -> /rain
	server.on("/ntfy",     handleNtfy);      // NTFY push        (13_Ntfy.cpp)
	server.on("/testntfy", handleTestNtfy);  // Send test NTFY   (13_Ntfy.cpp)
	server.on("/irtest", HTTP_POST, handleIrTestNow); // NTFY page "TEST IR SENSOR NOW" (13_Ntfy.cpp)
	server.on("/api/sysinfo", handleApiSysInfo); // Heap + tasks, plain text, no 503 guard (15_About.cpp)
	server.on("/about",    handleAbout);     // About / OTA      (15_About.cpp)
	server.on("/wss.js",   handleWssJs);     // Cached client script (04_WebPages.cpp)
	server.on("/api/irrigate", handleApiIrrigate); // JSON answer for the irrigation controller (19_Irrigation.cpp)
	server.on("/resettemprecords", handleResetTempRecords); // TEMPERATURE page button (21_TempHistory.cpp)
	server.on("/startfresh", HTTP_POST, handleStartFresh);  // SETTINGS "START FRESH" (12_Settings.cpp)
	server.on("/wifinet/add",    HTTP_POST, handleWifiNetAdd);    // SETTINGS "WiFi Networks" (05_WiFi.cpp)
	server.on("/wifinet/delete", HTTP_POST, handleWifiNetDelete);
	server.on("/wifiscan",       HTTP_GET,  handleWifiScan);      // SETTINGS "Networks in range" (05_WiFi.cpp)
	server.on("/fwcheck",    HTTP_POST, handleFwCheck);          // ABOUT "CHECK FOR UPDATE" (22_Update.cpp)
	server.on("/fwinstall",  HTTP_POST, handleFwInstall);        // ABOUT "INSTALL FROM GITHUB"
	server.on("/update",     HTTP_POST, handleFwUploadDone, handleFwUploadData);  // ABOUT ".bin upload"

	// ── File Manager ──────────────────────────────────────────────────────────
	server.on("/files/view",   handleFileView);
	server.on("/files/delete", handleFileDeleteJson);
	server.on("/api/files",    handleApiFiles);
	server.on("/files",        FilesPage);
	server.on("/download",     File_Download);
	server.on("/fupload", HTTP_POST,
		[](AsyncWebServerRequest *request){ request->redirect("/files"); },
		handleFileUpload
	);
}
