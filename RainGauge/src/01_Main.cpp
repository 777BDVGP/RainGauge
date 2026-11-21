// =============================================================================
// 01_Main.cpp — Hardware Initialisation, Watchdog & LED Heartbeat
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"
#include "lwip/sockets.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/icmp.h"
#include "lwip/prot/ip4.h"

void InitHardware(){
#ifdef LEDOUT
	pinMode(LEDOUT, OUTPUT);
#endif
	Heartbeat(true);

	SetupSystemIOs();
}

// Heartbeat indicator — the S2 Mini on-board LED (GPIO 15, active HIGH).
void Heartbeat(bool on){
	HeartbeatState = on;
#ifdef LEDOUT
	digitalWrite(LEDOUT, on);
#endif
}

void HeartbeatToggle(){
	Heartbeat(!HeartbeatState);
}

void WatchdogSetup() {
	secondTick.attach(1, ISRwatchdog);
}

// Runs in the esp_timer task (Ticker) — so no SPIFFS write here: if loop()
// hung inside a file operation, that write could block too.  The reason is
// left in RTC memory (survives a software restart) and logged by
// LogWatchdogReboot() from setup() after the reboot.
RTC_NOINIT_ATTR static uint32_t rtcRebootFlag;
#define RTC_FLAG_WATCHDOG  0x57444F47UL   // "WDOG"

void ISRwatchdog() {
	watchdogCount++;
	if (watchdogCount > 15)
	{
		rtcRebootFlag = RTC_FLAG_WATCHDOG;
		ESP.restart();
	}
}

// Called once from setup() after the "Up and Running" line.
void LogWatchdogReboot() {
	if (rtcRebootFlag == RTC_FLAG_WATCHDOG)
		SaveGenReport F("REBOOT - Watchdog (loop() stalled > 15 s) - logged after the restart");
	rtcRebootFlag = 0;
}

void LEDToggle(){

	watchdogCount = 0;

	if (WiFi.status() != WL_CONNECTED) {
		if (!Timer[TIM_WIFI_RECONNECT].Status) {
			activateTimer(TIM_WIFI_RECONNECT);
		} else if (TimerExpired(TIM_WIFI_RECONNECT)) {
			SaveGenReport F("REBOOT - WiFi Lost");
			delay(500);
			ESP.restart();
		}
	} else {
		deactivateTimer(TIM_WIFI_RECONNECT);
	}

	if(TimerExpired(TIM_LED_FLASHING_SPEED)){
		activateTimer(TIM_LED_FLASHING_SPEED);
		HeartbeatToggle();
	}
}

// ── Gateway reachability guard state ────────────────────────────────────────
// The ping is a non-blocking raw ICMP socket polled from loop() — no ping
// task (esp_ping would keep its own task + stack on the heap for good).
// wifiDrop* are written from the WiFi event task, the rest only from loop().
#define GW_PING_ID   0x5247                      // "RG" — our echo identifier
static int                gwSock          = -1;
static uint16_t           gwSeq           = 0;
static bool               gwWaiting       = false;
static uint32_t           gwSentMs        = 0;
static uint32_t           gwLastReplyMs   = 0;
static uint32_t           gwMissed        = 0;   // Consecutive ping timeouts
static volatile uint32_t  wifiDropCount   = 0;   // STA_DISCONNECTED events seen
static volatile uint8_t   wifiDropReason  = 0;   // wifi_err_reason_t of the last one

// Send one echo request to the gateway.
static void gwPingSend(){
	struct icmp_echo_hdr h = {};
	ICMPH_TYPE_SET(&h, ICMP_ECHO);
	h.id     = htons(GW_PING_ID);
	h.seqno  = htons(++gwSeq);
	h.chksum = inet_chksum(&h, sizeof(h));

	struct sockaddr_in to = {};
	to.sin_family      = AF_INET;
	to.sin_addr.s_addr = static_cast<uint32_t>(WiFi.gatewayIP());
	gwSentMs  = millis();                        // next try in GW_PING_INTERVAL_MS, sent or not
	gwWaiting = sendto(gwSock, &h, sizeof(h), 0, (struct sockaddr*)&to, sizeof(to)) == (int)sizeof(h);
}

// Drain the socket (never waits); our reply -> stamp it.  A raw IPv4 socket
// hands over every ICMP packet with its IP header in front.
static void gwPingPoll(){
	uint8_t buf[64];
	int n;
	while ((n = recvfrom(gwSock, buf, sizeof(buf), MSG_DONTWAIT, nullptr, nullptr)) > 0){
		int hl = IPH_HL((struct ip_hdr*)buf) * 4;
		if (n < hl + (int)sizeof(struct icmp_echo_hdr)) continue;
		struct icmp_echo_hdr* r = (struct icmp_echo_hdr*)(buf + hl);
		if (ICMPH_TYPE(r) == ICMP_ER && r->id == htons(GW_PING_ID) && r->seqno == htons(gwSeq)){
			gwLastReplyMs = millis();
			gwMissed      = 0;
			gwWaiting     = false;
		}
	}
	if (gwWaiting && millis() - gwSentMs > GW_PING_TIMEOUT_MS){
		gwWaiting = false;
		gwMissed++;
	}
}

// -----------------------------------------------------------------------------
// MaintainGatewayWatch  (called every 10 ms from loop())
// WiFi.status() can keep saying WL_CONNECTED after the device has silently
// dropped off the network — it then answers neither ping nor ARP, while
// loop() keeps running so neither the software watchdog nor the WiFi-loss
// guard in LEDToggle() fires.  This pings the router every GW_PING_INTERVAL_MS
// from a non-blocking raw socket polled here (no ping task) and reboots if no reply arrives for
// GW_UNREACHABLE_MS.  Also logs every WiFi disconnect event with its reason
// code.  The ping doubles as a KEEPALIVE (see Headboard project history), so
// this function must stay even once the diagnostics are no longer needed.
// -----------------------------------------------------------------------------
void MaintainGatewayWatch(){
	static uint32_t loggedDrops = 0;
	if (wifiDropCount != loggedDrops){
		loggedDrops = wifiDropCount;
		SaveGenReport("WiFi disconnected, reason " + String(wifiDropReason) +
		              " (drops since boot: " + String(loggedDrops) + ")");
	}

	if (gwSock < 0){
		if (WiFi.status() != WL_CONNECTED) return;

		static bool eventHooked = false;
		if (!eventHooked){
			eventHooked = true;
			WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info){
				wifiDropReason = info.wifi_sta_disconnected.reason;
				wifiDropCount++;
			}, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
		}

		gwSock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (gwSock < 0) return;                  // retried on the next pass
		fcntl(gwSock, F_SETFL, O_NONBLOCK);
		gwLastReplyMs = millis();
		gwPingSend();
		return;
	}

	gwPingPoll();
	if (millis() - gwSentMs >= GW_PING_INTERVAL_MS) gwPingSend();

	if (millis() - gwLastReplyMs > GW_UNREACHABLE_MS){
		SaveGenReport("REBOOT - Gateway unreachable " + String(GW_UNREACHABLE_MS / 60000UL) +
		              " min (missed " + String(gwMissed) + ", status " + String(WiFi.status()) +
		              ", RSSI " + String(WiFi.RSSI()) + ", heap " + String(ESP.getFreeHeap()) +
		              ", min heap " + String(ESP.getMinFreeHeap()) + ")");
		delay(500);
		ESP.restart();
	}
}

void UpdateTimerTimes(){
	// Reserved — copies user-configurable delays from Params into Timer[].
	// Application timers will be wired here when the rain functions are added.
}
