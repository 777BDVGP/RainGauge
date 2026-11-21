// =============================================================================
// 22_Update.cpp — Firmware update from GitHub or from an uploaded .bin
// =============================================================================
// See 22_Update.h.
// =============================================================================

#include <Arduino.h>
#include <Update.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "mbedtls/platform.h"
#include "esp_heap_caps.h"
#include "all_includes.h"
#include "22_RootCerts.h"

// Kept in the binary (used below), so every rain gauge .bin contains it.
static const char fwMarker[] = FW_MARKER;

// ─── Image checks, shared by both ways ───────────────────────────────────────
// Streaming search for fwMarker ('#' occurs once in it, so a simple reset on
// mismatch is enough) + the ESP image header: magic 0xE9, chip id 2 = ESP32-S2.

struct ImageCheck {
	size_t  pos = 0;          // bytes seen
	uint8_t hdr[16];
	size_t  match = 0;
	bool    marker = false;
	void feed(const uint8_t* d, size_t n){
		for (size_t i = 0; i < n; i++){
			if (pos < sizeof(hdr)) hdr[pos] = d[i];
			pos++;
			if (marker) continue;
			char c = (char)d[i];
			if (c == fwMarker[match]){ if (++match == sizeof(fwMarker) - 1) marker = true; }
			else match = (c == fwMarker[0]) ? 1 : 0;
		}
	}
	// "" = fine so far
	String headerProblem() const {
		if (pos < sizeof(hdr)) return "";
		if (hdr[0] != 0xE9) return "not an ESP32 firmware file";
		uint16_t chip = hdr[12] | (hdr[13] << 8);
		if (chip != 2) return "firmware for another chip (not ESP32-S2)";
		return "";
	}
};

static String fwStatusLine(const String& s){
	String when = RainTimeValid() ? GetCurrentTime() + " - " : String("");
	return when + s;
}

// Push FwStatus / FwLatest to the ABOUT page — also from inside the blocking
// download loop, so the browser sees the progress.
static void pushFw(){
	webParamUpdate(FwStatus, "FWSTAT",   "");
	webParamUpdate(FwLatest, "FWLATEST", "");
}

void SendFwValues(){
	static String lastS, lastL;
	if (FwStatus != lastS || FwLatest != lastL){ pushFw(); lastS = FwStatus; lastL = FwLatest; }
}

// Dotted version compare: "1.10.0" > "1.9.3".  >0 if a is newer than b.
static int versionCompare(const String& a, const String& b){
	int ia = 0, ib = 0;
	for (int k = 0; k < 4; k++){
		int na = a.substring(ia).toInt(), nb = b.substring(ib).toInt();
		if (na != nb) return na > nb ? 1 : -1;
		int da = a.indexOf('.', ia), db = b.indexOf('.', ib);
		if (da < 0 && db < 0) return 0;
		ia = da < 0 ? a.length() : da + 1;
		ib = db < 0 ? b.length() : db + 1;
	}
	return 0;
}

// ─── HTTPS with TLS buffers in PSRAM ─────────────────────────────────────────
static void* psCalloc(size_t n, size_t size){
	void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	return p ? p : heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
static void psFree(void* p){ heap_caps_free(p); }
// Same as the build default (CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC): internal RAM.
static void* inCalloc(size_t n, size_t size){ return heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
static void tlsToPsram(bool on){
	if (on) mbedtls_platform_set_calloc_free(psCalloc, psFree);
	else    mbedtls_platform_set_calloc_free(inCalloc, psFree);
}

// GET url over verified HTTPS.  Returns HTTP code (<0 = connection error);
// http / client stay open for the caller to read.
static int httpsGet(HTTPClient& http, NetworkClientSecure& client, const String& url){
	client.setCACert(GH_ROOT_CERTS);
	client.setTimeout(15);
	http.setConnectTimeout(15000);
	http.setTimeout(15000);
	http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
	http.setUserAgent("RainGauge/" FW_VERSION);
	if (!http.begin(client, url)) return -1;
	return http.GET();
}

// ─── CHECK FOR UPDATE ────────────────────────────────────────────────────────
static void doCheck(){
	if (!RainTimeValid()){ FwStatus = fwStatusLine("Clock not set yet (NTP) - try again in a minute"); return; }
	FwStatus = fwStatusLine("Checking GitHub..."); pushFw();
	watchdogCount = 0;
	tlsToPsram(true);
	String body; int code;
	{
		NetworkClientSecure client;
		HTTPClient http;
		code = httpsGet(http, client, String(FW_UPDATE_URL) + "version.json");
		if (code == 200) body = http.getString();
		http.end();
	}
	tlsToPsram(false);
	watchdogCount = 0;
	if (code != 200){
		FwStatus = fwStatusLine("Check failed (" + (code < 0 ? HTTPClient::errorToString(code) : "HTTP " + String(code)) + ")");
		return;
	}
	StaticJsonDocument<512> doc;
	if (deserializeJson(doc, body)){ FwStatus = fwStatusLine("Check failed (version.json not readable)"); return; }
	FwLatest  = doc["version"] | "-";
	FwBinPath = doc["bin"]     | "firmware/raingauge.bin";
	FwNotes   = doc["notes"]   | "";
	int cmp = versionCompare(FwLatest, FW_VERSION);
	if (cmp > 0)       FwStatus = fwStatusLine("NEW version " + FwLatest + " available - press INSTALL" + (FwNotes.length() ? " (" + FwNotes + ")" : String("")));
	else if (cmp == 0) FwStatus = fwStatusLine("Up to date (" + FwLatest + ")");
	else               FwStatus = fwStatusLine("This gauge (" FW_VERSION ") is newer than GitHub (" + FwLatest + ")");
}

// ─── INSTALL from GitHub ─────────────────────────────────────────────────────
static void doInstall(){
	if (FwBinPath.length() == 0){ FwStatus = fwStatusLine("Press CHECK FOR UPDATE first"); return; }
	if (!RainTimeValid())       { FwStatus = fwStatusLine("Clock not set yet (NTP) - try again in a minute"); return; }
	FwBusy = true;
	SaveGenReport("Firmware update from GitHub: " FW_VERSION " -> " + FwLatest + " started");
	FwStatus = fwStatusLine("Downloading " + FwLatest + "..."); pushFw();
	watchdogCount = 0;
	tlsToPsram(true);

	String err;
	bool   ok = false;
	{
		NetworkClientSecure client;
		HTTPClient http;
		int code = httpsGet(http, client, String(FW_UPDATE_URL) + FwBinPath);
		int total = http.getSize();
		if (code != 200) err = "download failed (" + (code < 0 ? HTTPClient::errorToString(code) : "HTTP " + String(code)) + ")";
		else if (total <= 0) err = "download failed (unknown size)";
		else if (!Update.begin(total, U_FLASH)) err = "no room for the new firmware (" + String(Update.errorString()) + ")";
		else {
			WiFiClient* s = http.getStreamPtr();
			ImageCheck chk;
			static uint8_t buf[2048];
			int done = 0, lastPct = -1;
			uint32_t lastData = millis();
			while (done < total){
				watchdogCount = 0;
				size_t avail = s->available();
				if (avail == 0){
					if (!http.connected() || millis() - lastData > 20000){ err = "download interrupted"; break; }
					delay(2); continue;
				}
				int n = s->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
				if (n <= 0) continue;
				lastData = millis();
				chk.feed(buf, n);
				String hp = chk.headerProblem();
				if (hp.length()){ err = hp; break; }
				if (Update.write(buf, n) != (size_t)n){ err = "flash write failed (" + String(Update.errorString()) + ")"; break; }
				done += n;
				int pct = (int)((int64_t)done * 100 / total);
				if (pct / 5 != lastPct / 5){ lastPct = pct; FwStatus = fwStatusLine("Downloading " + FwLatest + ": " + String(pct) + "%"); pushFw(); }
			}
			if (err.length() == 0 && !chk.marker) err = "file is not rain gauge firmware";
			if (err.length() == 0 && !Update.end(true)) err = "image check failed (" + String(Update.errorString()) + ")";
			if (err.length()) Update.abort(); else ok = true;
		}
		http.end();
	}
	tlsToPsram(false);
	FwBusy = false;
	if (ok){
		SaveGenReport("Firmware update from GitHub: " + FwLatest + " installed - restarting");
		FwStatus = fwStatusLine("Installed " + FwLatest + " - restarting..."); pushFw();
		delay(1500);
		ESP.restart();
	}
	FwStatus = fwStatusLine("Update FAILED: " + err + " - the old firmware keeps running");
	SaveGenReport("Firmware update from GitHub FAILED: " + err);
}

// ─── Requests from the ABOUT page (async_tcp -> loop) ────────────────────────
static volatile uint8_t fwRequest = 0;   // 1 = check, 2 = install
static volatile bool    fwRestart = false;

void handleFwCheck(AsyncWebServerRequest* r)  { if (!FwBusy) fwRequest = 1; r->send(200, "text/plain", "OK"); }
void handleFwInstall(AsyncWebServerRequest* r){ if (!FwBusy) fwRequest = 2; r->send(200, "text/plain", "OK"); }

// Called every 10 ms from loop().
void MaintainFwUpdate(){
	if (fwRestart){ delay(1500); ESP.restart(); }
	uint8_t req = fwRequest;
	if (!req) return;
	fwRequest = 0;
	if (SimActive || CalActive){ FwStatus = fwStatusLine("Not now - switch SIMULATION / CALIBRATION off first"); return; }
	if (req == 1) doCheck();
	else          doInstall();
}

// ─── UPLOAD .bin (POST /update, multipart) ───────────────────────────────────
// The upload callback runs in async_tcp: it only writes the spare partition
// (Update is self-contained); the restart is left to loop().
static ImageCheck upChk;
static String     upErr;

void handleFwUploadData(AsyncWebServerRequest* request, String filename, size_t index,
                        uint8_t* data, size_t len, bool final){
	if (index == 0){
		upChk = ImageCheck(); upErr = "";
		if (FwBusy){ upErr = "another update is running"; return; }
		FwBusy = true;
		FwStatus = fwStatusLine("Receiving " + filename + "...");
		if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) upErr = "no room for the new firmware (" + String(Update.errorString()) + ")";
	}
	if (upErr.length()) return;
	upChk.feed(data, len);
	String hp = upChk.headerProblem();
	if (hp.length()){ upErr = hp; Update.abort(); return; }
	if (Update.write(data, len) != len){ upErr = "flash write failed (" + String(Update.errorString()) + ")"; Update.abort(); return; }
	if (final){
		if (!upChk.marker){ upErr = "file is not rain gauge firmware"; Update.abort(); return; }
		if (!Update.end(true)) upErr = "image check failed (" + String(Update.errorString()) + ")";
	}
}

void handleFwUploadDone(AsyncWebServerRequest* request){
	if (upErr.length() || Update.hasError()){
		String e = upErr.length() ? upErr : String(Update.errorString());
		FwStatus = fwStatusLine("Upload FAILED: " + e + " - the old firmware keeps running");
		FwBusy = false;
		request->send(200, "text/plain", "FAILED: " + e);
		return;
	}
	FwStatus = fwStatusLine("Uploaded firmware installed - restarting...");
	request->send(200, "text/plain", "OK - installed, the gauge restarts");
	fwRestart = true;     // loop() restarts (and SaveGenReport is not called from async_tcp)
}

// ─── ABOUT page block ────────────────────────────────────────────────────────
void appendFwUpdateBlock(){
	appendHtmlStr("<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:14px 0 4px'>FIRMWARE UPDATE</div>"
	              "<table><tr><td>Installed</td><td>" FW_VERSION "</td></tr>"
	              "<tr><td>On GitHub</td><td><span id='FWLATEST'>");
	appendHtmlStr(FwLatest.c_str());
	appendHtmlStr("</span></td></tr><tr><td>Status</td><td><span id='FWSTAT' style='font-size:12px'>");
	appendHtmlStr(FwStatus.c_str());
	appendHtmlStr("</span></td></tr></table>"
	              "<div class='btns'>"
	              "<button class='btn btn-bl' onclick=\"fetch('/fwcheck',{method:'POST'})\">CHECK FOR UPDATE</button>"
	              "<button class='btn btn-gr' onclick=\"if(confirm('Install the version from GitHub?\\n\\nThe gauge downloads it (about a minute) and restarts.\\nSettings and rain history are kept.'))fetch('/fwinstall',{method:'POST'})\">INSTALL FROM GITHUB</button>"
	              "</div>"
	              "<div style='padding:4px 8px;font-size:12px;color:#475569'>Or upload a <b>.bin</b> file "
	              "(downloaded from the GitHub page) from this PC / phone:<br>"
	              "<input type='file' id='fwfile' accept='.bin' style='margin:6px 0'> "
	              "<button class='btn btn-or' onclick='fwUpload()'>UPLOAD &amp; INSTALL</button> "
	              "<span id='fwup'></span></div>"
	              "<script>function fwUpload(){var f=document.getElementById('fwfile').files[0];"
	              "var o=document.getElementById('fwup');if(!f){o.textContent='Choose a .bin file first';return;}"
	              "var fd=new FormData();fd.append('firmware',f,f.name);var x=new XMLHttpRequest();"
	              "x.upload.onprogress=function(e){if(e.lengthComputable)o.textContent='Uploading '+Math.round(e.loaded*100/e.total)+'%';};"
	              "x.onload=function(){o.textContent=x.responseText;if(x.responseText.indexOf('OK')==0)setTimeout(function(){location.reload();},20000);};"
	              "x.onerror=function(){o.textContent='Upload failed (connection)';};"
	              "x.open('POST','/update');x.send(fd);}</script>");
}
