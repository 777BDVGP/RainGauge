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
#define WIFI_SAVED_TRIES     2                  // attempts per round when only ONE network is saved
#define WIFI_MAX_NETS        8                  // networks kept in /WiFiConfig.json
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
	// Several access points may share one SSID (mesh / multi-AP). The ESP32's
	// default fast scan joins the FIRST one found on the channel sweep; scan
	// all channels and pick the strongest instead.
	WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
	WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
	WiFi.begin(c.ssid.c_str(), c.pass.c_str());
	if (!waitConnected()) return false;
	SaveGenReport("WiFi: joined " + c.ssid + " via AP " + WiFi.BSSIDstr() +
	              " (channel " + String(WiFi.channel()) + ", " + String(WiFi.RSSI()) + " dBm)");
	return true;
}

// ── /WiFiConfig.json — a LIST of networks ────────────────────────────────────
// { "nets": [ { ssid, pass, dhcp, ip, gw, sn, dns }, ... ] }   newest first.
// The 1.8.x file held one network as a plain object — still read, and
// written back as a list the next time anything is saved.

static void netFromJson(JsonVariantConst o, WifiCfg& c){
	c.ssid = o["ssid"] | "";
	c.pass = o["pass"] | "";
	c.dhcp = o["dhcp"] | true;
	if (!c.dhcp){
		if (!c.ip.fromString(o["ip"] | "")){ c.dhcp = true; return; }   // no usable fixed IP: DHCP
		// gw / sn / dns may be missing = 0.0.0.0 = learned from the router at the
		// first join (needsLearning) and then saved.
		c.gw.fromString(o["gw"] | "");
		c.sn.fromString(o["sn"] | "");
		c.dns.fromString(o["dns"] | "");
	}
}

// Fixed IP without a known gateway: learn gateway / subnet / DNS by DHCP first.
static bool needsLearning(const WifiCfg& c){
	return !c.dhcp && ((uint32_t)c.gw == 0 || (uint32_t)c.sn == 0);
}

// Fills out[] (max maxN), returns how many networks are saved.
static int loadNets(WifiCfg* out, int maxN){
	File fr = SPIFFS.open(StrWiFiConfig, "r");
	if (!fr) return 0;
	DynamicJsonDocument doc(4096);
	DeserializationError err = deserializeJson(doc, fr);
	fr.close();
	if (err) return 0;
	int n = 0;
	if (doc["nets"].is<JsonArrayConst>()){
		for (JsonVariantConst o : doc["nets"].as<JsonArrayConst>()){
			if (n >= maxN) break;
			netFromJson(o, out[n]);
			if (out[n].ssid.length()) n++;
		}
	} else if (doc["ssid"].is<const char*>()){          // old single-network file
		netFromJson(doc.as<JsonVariantConst>(), out[0]);
		if (out[0].ssid.length()) n = 1;
	}
	return n;
}

static void saveNets(const WifiCfg* nets, int n){
	DynamicJsonDocument doc(4096);
	JsonArray a = doc.createNestedArray("nets");
	for (int i = 0; i < n; i++){
		JsonObject o = a.createNestedObject();
		o["ssid"] = nets[i].ssid;
		o["pass"] = nets[i].pass;
		o["dhcp"] = nets[i].dhcp;
		if (!nets[i].dhcp){
			o["ip"] = nets[i].ip.toString();
			if ((uint32_t)nets[i].gw)  o["gw"]  = nets[i].gw.toString();    // not yet learned = left out
			if ((uint32_t)nets[i].sn)  o["sn"]  = nets[i].sn.toString();
			if ((uint32_t)nets[i].dns) o["dns"] = nets[i].dns.toString();
		}
	}
	File fw = SPIFFS.open(StrWiFiConfig, "w");
	if (!fw) { SaveGenReport F("WiFi: could not write /WiFiConfig.json"); return; }
	serializeJson(doc, fw);
	fw.close();
	SPIFFS.remove(StrWiFiDefault);   // the old (pre-1.8) file is not used any more
}

// Add a network (newest first).  The same SSID again replaces its entry;
// with a full list the oldest entry drops off.
static void addNet(const WifiCfg& c){
	WifiCfg nets[WIFI_MAX_NETS + 1];
	int n = loadNets(nets + 1, WIFI_MAX_NETS);
	nets[0] = c;
	int k = 1;
	for (int i = 1; i <= n; i++)
		if (nets[i].ssid != c.ssid && k < WIFI_MAX_NETS) nets[k++] = nets[i];
	saveNets(nets, k);
}

// Remove a network by SSID.  Returns false when it was not in the list.
static bool deleteNet(const String& ssid){
	WifiCfg nets[WIFI_MAX_NETS];
	int n = loadNets(nets, WIFI_MAX_NETS), k = 0;
	for (int i = 0; i < n; i++) if (nets[i].ssid != ssid) nets[k++] = nets[i];
	if (k == n) return false;
	saveNets(nets, k);
	return true;
}

// Write EVERY built-in network (KNOWN_NETS, private firmware) into
// /WiFiConfig.json, not only the one joined: after "INSTALL FROM GITHUB" the
// gauge runs the public firmware, which has no built-in networks, and then
// knows only this file — so a gauge set up at one place must still find the
// other places' networks.  The gateway is left out and learned from the
// router at the first join there (needsLearning).  An entry already in the
// file (same SSID) is kept as it is.  Same as Hydro17_WaterSystem 2.0.2.
static void seedKnownNets(){
	if (KNOWN_NET_COUNT == 0) return;
	WifiCfg nets[WIFI_MAX_NETS];
	int n = loadNets(nets, WIFI_MAX_NETS), before = n;
	for (int i = 0; i < KNOWN_NET_COUNT; i++){
		bool have = false;
		for (int j = 0; j < n; j++) if (nets[j].ssid == KNOWN_NETS[i].ssid) have = true;
		if (have || n >= WIFI_MAX_NETS) continue;
		WifiCfg c;
		c.ssid = KNOWN_NETS[i].ssid; c.pass = KNOWN_NETS[i].pass; c.dhcp = false;
		c.ip   = IPAddress(KNOWN_NETS[i].ip[0], KNOWN_NETS[i].ip[1], KNOWN_NETS[i].ip[2], KNOWN_NETS[i].ip[3]);
		nets[n++] = c;                     // appended: existing entries keep their order
		SaveGenReport("WiFi: built-in network " + c.ssid + " (" + c.ip.toString() + ") added to the saved networks");
	}
	if (n != before) saveNets(nets, n);
}

// ── Step 1: saved networks ───────────────────────────────────────────────────
// Scan, then try the saved networks that are in range, strongest first, and
// after them the ones not seen (hidden SSIDs, weak signal).  With a single
// saved network it gets WIFI_SAVED_TRIES attempts, otherwise one each.

static bool joinSaved(){
	WifiCfg nets[WIFI_MAX_NETS];
	int n = loadNets(nets, WIFI_MAX_NETS);
	if (n == 0) return false;

	int rssi[WIFI_MAX_NETS];
	for (int i = 0; i < n; i++) rssi[i] = INT_MIN;
	int found = WiFi.scanNetworks();
	for (int j = 0; j < found; j++)
		for (int i = 0; i < n; i++)
			if (WiFi.SSID(j) == nets[i].ssid && WiFi.RSSI(j) > rssi[i]) rssi[i] = WiFi.RSSI(j);
	WiFi.scanDelete();

	bool tried[WIFI_MAX_NETS] = { false };
	int tries = (n == 1) ? WIFI_SAVED_TRIES : 1;
	for (int k = 0; k < n; k++){
		int best = -1;                       // in range first (strongest), then the rest in list order
		for (int i = 0; i < n; i++)
			if (!tried[i] && (best < 0 || rssi[i] > rssi[best])) best = i;
		tried[best] = true;
		WifiCfg& c = nets[best];
		for (int t = 0; t < tries; t++){
			bool learned = false;
			if (needsLearning(c)){
				// Join by DHCP once to learn the router's gateway / subnet / DNS.
				WifiCfg d = c; d.dhcp = true;
				if (!joinWith(d)){ dropConnection(); continue; }
				c.gw = WiFi.gatewayIP();
				c.sn = WiFi.subnetMask();
				if ((uint32_t)c.dns == 0) c.dns = WiFi.dnsIP();
				dropConnection();
				learned = true;
			}
			if (joinWith(c)){
				WiFiSource = "saved";
				if (learned){
					WifiCfg all[WIFI_MAX_NETS];          // store what was learned, same place in the list
					int m = loadNets(all, WIFI_MAX_NETS);
					for (int i = 0; i < m; i++) if (all[i].ssid == c.ssid) all[i] = c;
					saveNets(all, m);
					SaveGenReport("WiFi: " + c.ssid + " on " + c.ip.toString() + " - gateway " + c.gw.toString() +
					              " learned from the router and saved");
				}
				return true;
			}
			dropConnection();
		}
	}
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
			addNet(c);
			WiFiSource = "known network";
			SaveGenReport("WiFi: joined " + c.ssid + " on " + c.ip.toString() + " - added to the saved networks");
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
<label>Gateway / router (empty = learned from the router)</label><input name='gw'>
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

// Network from a form (setup hotspot page and SETTINGS "ADD NETWORK"):
// ssid (or ssid2 typed by hand), pass, mode = dhcp / fixed, ip, gw, sn, dns.
// Returns "" when fine, otherwise the reason.
static String parseNetForm(AsyncWebServerRequest* req, WifiCfg& c){
	c.ssid = argTrim(req, "ssid2");
	if (c.ssid.length() == 0) c.ssid = argTrim(req, "ssid");
	c.pass = req->hasArg("pass") ? req->arg("pass") : String();
	c.dhcp = argTrim(req, "mode") != "fixed";
	if (c.ssid.length() == 0 || c.ssid.length() > 32) return "Choose or type a WiFi network name (max 32 characters).";
	if (c.pass.length() > 63)                         return "The WiFi password can be at most 63 characters.";
	if (!c.dhcp){
		String gw = argTrim(req, "gw"), sn = argTrim(req, "sn"), dns = argTrim(req, "dns");
		if (!c.ip.fromString(argTrim(req, "ip"))) return "The fixed IP must be like 192.168.1.124.";
		if (gw.length() == 0){
			// No gateway given: learned from the router at the first join (with subnet and DNS).
			c.gw = c.sn = c.dns = IPAddress();
			return "";
		}
		if (!c.gw.fromString(gw))                 return "The gateway must be like 192.168.1.1 (or leave it empty: learned from the router).";
		if (sn.length() == 0)                     c.sn = IPAddress(255, 255, 255, 0);
		else if (!c.sn.fromString(sn))            return "The subnet mask must be like 255.255.255.0.";
		if (dns.length() == 0)                    c.dns = c.gw;
		else if (!c.dns.fromString(dns))          return "DNS must be like 192.168.1.1 (or leave it empty).";
	}
	return "";
}

static void portalSave(AsyncWebServerRequest* req){
	WifiCfg c;
	String err = parseNetForm(req, c);
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
			addNet(pendingCfg);
			SaveGenReport("WiFi: setup added network " + pendingCfg.ssid +
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
	seedKnownNets();                 // all built-in networks into the file (survive a public update)

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

// =============================================================================
// SETTINGS page — "WiFi Networks": list (never the passwords), DELETE, ADD
// =============================================================================
// The page is built in async_tcp and only READS the file.  ADD / DELETE
// (POST /wifinet/add, /wifinet/delete) copy the request into one pending slot
// and raise a flag; MaintainWifiNets() (loop) writes the file.  Changes are
// used at the next start (or when the current network is lost).

static WifiCfg       wnPendingAdd;
static String        wnPendingDel;
static volatile bool wnAddReq = false, wnDelReq = false;

static String htmlEsc(const String& s){
	String o; o.reserve(s.length() + 8);
	for (size_t i = 0; i < s.length(); i++){
		char c = s[i];
		if      (c == '&')  o += "&amp;";
		else if (c == '<')  o += "&lt;";
		else if (c == '>')  o += "&gt;";
		else if (c == '"')  o += "&quot;";
		else if (c == 39)   o += "&#39;";
		else                o += c;
	}
	return o;
}

static const char WN_SECTION_TOP[] PROGMEM = R"HTML(
<div id='wifinets' style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:16px 0 4px'>WIFI NETWORKS</div>
<p style='color:#475569;font-size:12px;padding:0 8px 4px'>At start-up the gauge tries these networks, the strongest one in range first.
If none of them can be reached for about 5 minutes, it opens the hotspot <b>RainGauge-Setup</b>: connect a phone to it
and add a network there. Changes are used at the next start. Passwords are never shown.</p>
<table>)HTML";

static const char WN_SECTION_ADD[] PROGMEM = R"HTML(</table>
<div class='noupd' style='margin:8px 0;padding:8px;border:1px solid #cbd5e1;border-radius:6px;font-size:13px'>
<b>Add a network</b> (the same name again replaces that entry)<br>
<select id='wn_scan' onchange="if(this.value)document.getElementById('wn_ssid').value=this.value" style='width:300px;margin:4px 0'>
<option value=''>Networks in range: scanning...</option></select>
<button class='btn btn-bl' style='padding:3px 10px;font-size:11px' onclick='wnScan()'>SCAN AGAIN</button><br>
<input id='wn_ssid' placeholder='WiFi network name' style='width:200px;margin:4px 0'>
<input id='wn_pass' type='password' placeholder='password (empty = open network)' style='width:200px;margin:4px 0'><br>
<label><input type='radio' name='wn_mode' id='wn_dhcp' checked onclick="wnFix(0)"> Automatic IP (DHCP)</label>
<label><input type='radio' name='wn_mode' id='wn_fixed' onclick="wnFix(1)"> Fixed IP</label>
<div id='wn_fx' style='display:none'>
<input id='wn_ip' placeholder='fixed IP, e.g. 192.168.1.124' style='width:200px;margin:2px 0'>
<input id='wn_gw' placeholder='gateway (empty = learned from the router)' style='width:200px;margin:2px 0'>
<input id='wn_sn' value='255.255.255.0' style='width:200px;margin:2px 0'>
<input id='wn_dns' placeholder='DNS (empty = gateway)' style='width:200px;margin:2px 0'></div>
<div class='btns'><button class='btn btn-gr' onclick='wnAdd()'>ADD NETWORK</button></div>
<span id='wn_msg' style='font-size:12px'></span></div>
<script>
function wnFix(x){document.getElementById('wn_fx').style.display=x?'block':'none';}
function wnPost(u,b){var m=document.getElementById('wn_msg');
 fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
 .then(function(r){return r.text();}).then(function(t){m.textContent=t;if(t.indexOf('OK')==0)setTimeout(function(){location.reload();},1200);})
 .catch(function(){m.textContent='No answer from the gauge';});}
function wnAdd(){var v=function(i){return encodeURIComponent(document.getElementById(i).value.trim());};
 var b='ssid='+v('wn_ssid')+'&pass='+encodeURIComponent(document.getElementById('wn_pass').value)+
 '&mode='+(document.getElementById('wn_fixed').checked?'fixed':'dhcp')+'&ip='+v('wn_ip')+'&gw='+v('wn_gw')+'&sn='+v('wn_sn')+'&dns='+v('wn_dns');
 wnPost('/wifinet/add',b);}
function wnDel(s,cur){if(!confirm('Delete the WiFi network '+s+'?'+(cur?' The gauge is connected to it now: it stays connected until the next start, then tries the other networks.':'')))return;
 wnPost('/wifinet/delete','ssid='+encodeURIComponent(s));}
var wnTries=0;
function wnScanShow(j){var sel=document.getElementById('wn_scan');sel.innerHTML='';
 var o=document.createElement('option');o.value='';
 o.textContent=j.nets.length?('Networks in range ('+j.nets.length+') - pick one:'):'No networks in range - SCAN AGAIN';sel.appendChild(o);
 j.nets.forEach(function(n){var e=document.createElement('option');e.value=n.s;
  e.textContent=n.s+'   ('+n.r+' dBm'+(n.l?', locked':', open')+(n.v?', saved':'')+')';sel.appendChild(e);});}
function wnPoll(){fetch('/wifiscan').then(function(r){return r.json();}).then(function(j){
 if(j.status=='done'){wnScanShow(j);return;}
 if(++wnTries<20)setTimeout(wnPoll,1000);else document.getElementById('wn_scan').options[0].textContent='Scan failed - SCAN AGAIN';})
 .catch(function(){if(++wnTries<20)setTimeout(wnPoll,1500);});}
function wnScan(){wnTries=0;document.getElementById('wn_scan').innerHTML="<option value=''>Networks in range: scanning...</option>";
 fetch('/wifiscan?start=1').then(function(){setTimeout(wnPoll,2500);}).catch(function(){setTimeout(wnPoll,2500);});}
wnScan();
</script>
)HTML";

// Appends the section to the page being built (SETTINGS, async_tcp — read only).
void appendWifiNetsSection(){
	WifiCfg nets[WIFI_MAX_NETS];
	int n = loadNets(nets, WIFI_MAX_NETS);
	appendHtmlStr(WN_SECTION_TOP);
	String cur = WiFi.SSID();
	if (n == 0) appendHtmlStr("<tr><td colspan='3'>(no networks saved)</td></tr>");
	for (int i = 0; i < n; i++){
		String s   = htmlEsc(nets[i].ssid);
		bool   on  = (nets[i].ssid == cur);
		// Own widths: the page CSS gives a row's first cell width:100%, which
		// squeezed the middle column to one letter per line.
		String row = "<tr><td style='width:40%'><b>" + s + "</b>" + (on ? String(" <span style='color:#16a34a;font-size:11px'>(connected)</span>") : String("")) +
		             "</td><td style='font-size:12px;width:45%;text-align:left'>" + (nets[i].dhcp ? String("automatic IP") : "fixed " + nets[i].ip.toString() +
		              ((uint32_t)nets[i].gw ? String("") : String(" (gateway learned at the first start)"))) +
		             "</td><td style='width:15%;white-space:nowrap'><button class='btn btn-rd' style='padding:2px 8px;font-size:11px' data-s='" + s + "' onclick='wnDel(this.dataset.s," +
		             (on ? "1" : "0") + ")'>DELETE</button></td></tr>";
		appendHtmlStr(row.c_str());
	}
	appendHtmlStr(WN_SECTION_ADD);
}

void handleWifiNetAdd(AsyncWebServerRequest *request){
	if (wnAddReq || wnDelReq){ request->send(200, "text/plain", "Busy - try again"); return; }
	WifiCfg c;
	String err = parseNetForm(request, c);
	if (err.length()){ request->send(200, "text/plain", err); return; }
	wnPendingAdd = c;
	wnAddReq = true;
	request->send(200, "text/plain", "OK - " + c.ssid + " added");
}

void handleWifiNetDelete(AsyncWebServerRequest *request){
	if (wnAddReq || wnDelReq){ request->send(200, "text/plain", "Busy - try again"); return; }
	String s = argTrim(request, "ssid");
	if (s.length() == 0){ request->send(200, "text/plain", "No network given"); return; }
	wnPendingDel = s;
	wnDelReq = true;
	request->send(200, "text/plain", "OK - " + s + " deleted");
}

// ── Networks in range (SETTINGS "Add a network" drop-down) ───────────────────
// GET /wifiscan?start=1 asks for a scan; GET /wifiscan returns
// {"status":"scanning"} or {"status":"done","nets":[{s,r,l,v},...]}
// (SSID, RSSI dBm, locked, already saved) strongest first, each SSID once.
// The scan is started and collected in loop() (MaintainWifiNets) with the
// non-blocking WiFi.scanNetworks(true) — the gauge keeps counting tips and
// stays connected; async_tcp only reads the finished result.
// Same code as Hydro17_WaterSystem (2026-11-24).
static volatile int  wnScanState = 0;      // 0 idle, 1 requested, 2 scanning, 3 done
static String        wnScanJson;           // written by loop() before state 3

void handleWifiScan(AsyncWebServerRequest *request){
	if (request->hasArg("start")){
		if (wnScanState != 1 && wnScanState != 2) wnScanState = 1;
		request->send(200, "application/json", "{\"status\":\"scanning\"}");
		return;
	}
	if (wnScanState == 3) request->send(200, "application/json", wnScanJson);
	else                  request->send(200, "application/json", "{\"status\":\"scanning\"}");
}

static void wifiScanStep(){
	static uint32_t startMs = 0;
	if (wnScanState == 1){
		WiFi.scanDelete();
		if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED){        // true = async, returns at once
			wnScanJson = "{\"status\":\"done\",\"nets\":[]}";
			wnScanState = 3;
			return;
		}
		startMs = millis();
		wnScanState = 2;
		return;
	}
	if (wnScanState != 2) return;
	int n = WiFi.scanComplete();
	if (n == WIFI_SCAN_RUNNING){
		if (millis() - startMs < 15000) return;
		n = 0;                                                    // took too long: report nothing
	}
	if (n < 0) n = 0;

	WifiCfg saved[WIFI_MAX_NETS];
	int ns = loadNets(saved, WIFI_MAX_NETS);
	DynamicJsonDocument doc(3072);
	doc["status"] = "done";
	JsonArray a = doc.createNestedArray("nets");
	bool used[64] = { false };
	int m = n < 64 ? n : 64;
	for (int k = 0; k < m; k++){                                  // strongest first, each SSID once
		int best = -1;
		for (int j = 0; j < m; j++)
			if (!used[j] && (best < 0 || WiFi.RSSI(j) > WiFi.RSSI(best))) best = j;
		if (best < 0) break;
		used[best] = true;
		String s = WiFi.SSID(best);
		if (s.length() == 0) continue;                            // hidden network
		bool dup = false;
		for (JsonObject o : a) if (s == o["s"].as<const char*>()) { dup = true; break; }
		if (dup) continue;
		bool isSaved = false;
		for (int i = 0; i < ns; i++) if (saved[i].ssid == s) isSaved = true;
		JsonObject o = a.createNestedObject();
		o["s"] = s;
		o["r"] = WiFi.RSSI(best);
		o["l"] = WiFi.encryptionType(best) != WIFI_AUTH_OPEN;
		o["v"] = isSaved;
	}
	WiFi.scanDelete();
	String out;
	serializeJson(doc, out);
	wnScanJson  = out;
	wnScanState = 3;
}

// Called every 10 ms from loop().
void MaintainWifiNets(){
	wifiScanStep();
	if (wnAddReq){
		addNet(wnPendingAdd);
		SaveGenReport("WiFi: network " + wnPendingAdd.ssid + " added on SETTINGS" +
		              (wnPendingAdd.dhcp ? String(" (automatic IP)") : " (fixed " + wnPendingAdd.ip.toString() + ")"));
		wnPendingAdd = WifiCfg();
		wnAddReq = false;
	}
	if (wnDelReq){
		if (deleteNet(wnPendingDel)) SaveGenReport("WiFi: network " + wnPendingDel + " deleted on SETTINGS");
		wnDelReq = false;
	}
}
