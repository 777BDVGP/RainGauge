// =============================================================================
// 05_WiFi.cpp — WiFi join (saved config / known networks) + setup hotspot
// =============================================================================
// See 05_WiFi.h for the order in which SetupWiFi() tries things.
//
// /WiFiConfig.json  { ssid, pass, dhcp, ip, gw, sn, dns }
//   Written by the setup hotspot page, or automatically when the gauge joins
//   one of the KNOWN_NETS.  Holds the WiFi password, so the FILES page never
//   lists, shows or downloads it (IsProtectedFile, 03_Files.h).
//
// KNOWN_NETS come only from the private, git-ignored wifi_secrets.h
// (WIFI_KNOWN_NETS).  The public firmware has none: a new gauge goes straight
// to the setup hotspot.  The private firmware saves the network it joined in
// /WiFiConfig.json, so a later public firmware (update from GitHub) still
// connects to the same network on the same IP.
// =============================================================================

#include <Arduino.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <climits>
#include "all_includes.h"

// Optional private networks (wifi_secrets.h is NOT in Git).
#if __has_include("wifi_secrets.h")
	#include "wifi_secrets.h"
#endif

struct KnownNet {
	const char* ssid;
	const char* pass;
	uint8_t     ip[4];   // fixed IP on that network (http://<ip>/)
};

#ifdef WIFI_KNOWN_NETS
	static const KnownNet KNOWN_NETS[] = { WIFI_KNOWN_NETS };
	static const int KNOWN_NET_COUNT = sizeof(KNOWN_NETS) / sizeof(KNOWN_NETS[0]);
#else
	static const KnownNet* const KNOWN_NETS = nullptr;   // public firmware: setup hotspot only
	static const int KNOWN_NET_COUNT = 0;
#endif

#define WIFI_HOSTNAME        "raingauge"        // mDNS: http://raingauge.local/
#define WIFI_SETUP_SSID      "RainGauge-Setup"  // setup hotspot (open, 192.168.4.1)
#define WIFI_ROUNDS          6                  // full (saved + scan) rounds before the setup hotspot
#define WIFI_SAVED_TRIES     2                  // attempts on the saved network per round
#define WIFI_CONNECT_MS      15000UL            // per join attempt
#define WIFI_ROUND_GAP_MS    10000UL            // pause between rounds (router may be rebooting)
#define WIFI_PORTAL_MS       (5UL * 60UL * 1000UL)  // setup hotspot after failed rounds: 5 min, then reboot

// The network as saved in /WiFiConfig.json
struct WifiCfg {
	String    ssid, pass;
	bool      dhcp = true;
	IPAddress ip, gw, sn, dns;
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static const KnownNet* findKnown(const String& ssid){
	for (int i = 0; i < KNOWN_NET_COUNT; i++)
		if (ssid == KNOWN_NETS[i].ssid) return &KNOWN_NETS[i];
	return nullptr;
}

// Wait for WL_CONNECTED, flickering the heartbeat LED so "searching" is visible.
static bool waitConnected(){
	unsigned long t = millis();
	while (WiFi.status() != WL_CONNECTED) {
		if (millis() - t > WIFI_CONNECT_MS) {
			DEBUG_PRINTLN("WiFi: connection timed out");
			return false;
		}
		delay(100);
		HeartbeatToggle();
	}
	return true;
}

static void dropConnection(){
	WiFi.disconnect(false);
	delay(300);
}

static bool joinWith(const WifiCfg& c){
	DEBUG_PRINTF("WiFi: joining %s (%s)\n", c.ssid.c_str(), c.dhcp ? "DHCP" : c.ip.toString().c_str());
	if (c.dhcp) WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));   // 0.0.0.0 = DHCP
	else        WiFi.config(c.ip, c.gw, c.sn, c.dns);
	WiFi.begin(c.ssid.c_str(), c.pass.c_str());
	return waitConnected();
}

// ── /WiFiConfig.json ─────────────────────────────────────────────────────────

static bool loadCfg(WifiCfg& c){
	File fr = SPIFFS.open(StrWiFiConfig, "r");
	if (!fr) return false;
	StaticJsonDocument<512> doc;
	DeserializationError err = deserializeJson(doc, fr);
	fr.close();
	if (err) return false;
	c.ssid = doc["ssid"] | "";
	c.pass = doc["pass"] | "";
	c.dhcp = doc["dhcp"] | true;
	if (c.ssid.length() == 0) return false;
	if (!c.dhcp){
		bool ok = c.ip.fromString(doc["ip"] | "") && c.gw.fromString(doc["gw"] | "") &&
		          c.sn.fromString(doc["sn"] | "") && c.dns.fromString(doc["dns"] | "");
		if (!ok) c.dhcp = true;       // broken fixed-IP entry: fall back to DHCP
	}
	return true;
}

static void saveCfg(const WifiCfg& c){
	StaticJsonDocument<512> doc;
	doc["ssid"] = c.ssid;
	doc["pass"] = c.pass;
	doc["dhcp"] = c.dhcp;
	if (!c.dhcp){
		doc["ip"]  = c.ip.toString();
		doc["gw"]  = c.gw.toString();
		doc["sn"]  = c.sn.toString();
		doc["dns"] = c.dns.toString();
	}
	File fw = SPIFFS.open(StrWiFiConfig, "w");
	if (!fw) { SaveGenReport F("WiFi: could not write /WiFiConfig.json"); return; }
	serializeJson(doc, fw);
	fw.close();
	SPIFFS.remove(StrWiFiDefault);   // the old (pre-1.8) file is not used any more
}

// ── Step 1: saved network ────────────────────────────────────────────────────

static bool joinSaved(){
	WifiCfg c;
	if (!loadCfg(c)) return false;
	for (int i = 0; i < WIFI_SAVED_TRIES; i++) {
		if (i > 0) dropConnection();
		if (joinWith(c)) { WiFiSource = "saved"; return true; }
	}
	dropConnection();
	return false;
}

// ── Step 2: known networks (private firmware only) ───────────────────────────
// Joined by DHCP first to LEARN the gateway / subnet / DNS, then rejoined on
// the fixed IP from the table and saved in /WiFiConfig.json.

static bool scanAndJoinKnown(){
	if (KNOWN_NET_COUNT == 0) return false;
	DEBUG_PRINTLN("WiFi: scanning for known networks...");
	int n = WiFi.scanNetworks();

	int rssi[KNOWN_NET_COUNT + 1];
	for (int i = 0; i < KNOWN_NET_COUNT; i++) rssi[i] = INT_MIN;
	for (int j = 0; j < n; j++) {
		String found = WiFi.SSID(j);
		for (int i = 0; i < KNOWN_NET_COUNT; i++)
			if (found == KNOWN_NETS[i].ssid && WiFi.RSSI(j) > rssi[i]) rssi[i] = WiFi.RSSI(j);
	}
	WiFi.scanDelete();

	bool tried[KNOWN_NET_COUNT + 1] = { false };
	for (;;) {
		int best = -1;
		for (int i = 0; i < KNOWN_NET_COUNT; i++)
			if (!tried[i] && rssi[i] != INT_MIN && (best < 0 || rssi[i] > rssi[best])) best = i;
		if (best < 0) break;
		tried[best] = true;
		const KnownNet* k = &KNOWN_NETS[best];

		WifiCfg c;
		c.ssid = k->ssid; c.pass = k->pass; c.dhcp = true;
		if (!joinWith(c)) { dropConnection(); continue; }
		c.dhcp = false;
		c.ip   = IPAddress(k->ip[0], k->ip[1], k->ip[2], k->ip[3]);
		c.gw   = WiFi.gatewayIP();
		c.sn   = WiFi.subnetMask();
		c.dns  = WiFi.dnsIP();
		dropConnection();

		if (joinWith(c)) {
			saveCfg(c);
			WiFiSource = "known network";
			SaveGenReport("WiFi: joined " + c.ssid + " on " + c.ip.toString() + " - saved");
			return true;
		}
		dropConnection();
	}
	return false;
}

// ── Setup hotspot ────────────────────────────────────────────────────────────
// Open hotspot "RainGauge-Setup" (192.168.4.1) with a captive page: pick the
// network, password, automatic (DHCP) or fixed IP.  The async_tcp handler
// only copies the form into pending + raises a flag; the loop below saves
// it and reboots.  Always ends in a reboot (save, cancel or timeout).

static WifiCfg       pendingCfg;
static volatile bool pendingSave   = false;
static volatile bool pendingCancel = false;
static String        portalNetOptions;   // <option>s from the scan at start

static const char PORTAL_PAGE_1[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'><title>Rain Gauge WiFi Setup</title>
<style>body{font-family:sans-serif;max-width:420px;margin:0 auto;padding:12px;background:#f8fafc;color:#0f172a}
h2{color:#0284c7;text-align:center;margin:6px 0}label{display:block;margin:10px 0 3px;font-weight:600;font-size:14px}
input,select{width:100%;box-sizing:border-box;padding:8px;font-size:15px;border:1px solid #cbd5e1;border-radius:5px}
.r{display:flex;gap:16px;margin-top:6px}.r label{font-weight:400;margin:0}.r input{width:auto}
button{width:100%;padding:12px;margin-top:16px;font-size:16px;font-weight:700;border:0;border-radius:6px;color:#fff;background:#16a34a}
.c{background:#64748b}p{font-size:13px;color:#475569}</style></head><body>
<h2>Rain Gauge &mdash; WiFi Setup</h2>
<p>Choose the WiFi network the rain gauge must join. After SAVE it restarts and joins it.
Then open <b>http://raingauge.local/</b> (or the fixed IP you chose) on a phone or PC on that network.</p>
<form method='POST' action='/save'>
<label>WiFi network</label><select name='ssid'>)HTML";

static const char PORTAL_PAGE_2[] PROGMEM = R"HTML(<option value=''>Other (type below)...</option></select>
<label>...or type the network name</label><input name='ssid2' placeholder='only if not in the list'>
<label>WiFi password</label><input name='pass' type='password' placeholder='leave empty for an open network'>
<label>IP address</label>
<div class='r'><label><input type='radio' name='mode' value='dhcp' checked onclick="f(0)"> Automatic (DHCP)</label>
<label><input type='radio' name='mode' value='fixed' onclick="f(1)"> Fixed IP</label></div>
<div id='fx' style='display:none'>
<label>Fixed IP (e.g. 192.168.1.124)</label><input name='ip'>
<label>Gateway / router (e.g. 192.168.1.1)</label><input name='gw'>
<label>Subnet mask</label><input name='sn' value='255.255.255.0'>
<label>DNS (empty = same as gateway)</label><input name='dns'>
<p>A fixed IP is needed when another controller (e.g. an irrigation controller) reads the gauge.</p></div>
<button type='submit'>SAVE &amp; RESTART</button></form>
<form method='POST' action='/cancel'><button class='c' type='submit'>CANCEL (keep the old setting)</button></form>
<script>function f(x){document.getElementById('fx').style.display=x?'block':'none';}</script>
</body></html>)HTML";

static void portalSend(AsyncWebServerRequest* req){
	String page;
	page.reserve(3500 + portalNetOptions.length());
	page += FPSTR(PORTAL_PAGE_1);
	page += portalNetOptions;
	page += FPSTR(PORTAL_PAGE_2);
	req->send(200, "text/html", page);
}

static String argTrim(AsyncWebServerRequest* req, const char* name){
	String v = req->hasArg(name) ? req->arg(name) : String();
	v.trim();
	return v;
}

static void portalSave(AsyncWebServerRequest* req){
	WifiCfg c;
	c.ssid = argTrim(req, "ssid2");
	if (c.ssid.length() == 0) c.ssid = argTrim(req, "ssid");
	c.pass = req->hasArg("pass") ? req->arg("pass") : String();
	c.dhcp = argTrim(req, "mode") != "fixed";
	String err;
	if (c.ssid.length() == 0) err = "Choose or type a WiFi network.";
	if (!c.dhcp && err.length() == 0){
		String dns = argTrim(req, "dns");
		if (!c.ip.fromString(argTrim(req, "ip")) || !c.gw.fromString(argTrim(req, "gw")) ||
		    !c.sn.fromString(argTrim(req, "sn")))  err = "Fixed IP, gateway and subnet mask must be like 192.168.1.124.";
		else if (dns.length() == 0)               c.dns = c.gw;
		else if (!c.dns.fromString(dns))           err = "DNS must be like 192.168.1.1 (or leave it empty).";
	}
	if (err.length()){
		req->send(200, "text/html", "<meta name='viewport' content='width=device-width,initial-scale=1'>"
		          "<p style='font-family:sans-serif;color:#b91c1c'>" + err + "</p><p><a href='/'>Back</a></p>");
		return;
	}
	pendingCfg  = c;
	pendingSave = true;
	req->send(200, "text/html", "<meta name='viewport' content='width=device-width,initial-scale=1'>"
	          "<div style='font-family:sans-serif;text-align:center;padding:20px'><h3>Saved</h3>"
	          "<p>The rain gauge restarts and joins <b>" + c.ssid + "</b>.</p>"
	          "<p>Connect your phone back to your normal WiFi and open<br><b>" +
	          (c.dhcp ? String("http://raingauge.local/") : "http://" + c.ip.toString() + "/") + "</b></p>"
	          "<p style='color:#64748b;font-size:13px'>If it cannot join, the RainGauge-Setup hotspot comes back "
	          "after a few minutes so you can try again.</p></div>");
}

static void runSetupPortal(uint32_t timeoutMs, const String& why){
	SaveGenReport("WiFi: setup hotspot " WIFI_SETUP_SSID " started (" + why + ")");
	WiFi.disconnect(true);
	delay(200);
	WiFi.mode(WIFI_AP_STA);

	// Networks in range, strongest first, each SSID once.
	int n = WiFi.scanNetworks();
	{   // strongest first, each SSID once (n is small: simple selection sort)
		portalNetOptions = "";
		bool used[64] = { false };
		int m = n < 64 ? n : 64;
		for (int k = 0; k < m; k++){
			int best = -1;
			for (int j = 0; j < m; j++)
				if (!used[j] && (best < 0 || WiFi.RSSI(j) > WiFi.RSSI(best))) best = j;
			if (best < 0) break;
			used[best] = true;
			String s = WiFi.SSID(best);
			if (s.length() == 0 || s.indexOf('\'') >= 0 || s.indexOf('<') >= 0) continue;
			if (portalNetOptions.indexOf("value='" + s + "'") >= 0) continue;
			portalNetOptions += "<option value='" + s + "'>" + s + "  (" + String(WiFi.RSSI(best)) + " dBm)</option>";
		}
	}
	WiFi.scanDelete();

	WiFi.softAP(WIFI_SETUP_SSID);
	IPAddress apIP = WiFi.softAPIP();
	DNSServer dns;
	dns.start(53, "*", apIP);     // every name -> the gauge: phones show the page by themselves

	server.on("/",       HTTP_GET,  portalSend);
	server.on("/save",   HTTP_POST, portalSave);
	server.on("/cancel", HTTP_POST, [](AsyncWebServerRequest* r){
		pendingCancel = true;
		r->send(200, "text/html", "<p style='font-family:sans-serif'>Cancelled - the rain gauge restarts with its old setting.</p>");
	});
	server.onNotFound([apIP](AsyncWebServerRequest* r){ r->redirect("http://" + apIP.toString() + "/"); });
	server.begin();
	DEBUG_PRINTLN("WiFi: setup hotspot " WIFI_SETUP_SSID " at " + apIP.toString());

	uint32_t start = millis(), blink = 0;
	for (;;){
		dns.processNextRequest();
		if (pendingSave){
			delay(1500);                  // let the "Saved" page reach the phone
			saveCfg(pendingCfg);
			SaveGenReport("WiFi: setup saved - network " + pendingCfg.ssid +
			              (pendingCfg.dhcp ? String(" (DHCP)") : " on " + pendingCfg.ip.toString()));
			ESP.restart();
		}
		if (pendingCancel){ delay(1000); ESP.restart(); }
		if (timeoutMs && millis() - start > timeoutMs){
			SaveGenReport F("WiFi: setup hotspot timed out - restarting to try the saved network again");
			ESP.restart();
		}
		if (millis() - blink > 150){ blink = millis(); HeartbeatToggle(); }   // fast blink = setup mode
		delay(5);
	}
}

// ── Public API ────────────────────────────────────────────────────────────────

String WiFiHostName(){
	return WIFI_HOSTNAME;
}

bool SetupWiFi() {
	WiFi.persistent(false);          // our own file, not the IDF NVS copy
	WiFi.setHostname(WIFI_HOSTNAME);
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(false);

	// ABOUT "WIFI SETUP" button: restart straight into the setup hotspot.
	if (SPIFFS.exists(StrWiFiSetupFlag)){
		SPIFFS.remove(StrWiFiSetupFlag);
		runSetupPortal(WIFI_PORTAL_MS, "requested on the ABOUT page");
	}

	// Brand-new gauge (public firmware, nothing saved): nothing to try — set up.
	if (!SPIFFS.exists(StrWiFiConfig) && KNOWN_NET_COUNT == 0)
		runSetupPortal(0, "no WiFi network saved yet");

	for (int round = 0; round < WIFI_ROUNDS; round++) {
		if (round > 0) {
			DEBUG_PRINTF("WiFi: round %d failed - retry in %lu s\n", round, WIFI_ROUND_GAP_MS / 1000);
			delay(WIFI_ROUND_GAP_MS);
		}
		bool joined = joinSaved();
		if (!joined) joined = scanAndJoinKnown();

		if (joined) {
			Heartbeat(false);
			DEBUG_PRINT("WiFi: connected to "); DEBUG_PRINT(WiFi.SSID());
			DEBUG_PRINT(" ("); DEBUG_PRINT(WiFiSource); DEBUG_PRINT("), IP = ");
			DEBUG_PRINTLN(WiFi.localIP().toString());
			if (MDNS.begin(WIFI_HOSTNAME)) {
				MDNS.addService("http", "tcp", 80);
				DEBUG_PRINTLN("WiFi: mDNS started as " WIFI_HOSTNAME ".local");
			}
			return true;
		}
	}
	// Nothing joined for several minutes: open the setup hotspot for a while
	// (maybe the password changed), then reboot and try again.
	runSetupPortal(WIFI_PORTAL_MS, "saved network not reachable");
	return false;   // not reached — the portal always reboots
}

// ── ABOUT "WIFI SETUP" button ────────────────────────────────────────────────
// POST /wifisetup (async_tcp) only raises the flag; MaintainWifiSetupRequest()
// (loop) writes /WiFiSetup.flag and restarts into the setup hotspot.

static volatile bool wifiSetupRequested = false;

void handleWifiSetupRequest(AsyncWebServerRequest *request){
	wifiSetupRequested = true;
	request->send(200, "text/plain", "OK");
}

void MaintainWifiSetupRequest(){
	if (!wifiSetupRequested) return;
	wifiSetupRequested = false;
	File f = SPIFFS.open(StrWiFiSetupFlag, "w");
	if (f){ f.print("1"); f.close(); }
	SaveGenReport F("WiFi: restarting into the setup hotspot (ABOUT page)");
	delay(300);
	ESP.restart();
}
