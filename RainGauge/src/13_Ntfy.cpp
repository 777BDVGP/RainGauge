// =============================================================================
// 13_Ntfy.cpp — NTFY.sh Push Notifications & NTFY Configuration Web Page
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// NtfyParams — Constructor / Destructor / Persistence
// =============================================================================

NtfyParams::NtfyParams()  {}
NtfyParams::~NtfyParams() {}

void NtfyParams::WriteData()
{
	File f = SPIFFS.open("/NTFY.json", "w");
	if (!f) return;
	DynamicJsonDocument doc(512);
	doc["NtfyCode"]            = NtfyCode;
	doc["NtfyPriority"]        = NtfyPriority;
	doc["EnableIrFaultNotify"] = EnableIrFaultNotify;
	doc["EnableTempHigh"]      = EnableTempHigh;
	doc["TempHighC"]           = TempHighC;
	doc["EnableTempLow"]       = EnableTempLow;
	doc["TempLowC"]            = TempLowC;
	serializeJsonPretty(doc, f);
	f.close();
}

void NtfyParams::ReadData()
{
	File f = SPIFFS.open("/NTFY.json", "r");
	if (!f) return;
	if (f.size() > 512) { f.close(); return; }
	DynamicJsonDocument doc(512);
	if (!deserializeJson(doc, f)) {
		if (!doc["NtfyCode"].isNull())            NtfyCode            = doc["NtfyCode"].as<String>();
		if (!doc["NtfyPriority"].isNull())        NtfyPriority        = doc["NtfyPriority"].as<String>();
		if (!doc["EnableIrFaultNotify"].isNull()) EnableIrFaultNotify = doc["EnableIrFaultNotify"].as<bool>();
		if (!doc["EnableTempHigh"].isNull())      EnableTempHigh      = doc["EnableTempHigh"].as<bool>();
		if (!doc["TempHighC"].isNull())           TempHighC           = doc["TempHighC"].as<float>();
		if (!doc["EnableTempLow"].isNull())       EnableTempLow       = doc["EnableTempLow"].as<bool>();
		if (!doc["TempLowC"].isNull())            TempLowC            = doc["TempLowC"].as<float>();
	}
	f.close();
}

// =============================================================================
// CreateNtfyParams  (called from InitFileSystem in 03_Files.cpp)
// =============================================================================
void CreateNtfyParams() {
	NtfyData = new NtfyParams;
	NtfyData->ReadData();
	NtfyData->WriteData();
	MapNtfyPage();
}

// =============================================================================
// MapNtfyPage — binds the NTFY form elements to NtfyParams fields
// =============================================================================
void MapNtfyPage() {
	NtfyList = new PageVar[PAGE_VAR_NTFY];
	for (int i = 0; i < PAGE_VAR_NTFY; i++)
	{
		switch (i)
		{
			case NTFY_CODE:
				NtfyList[i].Var = new PageElement(&NtfyData->NtfyCode,            F("NTFY Topic Name"),          "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_EN_IR_FAULT:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableIrFaultNotify, F("Alert on IR Sensor Fault"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_PRIORITY:
				NtfyList[i].Var = new PageElement(&NtfyData->NtfyPriority, F("Alert Level (on the phone)"), "", &NtfyStyle, i + ID_NTFY);
				NtfyList[i].Var->SetStringToSelect(5);   // must match the number of AddOption calls
				NtfyList[i].Var->AddOption("urgent");
				NtfyList[i].Var->AddOption("high");
				NtfyList[i].Var->AddOption("default");
				NtfyList[i].Var->AddOption("low");
				NtfyList[i].Var->AddOption("min");
				break;
			case NTFY_EN_T_HIGH:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableTempHigh, F("High alert enabled"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_T_HIGH:
				NtfyList[i].Var = new PageElement(&NtfyHighDisp, F("Alert when Outside reaches (°C)"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_EN_T_LOW:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableTempLow, F("Low alert enabled"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_T_LOW:
				NtfyList[i].Var = new PageElement(&NtfyLowDisp, F("Alert when Outside drops to (°C)"), "", &NtfyStyle, i + ID_NTFY);
				break;
		}
	}
	NtfyPageObject = true;
	RefreshTempUnitLabels();   // limits + labels in the display unit
}

// -----------------------------------------------------------------------------
// RefreshTempUnitLabels — NTFY limit rows in the SETTINGS "Temperature Unit":
// copy TempHighC / TempLowC (°C) into the page copies, set label and range.
// Called at boot (MapNtfyPage), after a unit change and after NTFY UPDATE.
// -----------------------------------------------------------------------------
void RefreshTempUnitLabels(){
	if (!NtfyList || !NtfyData) return;
	NtfyHighDisp = roundf(TempDisp(NtfyData->TempHighC) * 10.0f) / 10.0f;
	NtfyLowDisp  = roundf(TempDisp(NtfyData->TempLowC)  * 10.0f) / 10.0f;
	NtfyList[NTFY_T_HIGH].Var->SetDescription(String("Alert when Outside reaches (") + TempUnitSym() + ")");
	NtfyList[NTFY_T_LOW].Var->SetDescription(String("Alert when Outside drops to (") + TempUnitSym() + ")");
	NtfyList[NTFY_T_HIGH].Var->SetRange(TempDisp(-40), TempDisp(70), 0.5);
	NtfyList[NTFY_T_LOW].Var->SetRange(TempDisp(-40), TempDisp(70), 0.5);
}

// =============================================================================
// handleNtfy  (GET /ntfy)
// =============================================================================
void handleNtfy(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;
	WT_SETUP_Ntfy();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_10);
	appendMainPageSel(PS_NTFY);
	appendHtmlStr(webScriptTag);
	// ── How to set NTFY up on a phone — above the parameter table.
	// Tick box shows / hides the help; the choice is remembered in this browser
	// (localStorage) — ticked the first time, so a new user sees it at once.
	appendHtmlStr("<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:14px 0 4px'>NTFY SETTINGS</div>"
	              "<div style='margin:8px 0 0;font-size:13px;text-align:left'><label>"
	              "<input type='checkbox' id='ntfyhelpcb' checked onchange='ntfyHelp(this.checked,1)'> "
	              "Show help (how NTFY works)</label></div>"
	              "<div id='ntfyhelp' style='background:#f1f5f9;border:1px solid #cbd5e1;border-radius:6px;padding:8px 10px;margin:4px 0 8px;font-size:12px;line-height:1.5;color:#334155'>"
	              "<b style='color:#0284c7'>Alerts on your phone with NTFY (free, no account needed)</b>"
	              "<ol style='margin:4px 0 4px 18px;padding:0'>"
	              "<li>Install the <b>ntfy</b> app on the phone (Google Play / App Store, by Philipp Heckel).</li>"
	              "<li>Make up a <b>topic name</b>: letters, numbers, - and _ only, no spaces, e.g. <i>raingauge-smith-7342</i>. "
	              "Anyone who knows the name can read the messages, so make it hard to guess.</li>"
	              "<li>In the app tap <b>+</b> (Subscribe to topic), type <b>exactly the same name</b> "
	              "(upper / lower case matters), server <i>ntfy.sh</i>, and subscribe.</li>"
	              "<li>Type the same name in <b>NTFY Topic Name</b> below, choose the <b>Alert Level</b>, press <b>UPDATE</b>, "
	              "then <b>TEST NTFY</b> &mdash; the test message must appear on the phone.</li>"
	              "</ol>"
	              "<b>Alert Level:</b> <i>urgent</i> = long vibration and sound, even in Do Not Disturb (if the app allows it); "
	              "<i>high</i> = sound and pop-up; <i>default</i> = normal notification; <i>low</i> = no sound; "
	              "<i>min</i> = silent, only in the list. Several phones can subscribe to the same topic.</div>"
	              "<script>function ntfyHelp(on,save){document.getElementById('ntfyhelp').style.display=on?'block':'none';"
	              "if(save){try{localStorage.setItem('ntfyHelp',on?'1':'0');}catch(e){}}}"
	              "(function(){var on=true;try{on=localStorage.getItem('ntfyHelp')!=='0';}catch(e){}"
	              "document.getElementById('ntfyhelpcb').checked=on;ntfyHelp(on,0);})();</script>");
	appendHtmlBuf(PageBuf, getPageBufLen());
	appendHtmlStr(webUpateButton);
	appendNtfyTestButton();

	// ── IR sensor test — live bucket position, whether a test can run, last result.
	IrBeamView v = IrBeamNow();
	appendHtmlStr("<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:14px 0 4px'>IR SENSOR TEST</div>"
	              "<table class='info'>"   // info: value column may wrap (long "Last result" text squeezed the labels to 1 char wide on a phone)
	              "<tr><td>Bucket Position (IR beam)</td><td><span id='NTBEAM' style='");
	appendHtmlStr(v.style.c_str());
	appendHtmlStr("'>");
	appendHtmlStr(v.beam.c_str());
	appendHtmlStr("</span></td></tr><tr><td>Test possible?</td><td><span id='NTIRCAN' style='font-size:12px'>");
	appendHtmlStr(v.can.c_str());
	appendHtmlStr("</span></td></tr><tr><td>Last result</td><td><span id='NTIRRES' style='font-size:12px'>");
	appendHtmlStr(IrTestText.c_str());
	appendHtmlStr("</span></td></tr></table>"
	              "<div class='btns'><button class='btn btn-gr' onclick=\"fetch('/irtest',{method:'POST'})\">TEST IR SENSOR NOW</button></div>"
	              "<p style='color:#475569;font-size:12px;padding:6px 8px'>"
	              "The test switches the IR LED off for 50 ms: the pickup must go dark, then see light again. "
	              "It only works while the beam is <b style='color:green'>OPEN</b> &mdash; when the bucket is "
	              "<b style='color:red'>BROKEN</b> (blocking the beam) the pickup is dark anyway, so tip the bucket over first. "
	              "The same test also runs by itself every few minutes (SETTINGS &rarr; IR Self-Test); "
	              "two failures in a row send an NTFY alert, and another when it is OK again.</p>");

	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// =============================================================================
// WT_SETUP_Ntfy — NTFY parameter table into PageBuf
// =============================================================================
void WT_SETUP_Ntfy() {
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("NTFY Parameter", "Value", "");
	WT_EditRow(&NtfyList[NTFY_CODE]);
	WT_EditRow(&NtfyList[NTFY_PRIORITY]);

	WT_SectionHeader("Rain Sensor");
	WT_RightCheckRow(&NtfyList[NTFY_EN_IR_FAULT]);

	// Outside temperature: tick box + limit on one row each (label = the limit's text)
	WT_SectionHeader("Outside Temperature Alerts  (tick to enable)");
	WT_CheckEditRow(&NtfyList[NTFY_EN_T_HIGH], &NtfyList[NTFY_T_HIGH]);
	WT_CheckEditRow(&NtfyList[NTFY_EN_T_LOW],  &NtfyList[NTFY_T_LOW]);

	WT_EndPage();
}

// =============================================================================
// SendNtfyPageValues  (called from MaintainWebPageValues)
// =============================================================================
void SendNtfyPageValues() {
	if (!NtfyPageObject) return;
	for (int i = 0; i < PAGE_VAR_NTFY; i++) {
		if (NtfyList[i].Var->HasChanged()) {
			webParamUpdate(NtfyList[i].Var->GetName(), NtfyList[i].Var->GetID(""), NtfyList[i].Var->GetVarStyle());
			NtfyList[i].Var->UpdateLast();
		}
	}
	// IR SENSOR TEST block — push whatever changed (every 50 ms, from loop).
	static String lastBeam, lastCan, lastRes;
	IrBeamView v = IrBeamNow();
	if (v.beam != lastBeam)   { webParamUpdate(v.beam,     "NTBEAM",  v.style); lastBeam = v.beam; }
	if (v.can  != lastCan)    { webParamUpdate(v.can,      "NTIRCAN", "");      lastCan  = v.can;  }
	if (IrTestText != lastRes){ webParamUpdate(IrTestText, "NTIRRES", "");      lastRes  = IrTestText; }
}

// -----------------------------------------------------------------------------
// CheckTempAlerts — outside high / low alerts (every reading, 10 s, from loop).
// One NTFY + General Log line when the outside temperature reaches the limit;
// it can only alert again after coming back TEMP_ALERT_HYST_C past the limit
// (so 34.9 / 35.0 / 34.9 / 35.0 does not send a message every 10 s).  Nothing
// in simulation, and nothing while the sensor gives no reading.
// -----------------------------------------------------------------------------
void CheckTempAlerts(float outside){
	static bool highOn = false, lowOn = false;
	if (!NtfyData || SimActive || isnan(outside)) return;

	if (!NtfyData->EnableTempHigh) highOn = false;
	else if (!highOn && outside >= NtfyData->TempHighC){
		highOn = true;
		String m = "HIGH outside temperature: " + TempNum(outside) + " " + TempUnitSym() + " (alert at " + TempNum(NtfyData->TempHighC) + " " + TempUnitSym() + ")";
		SaveGenReport{m};
		MessageNTFY(Params->SysName + " - " + m);
	}
	else if (highOn && outside <= NtfyData->TempHighC - TEMP_ALERT_HYST_C){
		highOn = false;
		SaveGenReport("Outside temperature back below " + TempNum(NtfyData->TempHighC) + " " + TempUnitSym() + " (" + TempNum(outside) + " " + TempUnitSym() + ")");
	}

	if (!NtfyData->EnableTempLow) lowOn = false;
	else if (!lowOn && outside <= NtfyData->TempLowC){
		lowOn = true;
		String m = "LOW outside temperature: " + TempNum(outside) + " " + TempUnitSym() + " (alert at " + TempNum(NtfyData->TempLowC) + " " + TempUnitSym() + ")";
		SaveGenReport{m};
		MessageNTFY(Params->SysName + " - " + m);
	}
	else if (lowOn && outside >= NtfyData->TempLowC + TEMP_ALERT_HYST_C){
		lowOn = false;
		SaveGenReport("Outside temperature back above " + TempNum(NtfyData->TempLowC) + " " + TempUnitSym() + " (" + TempNum(outside) + " " + TempUnitSym() + ")");
	}
}

// -----------------------------------------------------------------------------
// IrBeamNow — what the NTFY page shows about the IR beam right now.
// Uses the debounced pickup (HIGH = dark = beam broken), like HOME.
// -----------------------------------------------------------------------------
IrBeamView IrBeamNow(){
	IrBeamView v;
	bool broken = InPort[IIDX_IR_PICKUP].Input;
	v.beam  = broken ? "BROKEN" : "OPEN";
	v.style = broken ? ATT_RED : ATT_GREEN;
	if (SimActive)       v.can = "No - SIMULATION is on";
	else if (CalActive)  v.can = "No - CALIBRATION is running";
	else if (IrTestBusy) v.can = "Testing...";
	else if (broken)     v.can = "No - beam is BROKEN: tip the bucket over first";
	else                 v.can = "Yes - beam is OPEN";
	return v;
}

// -----------------------------------------------------------------------------
// handleIrTestNow  (POST /irtest, NTFY page button) — async_tcp: only raises
// the flag; MaintainIrSelfTest() (loop) runs the test or says why it can't.
// -----------------------------------------------------------------------------
void handleIrTestNow(AsyncWebServerRequest *request){
	IrTestNowRequested = true;
	request->send(200, "text/plain", "OK");
}

// =============================================================================
// UpdateNtfyPageValues  (from the WebSocket dispatcher when ID_NTFY received)
// =============================================================================
void UpdateNtfyPageValues(uint32_t clientId, JsonObject *root) {
	for (JsonPair kv : *root) {
		for (int i = 0; i < PAGE_VAR_NTFY; i++) {
			if (NtfyList[i].Var->GetID("") == kv.key().c_str()) {
				NtfyList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}
	// The limits were typed in the display unit: back to °C (0.1 resolution).
	NtfyData->TempHighC = roundf(TempFromDisp(NtfyHighDisp) * 10.0f) / 10.0f;
	NtfyData->TempLowC  = roundf(TempFromDisp(NtfyLowDisp)  * 10.0f) / 10.0f;
	RefreshTempUnitLabels();
	WebMessUpdate(clientId, "NTFY Updated!", "color: red", true);
	NtfyData->WriteData();
}

// =============================================================================
// NTFY.sh Push Notification Engine  (no task of its own)
// =============================================================================
// MessageNTFY() only queues the text.  MaintainNtfy() (loop) starts one POST
// at a time on an AsyncClient; the connect / DNS / send / reply all happen in
// the async_tcp task that the web server already runs, so loop() never waits
// and no extra stack is needed.  The callbacks touch only their own NtfyJob
// and the volatile result fields below; the job deletes itself on disconnect.

struct NtfyJob {
	AsyncClient client;
	String      request;
	uint32_t    gen;
};

static String            ntfyPending[NTFY_QUEUE];      // loop context only
static uint8_t           ntfyPendingN   = 0;
static volatile bool     ntfyBusy       = false;
static volatile uint32_t ntfyGen        = 0;           // id of the POST in flight
static volatile int      ntfyHttpCode   = 0;           // 0 = no reply (yet)
static volatile int8_t   ntfyErr        = 0;           // AsyncTCP error, 0 = none
static uint32_t          ntfyStartMs    = 0;

// MessageNTFY — queue a push notification; returns immediately.
// Silently dropped if the queue is full or the topic is blank.
void MessageNTFY(String message)
{
	if (!NtfyData) return;
	String code = NtfyData->NtfyCode;
	code.trim();
	if (code.length() == 0)              return;
	if (ntfyPendingN >= NTFY_QUEUE)      return;
	ntfyPending[ntfyPendingN++] = message;
}

static void ntfyStart(const String& message)
{
	String topic = NtfyData->NtfyCode;
	topic.trim();

	NtfyJob* job = new (std::nothrow) NtfyJob;
	if (!job){ SaveGenReport F("NTFY: no heap - message dropped"); return; }
	job->gen     = ++ntfyGen;
	// Alert level from the NTFY page; anything unknown -> urgent (old behaviour).
	String prio = NtfyData->NtfyPriority;
	if (prio != "urgent" && prio != "high" && prio != "default" && prio != "low" && prio != "min") prio = "urgent";
	job->request = "POST /" + topic + " HTTP/1.1\r\n"
	               "Host: " NTFY_HOST "\r\n"
	               "Content-Type: text/plain\r\n"
	               "X-Priority: " + prio + "\r\n"
	               "Content-Length: " + String(message.length()) + "\r\n"
	               "Connection: close\r\n\r\n" + message;

	ntfyHttpCode = 0;
	ntfyErr      = 0;
	ntfyBusy     = true;
	ntfyStartMs  = millis();

	AsyncClient& c = job->client;
	c.setRxTimeout(NTFY_TIMEOUT_SEC);
	c.onConnect([](void* arg, AsyncClient* cl){
		NtfyJob* j = (NtfyJob*)arg;
		cl->write(j->request.c_str(), j->request.length());
	}, job);
	c.onData([](void* arg, AsyncClient*, void* data, size_t len){
		// "HTTP/1.1 200 OK" — keep the status code of the first packet.
		NtfyJob* j = (NtfyJob*)arg;
		if (j->gen == ntfyGen && ntfyHttpCode == 0 && len >= 12)
			ntfyHttpCode = atoi((const char*)data + 9);
	}, job);
	c.onError([](void* arg, AsyncClient*, int8_t err){
		NtfyJob* j = (NtfyJob*)arg;
		if (j->gen == ntfyGen) ntfyErr = err;
	}, job);
	c.onTimeout([](void*, AsyncClient* cl, uint32_t){ cl->close(); }, job);
	c.onDisconnect([](void* arg, AsyncClient*){
		NtfyJob* j = (NtfyJob*)arg;
		if (j->gen == ntfyGen) ntfyBusy = false;
		delete j;                                 // the AsyncClient lives inside the job
	}, job);

	if (!c.connect(NTFY_HOST, 80)){
		// Not deleted: if AsyncTCP already fired onDisconnect it is gone, and
		// a rare leaked job beats a double free.
		ntfyBusy = false;
		ntfyGen++;
		SaveGenReport F("NTFY: connect to ntfy.sh failed");
	}
}

// MaintainNtfy  (called every 10 ms from loop())
// Reports the finished POST, then starts the next queued message.
void MaintainNtfy()
{
	static bool reported = true;
	if (NtfyTestRequested){
		NtfyTestRequested = false;
		MessageNTFY("TEST FROM " + Params->SysName);
		WebMessUpdate(Lastnum, "Test NTFY sent!", "color: red", true);
	}
	if (ntfyBusy){
		if (millis() - ntfyStartMs < (NTFY_TIMEOUT_SEC + 5) * 1000UL) return;
		ntfyBusy = false;                         // stuck — give up on it (its callbacks see a stale gen)
		ntfyGen++;
	}
	if (!reported){
		reported = true;
		if (ntfyHttpCode != 200)
			SaveGenReport("NTFY: send failed (HTTP " + String(ntfyHttpCode) + ", error " + String(ntfyErr) + ")");
	}
	if (ntfyPendingN == 0) return;
	if (WiFi.status() != WL_CONNECTED) return;

	String msg = ntfyPending[0];
	for (uint8_t i = 1; i < ntfyPendingN; i++) ntfyPending[i - 1] = ntfyPending[i];
	ntfyPending[--ntfyPendingN] = "";
	ntfyStart(msg);
	if (ntfyBusy) reported = false;
}

// =============================================================================
// Web Helpers
// =============================================================================

// appendNtfyTestButton — TEST NTFY button (GET /testntfy).
void appendNtfyTestButton() {
	appendHtmlStr("<script>function ButtonNtfyTest(){fetch('/testntfy');}</script>"
	              "<div><br></div><div>"
	              "<b><a><input type=\"button\" id=\"TESTNTFY\" onclick=\"ButtonNtfyTest()\" style=\"font-size:14px\" value=\"TEST NTFY\"></a></b>"
	              "</div>");
}

// handleTestNtfy  (GET /testntfy)
// Runs in async_tcp — only raises a flag; MaintainNtfy (loop) queues the test.
void handleTestNtfy(AsyncWebServerRequest *request)
{
	NtfyTestRequested = true;
	request->send(200, "text/plain", "OK");
}
