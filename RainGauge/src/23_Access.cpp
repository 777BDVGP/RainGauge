// =============================================================================
// 23_Access.cpp — Remote access password (see 23_Access.h)
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"
#include <mbedtls/sha256.h>
#include <esp_random.h>

// ── Shared state ─────────────────────────────────────────────────────────────
// Sessions and the password copy are used by async_tcp (pages, WebSocket) and
// changed by loop() (password set / clear) — short critical sections, no
// allocation inside.
static portMUX_TYPE accMux = portMUX_INITIALIZER_UNLOCKED;

struct AccSession { char key[33]; uint32_t startMs; bool used; };
static AccSession accSess[ACCESS_SESSIONS];

static char accSalt[33] = "";        // hex
static char accHash[65] = "";        // hex SHA-256(salt + password); "" = no password

// Password change from /setpass → MaintainAccess() writes the file.
static volatile bool accPendingSet = false;
static char accNewSalt[33], accNewHash[65];

// Log counters (async_tcp counts, loop logs).
static volatile uint16_t accOkCount = 0, accFailCount = 0, accLockCount = 0;
static char accLastIp[16] = "";
static uint8_t  accFails     = 0;    // wrong passwords in a row (async_tcp only)
static uint32_t accLockUntil = 0;    // millis; 0 = not locked

// WebSocket clients allowed to send commands.
#define ACC_WS_SLOTS 16
static uint32_t accWsOk[ACC_WS_SLOTS];

// ── Helpers ──────────────────────────────────────────────────────────────────
static void toHex(const uint8_t* b, size_t n, char* out){
	static const char* h = "0123456789abcdef";
	for (size_t i = 0; i < n; i++){ out[2*i] = h[b[i] >> 4]; out[2*i+1] = h[b[i] & 15]; }
	out[2*n] = '\0';
}

static void randomHex(char* out, size_t bytes){
	uint8_t b[32];
	for (size_t i = 0; i < bytes; i += 4){ uint32_t r = esp_random(); memcpy(b + i, &r, min((size_t)4, bytes - i)); }
	toHex(b, bytes, out);
}

static void hashPass(const char* salt, const String& pass, char* out65){
	String s = String(salt) + pass;
	uint8_t d[32];
	mbedtls_sha256((const unsigned char*)s.c_str(), s.length(), d, 0);
	toHex(d, 32, out65);
}

// Constant-time compare of two 64-char hex strings.
static bool sameHash(const char* a, const char* b){
	uint8_t diff = 0;
	for (int i = 0; i < 64; i++) diff |= (uint8_t)(a[i] ^ b[i]);
	return diff == 0 && a[0] != '\0';
}

bool AccessPasswordSet(){ return accHash[0] != '\0'; }

static bool isLocalIP(const IPAddress& ip){
	if (ip.type() != IPv4) return false;      // IPv6 from outside — treat as outside
	uint8_t a = ip[0], b = ip[1];
	if (a == 10 || a == 127) return true;
	if (a == 192 && b == 168) return true;
	if (a == 172 && b >= 16 && b <= 31) return true;
	if (a == 169 && b == 254) return true;
	return false;
}

bool AccessIsLocal(AsyncWebServerRequest* request){
	AsyncClient* c = request->client();
	return c && isLocalIP(c->remoteIP());
}

// Session key from the "rgsid" cookie ("" when none).
static String cookieKey(AsyncWebServerRequest* request){
	const AsyncWebHeader* h = request->getHeader("Cookie");
	if (!h) return "";
	const String& v = h->value();
	int p = v.indexOf("rgsid=");
	if (p < 0) return "";
	String k = v.substring(p + 6, p + 6 + 32);
	return k.length() == 32 ? k : "";
}

static bool sessionValid(const String& key){
	if (key.length() != 32) return false;
	bool ok = false;
	uint32_t now = millis();
	portENTER_CRITICAL(&accMux);
	for (int i = 0; i < ACCESS_SESSIONS; i++){
		if (!accSess[i].used) continue;
		if (now - accSess[i].startMs >= ACCESS_SESSION_MS){ accSess[i].used = false; continue; }
		if (strncmp(accSess[i].key, key.c_str(), 32) == 0) ok = true;
	}
	portEXIT_CRITICAL(&accMux);
	return ok;
}

static void sessionNew(char* out33){
	randomHex(out33, 16);
	uint32_t now = millis();
	portENTER_CRITICAL(&accMux);
	int slot = 0;                                   // free slot, else the oldest
	for (int i = 0; i < ACCESS_SESSIONS; i++){
		if (!accSess[i].used){ slot = i; break; }
		if (now - accSess[i].startMs > now - accSess[slot].startMs) slot = i;
	}
	memcpy(accSess[slot].key, out33, 33);
	accSess[slot].startMs = now;
	accSess[slot].used    = true;
	portEXIT_CRITICAL(&accMux);
}

static void sessionDrop(const String& key){
	portENTER_CRITICAL(&accMux);
	for (int i = 0; i < ACCESS_SESSIONS; i++)
		if (accSess[i].used && key.length() == 32 && strncmp(accSess[i].key, key.c_str(), 32) == 0) accSess[i].used = false;
	portEXIT_CRITICAL(&accMux);
}

// Home network, or outside with a valid login.
bool AccessAllowed(AsyncWebServerRequest* request){
	if (AccessIsLocal(request)) return true;
	if (!AccessPasswordSet())   return false;
	return sessionValid(cookieKey(request));
}

static void setNav(AsyncWebServerRequest* request){
	NavOutside = !AccessIsLocal(request);
	NavAuthed  = AccessAllowed(request);
}

static void denied(AsyncWebServerRequest* request){
	if (request->method() == HTTP_GET && !request->url().startsWith("/api") &&
	    !request->url().startsWith("/files/") && request->url() != "/wifiscan")
		request->redirect(AccessPasswordSet() ? "/login" : "/");
	else
		request->send(403, "text/plain", "Not allowed from outside the home network - log in first");
}

// ── Route wrappers (AddRoutes) ───────────────────────────────────────────────
ArRequestHandlerFunction AccessGuard(ArRequestHandlerFunction f){
	return [f](AsyncWebServerRequest* r){
		if (!AccessAllowed(r)){ denied(r); return; }
		setNav(r);
		f(r);
	};
}

ArRequestHandlerFunction AccessOpen(ArRequestHandlerFunction f){
	return [f](AsyncWebServerRequest* r){ setNav(r); f(r); };
}

ArUploadHandlerFunction AccessGuardUpload(ArUploadHandlerFunction f){
	return [f](AsyncWebServerRequest* r, const String& fn, size_t idx, uint8_t* d, size_t len, bool fin){
		if (!AccessAllowed(r)) return;        // data dropped; the done handler answers 403
		f(r, fn, idx, d, len, fin);
	};
}

// ── WebSocket ────────────────────────────────────────────────────────────────
void AccessWsConnect(uint32_t id, AsyncWebServerRequest* request){
	if (!request || !AccessAllowed(request)) return;   // may watch, not send commands
	portENTER_CRITICAL(&accMux);
	int slot = -1;
	for (int i = 0; i < ACC_WS_SLOTS; i++) if (accWsOk[i] == 0){ slot = i; break; }
	if (slot < 0){                                      // full: drop the oldest (lowest id)
		slot = 0;
		for (int i = 1; i < ACC_WS_SLOTS; i++) if (accWsOk[i] < accWsOk[slot]) slot = i;
	}
	accWsOk[slot] = id;
	portEXIT_CRITICAL(&accMux);
}

void AccessWsDisconnect(uint32_t id){
	portENTER_CRITICAL(&accMux);
	for (int i = 0; i < ACC_WS_SLOTS; i++) if (accWsOk[i] == id) accWsOk[i] = 0;
	portEXIT_CRITICAL(&accMux);
}

bool AccessWsAllowed(uint32_t id){
	bool ok = false;
	portENTER_CRITICAL(&accMux);
	for (int i = 0; i < ACC_WS_SLOTS; i++) if (id != 0 && accWsOk[i] == id) ok = true;
	portEXIT_CRITICAL(&accMux);
	return ok;
}

// ── /Access.json ─────────────────────────────────────────────────────────────
void AccessLoad(){
	accSalt[0] = accHash[0] = '\0';
	File f = SPIFFS.open(StrAccessFile, "r");
	if (!f) return;
	StaticJsonDocument<256> doc;
	if (!deserializeJson(doc, f)){
		const char* s = doc["salt"] | "";
		const char* h = doc["hash"] | "";
		if (strlen(s) == 32 && strlen(h) == 64){ strlcpy(accSalt, s, sizeof(accSalt)); strlcpy(accHash, h, sizeof(accHash)); }
	}
	f.close();
}

// Called every 10 ms from loop(): saves a new / cleared password, writes the log lines.
void MaintainAccess(){
	if (accPendingSet){
		char s[33], h[65];
		portENTER_CRITICAL(&accMux);
		memcpy(s, accNewSalt, sizeof(s)); memcpy(h, accNewHash, sizeof(h));
		accPendingSet = false;
		portEXIT_CRITICAL(&accMux);
		if (h[0] == '\0'){
			SPIFFS.remove(StrAccessFile);
		} else {
			StaticJsonDocument<256> doc;
			doc["salt"] = s; doc["hash"] = h;
			File f = SPIFFS.open(StrAccessFile, "w");
			if (f){ serializeJson(doc, f); f.close(); }
		}
		portENTER_CRITICAL(&accMux);
		memcpy(accSalt, s, sizeof(accSalt)); memcpy(accHash, h, sizeof(accHash));
		for (int i = 0; i < ACCESS_SESSIONS; i++) accSess[i].used = false;   // everybody logs in again
		portEXIT_CRITICAL(&accMux);
		SaveGenReport(h[0] ? F("Remote access: password set (outside visitors must log in again)")
		                   : F("Remote access: password cleared - outside visitors see only HOME and REPORTS"));
	}
	static uint16_t ok = 0, fail = 0, lock = 0;
	if (accOkCount   != ok)  { ok   = accOkCount;   SaveGenReport("Remote access: login from " + String(accLastIp)); }
	if (accFailCount != fail){ fail = accFailCount; SaveGenReport("Remote access: WRONG password from " + String(accLastIp)); }
	if (accLockCount != lock){ lock = accLockCount; SaveGenReport(F("Remote access: login locked for 15 min after 5 wrong passwords")); }
}

// ── Pages ────────────────────────────────────────────────────────────────────
static void loginPage(AsyncWebServerRequest* request, const char* msg, bool red){
	if (!beginHtmlStream(request)) return;
	NavOutside = true; NavAuthed = false;
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_LOGIN);
	appendMainPageSel(PS_LOGIN);
	appendHtmlStr("<div style='max-width:340px;margin:18px auto;text-align:center'>"
	              "<p style='text-align:center;margin:0 0 12px;color:#475569'>"
	              "HOME and REPORTS are open to everybody. Log in to change settings.</p>");
	if (msg){
		appendHtmlStr("<p style='text-align:center;margin:0 0 10px;font-weight:700;color:");
		appendHtmlStr(red ? "#b91c1c'>" : "#15803d'>");
		appendHtmlStr(msg);
		appendHtmlStr("</p>");
	}
	appendHtmlStr("<form method='post' action='/login'>"
	              "<input type='password' name='pass' autocomplete='current-password' placeholder='Password' "
	              "style='width:220px;max-width:80vw;font-size:15px;padding:6px 8px' autofocus>"
	              "<div class='btns'><button class='btn btn-bl' type='submit'>LOG IN</button></div></form></div>");
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

void handleLogin(AsyncWebServerRequest* request){
	if (AccessIsLocal(request))  { request->redirect("/settings"); return; }   // home network: no login needed
	if (!AccessPasswordSet())    { request->redirect("/");         return; }   // remote access off
	bool locked = accLockUntil && (int32_t)(accLockUntil - millis()) > 0;     // now < lockUntil
	if (!locked) accLockUntil = 0;

	if (request->method() != HTTP_POST){
		if (AccessAllowed(request)) { request->redirect("/settings"); return; }
		const char* m = nullptr;
		if (locked)                           m = "Too many wrong passwords - try again in 15 minutes.";
		else if (request->hasParam("e"))      m = "Wrong password.";
		loginPage(request, m, true);
		return;
	}
	if (locked){ request->redirect("/login"); return; }

	String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
	char salt[33], hash[65], want[65];
	portENTER_CRITICAL(&accMux);
	memcpy(salt, accSalt, sizeof(salt)); memcpy(want, accHash, sizeof(want));
	portEXIT_CRITICAL(&accMux);
	hashPass(salt, pass, hash);
	strlcpy(accLastIp, request->client()->remoteIP().toString().c_str(), sizeof(accLastIp));

	if (pass.length() > 0 && pass.length() <= ACCESS_PASS_MAX && sameHash(hash, want)){
		accFails = 0;
		char key[33];
		sessionNew(key);
		accOkCount++;
		AsyncWebServerResponse* r = request->beginResponse(302);
		r->addHeader("Location", "/settings");
		r->addHeader("Set-Cookie", String("rgsid=") + key + "; Path=/; Max-Age=86400; HttpOnly; SameSite=Lax");
		request->send(r);
		return;
	}
	accFailCount++;
	if (++accFails >= ACCESS_MAX_FAILS){
		accFails = 0;
		accLockUntil = (millis() + ACCESS_LOCK_MS) | 1;
		accLockCount++;
	}
	request->redirect("/login?e=1");
}

void handleLogout(AsyncWebServerRequest* request){
	sessionDrop(cookieKey(request));
	AsyncWebServerResponse* r = request->beginResponse(302);
	r->addHeader("Location", "/");
	r->addHeader("Set-Cookie", "rgsid=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
	request->send(r);
}

// POST /setpass  pass=<new>  (empty = clear) — home network only.
void handleSetPass(AsyncWebServerRequest* request){
	if (!AccessIsLocal(request)){ request->send(403, "text/plain", "The password can only be changed from the home network"); return; }
	String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
	if (pass.length() > 0 && (pass.length() < ACCESS_PASS_MIN || pass.length() > ACCESS_PASS_MAX)){
		request->send(400, "text/plain", "Password must be " + String(ACCESS_PASS_MIN) + " to " + String(ACCESS_PASS_MAX) + " characters");
		return;
	}
	char s[33] = "", h[65] = "";
	if (pass.length()){ randomHex(s, 16); hashPass(s, pass, h); }
	portENTER_CRITICAL(&accMux);
	memcpy(accNewSalt, s, sizeof(s)); memcpy(accNewHash, h, sizeof(h));
	accPendingSet = true;
	portEXIT_CRITICAL(&accMux);
	request->send(200, "text/plain", pass.length() ? "Password set" : "Password cleared");
}

// SETTINGS block — only built for the home network (handleSettings).
void appendAccessSection(){
	appendHtmlStr("<div class='noupd' style='border:1px solid #cbd5e1;border-radius:6px;padding:8px;margin:10px 0'>"
	              "<div style='font-weight:800;color:#0284c7;text-align:center;letter-spacing:1px'>REMOTE ACCESS (from outside your network)</div>"
	              "<p style='margin:6px 0;color:#475569;font-size:12px'>Visitors from outside (e.g. through a port forward on the router) "
	              "see only HOME and REPORTS. With a password they can LOG IN for one day and use everything. "
	              "On your home network no password is needed. Status: <b id='ACCST' style='color:");
	appendHtmlStr(AccessPasswordSet() ? "#15803d'>PASSWORD SET" : "#b91c1c'>NO PASSWORD - remote login switched off");
	appendHtmlStr("</b></p>"
	              "<div style='text-align:center'><input type='password' id='ACCPW' autocomplete='new-password' "
	              "placeholder='New password (6-64)' style='width:200px;max-width:60vw'> "
	              "<button class='btn btn-bl' onclick='accSet(1)'>SET PASSWORD</button> ");
	if (AccessPasswordSet()) appendHtmlStr("<button class='btn btn-rd' onclick='accSet(0)'>CLEAR</button>");
	appendHtmlStr("</div><p style='margin:6px 0 0;color:#92400e;font-size:11px'>The page uses http, not https: "
	              "use a password you do not use anywhere else.</p></div>"
	              "<script>function accSet(on){var p=on?document.getElementById('ACCPW').value:'';"
	              "if(on&&p.length<6){alert('At least 6 characters');return;}"
	              "if(!on&&!confirm('Clear the password? Outside visitors then see only HOME and REPORTS.'))return;"
	              "fetch('/setpass',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
	              "body:'pass='+encodeURIComponent(p)}).then(function(r){return r.text();}).then(function(t){"
	              "alert(t);setTimeout(function(){location.reload();},300);});}</script>");
}

// ABOUT: who is this visitor?
void appendAccessInfo(AsyncWebServerRequest* request){
	String ip = request->client() ? request->client()->remoteIP().toString() : "?";
	bool local = AccessIsLocal(request);
	// The VISITOR's address (PC / phone viewing this page) — not the gauge's,
	// which is under NETWORK → IP Address.
	appendHtmlStr("<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:14px 0 4px'>REMOTE ACCESS</div>"
	              "<table class='info'><tr><td>Your device (viewing this page)</td><td>");
	appendHtmlStr(ip.c_str());
	appendHtmlStr(local ? " - home network, no password needed" : " - outside, logged in");
	appendHtmlStr("</td></tr><tr><td>Remote access password</td><td>");
	appendHtmlStr(AccessPasswordSet() ? "Set" : "Not set - outside visitors see only HOME and REPORTS");
	appendHtmlStr("</td></tr></table>");
}
