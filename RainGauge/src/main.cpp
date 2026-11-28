// =============================================================================
// main.cpp — Boot Sequence & Cooperative Main Loop
// =============================================================================
//
// Project   : RainGauge — tipping-bucket rain meter with an IR beam pickup
// Platform  : WEMOS LOLIN S2 Mini (ESP32-S2, single-core LX7, 240 MHz,
//             4 MB Flash, 2 MB PSRAM, 320 KB SRAM)
// Framework : Arduino  (PlatformIO, pioarduino arduino-esp32 3.x)
// PCB       : RAINMETER_V1
//
// Skeleton taken from KamerHeadboard/Headboard — see that project's main.cpp
// for why everything runs in loop() on millis() timers instead of FreeRTOS
// tasks (on the single-core S2 that reasoning applies even more strongly).
//
// loop() schedule:
//   every  10 ms : LEDToggle, MaintainGatewayWatch, MaintainTimers, PinDebounce,
//                  MaintainIrSelfTest, MaintainNtfy, MaintainWsCommands,
//                  MaintainTempRecordsReset, MaintainRain, MaintainTemperature,
//                  MaintainWaterBalance, MaintainScheduledReboot,
//                  MaintainHeapGuard
//
// No application tasks: code that runs in async_tcp / esp_timer only queues
// or flags work for loop() — see CLAUDE.md "Everything runs in loop()".
//   every  50 ms : MaintainWebPage           (inside MaintainWebService)
//   every 200 ms : ArduinoOTA.handle         (inside MaintainWebService)
//   every 500 ms : MaintainHomePage, SendCalibrationValues (inside MaintainWebService)
//   every pass   : UpdateWebPageTime (self-limits to 1/s), feed watchdog, delay(1)
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// Main loop() runs all maintenance.  8 KB (the Arduino default) — it was
// 12 KB but used only ~2.5 KB (ABOUT page: "loopTask ... stack free (min)").
// If that free figure ever drops below ~2 KB, raise it again.
SET_LOOP_TASK_STACK_SIZE(8192);

static uint32_t lastPrint = 0;
String PrevMessage = "";
String FSInitLog   = "";

void setup(){
	Serial.begin(115200);
	// Headless-safe logging: every Serial write is gated on `if (Serial)`
	// (DEBUG_* in 01_Main.h) and a small, non-zero TX timeout bounds a USB
	// host that vanishes mid-write.
	Serial.setTxTimeoutMs(10);
	#if DEBUG
		// Give a serial monitor time to attach so it catches the boot log — but
		// only actually wait while none is connected.
		for (int i = 0; i < 50 && !Serial; i++) delay(100);
		if (Serial) delay(1500);
	#endif

	// --- Boot banner (best-effort; skipped when no USB host is attached) ---
	if (Serial) {
		Serial.println();
		Serial.println(F("======================================================"));
		Serial.printf ("  RainGauge  |  build %s %s\n", __DATE__, __TIME__);
		Serial.printf ("  reset reason : %d\n", (int) esp_reset_reason());
		Serial.printf ("  free heap    : %u bytes\n", ESP.getFreeHeap());
		Serial.printf ("  PSRAM        : %u bytes\n", ESP.getPsramSize());
		Serial.println(F("======================================================"));
	}

	InitHardware();

	InitFileSystem();

	SetupTemperature();

	DEBUG_PRINTLN(F("\nBootUp RainGauge  (hardware + filesystem init done)"));

	if (FSInitLog.length() > 0) DEBUG_PRINT(FSInitLog);

	// Join the saved default hotspot, or scan for a known one (05_WiFi.cpp);
	// reboot if every attempt fails.
	if (!SetupWiFi()) {
		SaveGenReport F("REBOOT - WiFi connect failed");
		DEBUG_PRINTLN(F("\n*** WiFi connect FAILED - restarting in 2s ***\n"));
		delay(2000);
		ESP.restart();
	}
	else {

		DEBUG_PRINTLN(F("*** WiFi connected ***"));
		SetupTime();
		SetupAbout();
		SetupOTA();
		SetupWebPage();
		UpdateTimerTimes();

		SaveGenReport("- Up and Running after Reboot  (" + WiFi.SSID() + "  " +
		              WiFi.localIP().toString() + ", heap " + String(ESP.getFreeHeap()) + ")");
		LogWatchdogReboot();       // "REBOOT - Watchdog" if that caused this boot (01_Main.cpp)

		localIP = WiFi.localIP().toString();

		WatchdogSetup();
		activateTimer(TIM_STABILIZE_DELAY);

		DEBUG_PRINTLN("Setup complete — entering main loop");
	}
}

void loop(){
	// During an OTA flash, do nothing but pump the update stream as fast as
	// possible — anything else can stall or kill the transfer.
	if (OTAInProgress){
		ArduinoOTA.handle();
		delay(1);
		return;
	}

	const uint32_t now = millis();
	watchdogCount = 0;                 // feed the software watchdog (01_Main.cpp)

	static uint32_t t10 = 0;
	if (now - t10 >= 10){
		t10 = now;
		LEDToggle();                   // LED heartbeat, WiFi-loss reboot
		MaintainGatewayWatch();        // KEEPALIVE ping + reboot if the router stops answering for 1 min — do NOT remove
		MaintainTimers();              // expire elapsed software timers
		PinDebounce();                 // debounce the IR pickup input
		MaintainIrSelfTest();          // IR LED off/on check every N min -> NTFY on fault
		MaintainNtfy();                // send queued NTFY messages (AsyncClient, never blocks)
		MaintainWsCommands();          // apply WebSocket commands queued by async_tcp
		MaintainTempRecordsReset();    // REPORTS "RESET RECORDS" (flag from async_tcp)
		MaintainStartFresh();          // SETTINGS "START FRESH" — wipe data files + restart (flag from async_tcp)
		MaintainAccess();              // remote access password save + login log lines (23_Access.cpp)
		MaintainWifiNets();            // SETTINGS "WiFi Networks" add / delete (queued by async_tcp)
		MaintainFwUpdate();            // ABOUT firmware CHECK / INSTALL from GitHub (blocking, by hand) + restart after upload
		MaintainRain();                // count tips, calibration, log rain per 30 s window
		MaintainTemperature();         // DS18S20 x2, every 10 s (non-blocking)
		MaintainWaterBalance();        // daily soil-water balance + latch at irrigation time
		MaintainScheduledReboot();     // scheduled daily reboot
		MaintainHeapGuard();           // low-heap / fragmentation self-reboot
	}

	MaintainWebService();             // ArduinoOTA + web/WebSocket upkeep (self-timed)

	delay(1);                          // yield to AsyncTCP / WiFi / IDLE task
}

void PrintTestMsg(String Message, uint32_t seconds){
	if (Message != PrevMessage) {
		PrevMessage = Message;
		lastPrint   = millis();
		DEBUG_PRINTLN(Message);
	} else if (millis() - lastPrint >= seconds * 1000UL) {
		lastPrint = millis();
		DEBUG_PRINTLN(Message);
	}
}
