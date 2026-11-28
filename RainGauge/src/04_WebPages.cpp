// =============================================================================
// 04_WebPages.cpp — Web Service Upkeep, Server Setup & Page Builder Utilities
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// Web / WebSocket maintenance — driven from loop() (see main.cpp).
// ArduinoOTA.handle() is polled unconditionally; the rest pauses during a flash.
void MaintainWebService() {
	static uint32_t lastOTACheck  = 0;
	static uint32_t lastWebPage   = 0;
	static uint32_t lastHomePage  = 0;
	uint32_t now = millis();

	if (now - lastOTACheck >= 200) {
		lastOTACheck = now;
		ArduinoOTA.handle();
	}

	if (!OTAInProgress) {
		if (now - lastWebPage >= 50) {
			lastWebPage = now;
			MaintainWebPage();
		}
		UpdateWebPageTime();

		if (now - lastHomePage >= 500) {
			lastHomePage = now;
			MaintainHomePage();
			SendCalibrationValues(false);
			SendSimulationValues(false);
		}
	}
}

bool beginHtmlStream(AsyncWebServerRequest *request)
{
	uint32_t freeHeap    = ESP.getFreeHeap();
	uint32_t maxAlloc    = ESP.getMaxAllocHeap();
	if (freeHeap < HEAP_FREE_MIN || maxAlloc < HEAP_MAXBLK_MIN) {
		// The 503 page refreshes every second — log once a minute, not every hit.
		static uint32_t last503Log = 0;
		if (last503Log == 0 || millis() - last503Log >= 60000UL){
			last503Log = millis() | 1;
			SaveGenReport("503: heap=" + String(freeHeap) + " maxBlk=" + String(maxAlloc));
		}
		request->send(503, "text/html",
		    "<html><head><meta http-equiv='refresh' content='1'></head>"
		    "<body>Low memory &mdash; retrying&hellip;</body></html>");
		return false;
	}
	return true;
}

void SetupWebPage(){
	AddRoutes();
	ws.onEvent(webSocketEvent);
	server.addHandler(&ws);
	server.begin();
}

// GET /wss.js — the client script, served once and cached by the browser so
// it isn't re-sent (and re-parsed) with every page.  Streamed straight from
// the flash-resident buffer; no heap copy, no beginHtmlStream() guard.
void handleWssJs(AsyncWebServerRequest *request){
	AsyncWebServerResponse *res = request->beginResponse(
		200, "application/javascript",
		reinterpret_cast<const uint8_t*>(webSocketScript),
		strlen(webSocketScript));
	res->addHeader("Cache-Control", "public, max-age=604800");   // 7 days; ?v= busts it
	request->send(res);
}

// -----------------------------------------------------------------------------
// WebSocket command queue
// webSocketEvent runs in the async_tcp task.  It only copies each incoming
// frame into wsCmdQ; MaintainWsCommands() (loop) parses and applies it, so
// Params / Sim / calibration / NTFY settings are only ever changed by loop().
// -----------------------------------------------------------------------------
struct WsCmd { uint32_t clientId; uint8_t* data; size_t len; };
static WsCmd             wsCmdQ[WS_CMD_QUEUE];
static uint8_t           wsCmdHead   = 0;
static uint8_t           wsCmdCount  = 0;
static volatile uint32_t wsCmdDropped = 0;
static portMUX_TYPE      wsCmdMux    = portMUX_INITIALIZER_UNLOCKED;

static void queueWsCommand(uint32_t clientId, const uint8_t* data, size_t len){
	if (len > WS_CMD_MAX_LEN){ wsCmdDropped++; return; }
	uint8_t* copy = (uint8_t*)malloc(len);
	if (!copy){ wsCmdDropped++; return; }
	memcpy(copy, data, len);
	bool queued = false;
	portENTER_CRITICAL(&wsCmdMux);
	if (wsCmdCount < WS_CMD_QUEUE){
		wsCmdQ[(wsCmdHead + wsCmdCount) % WS_CMD_QUEUE] = { clientId, copy, len };
		wsCmdCount++;
		queued = true;
	}
	portEXIT_CRITICAL(&wsCmdMux);
	if (!queued){ free(copy); wsCmdDropped++; }
}

// MaintainWsCommands  (called every 10 ms from loop())
void MaintainWsCommands(){
	static uint32_t loggedDrops = 0;
	if (wsCmdDropped != loggedDrops){
		loggedDrops = wsCmdDropped;
		SaveGenReport("WebSocket command dropped (queue full / too long / no heap), total " + String(loggedDrops));
	}
	for (;;){
		WsCmd c;
		bool have = false;
		portENTER_CRITICAL(&wsCmdMux);
		if (wsCmdCount){
			c = wsCmdQ[wsCmdHead];
			wsCmdHead = (wsCmdHead + 1) % WS_CMD_QUEUE;
			wsCmdCount--;
			have = true;
		}
		portEXIT_CRITICAL(&wsCmdMux);
		if (!have) return;
		UnpackJsonPayload(c.clientId, c.data, c.len);
		free(c.data);
	}
}

void MaintainWebPage(){
	ws.cleanupClients();
	MaintainWebPageValues();
	UpdateSelectMessDelay();
}

void webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){

	switch (type){
		case WS_EVT_DISCONNECT:
		break;

		case WS_EVT_CONNECT: {
			String ActTime = DateTime;
			webParamUpdate(Params->SysName, "H01", "");
			webParamUpdate(ActTime,         "H02", "");
		}
		break;

		case WS_EVT_DATA: {
			AwsFrameInfo *info = (AwsFrameInfo*)arg;
			if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT){
				queueWsCommand(client->id(), data, len);   // applied by loop()
			}
		}
		break;

		case WS_EVT_ERROR:
		case WS_EVT_PONG:
		break;
	}
}

void webParamUpdate(const String& Name, const String& ID, const String& ATT, const char* ATTName){
	if (ws.count() > 0){
		StaticJsonDocument<256> doc;
		doc[ID] = Name;
		if (ATT.length() > 0){
			doc["Attribute"] = ATT;
			if (strcmp(ATTName, "style") != 0) doc["AttributeName"] = ATTName;
		}
		char buf[256];
		serializeJson(doc, buf, sizeof(buf));
		ws.textAll(buf);
	}
}

void UnpackJsonPayload(uint32_t clientId, uint8_t *payload, size_t length){
	DynamicJsonDocument doc((length + 500));

	// Bounded parse — the WebSocket frame is not null-terminated.
	DeserializationError error = deserializeJson(doc, (const char*)payload, length);
	if (error){
		SaveGenReport F("Error parsing JSON payload");
		return;
	}

	JsonObject root = doc.as<JsonObject>();
	if (root.isNull() || root.size() == 0) return;

	JsonObject::iterator it = root.begin();
	String firstKey = it->key().c_str();
	int WEBPAGE = firstKey.toInt();

	switch (WEBPAGE)
	{
		case ID_HOME:
			break;

		case ID_SETTINGS:
			UpdateSettingsPageValues(clientId, &root);
			break;

		case ID_SIM:
			UpdateSimPageValues(clientId, &root);
			break;

		case ID_NTFY:
			if (NtfyPageObject) UpdateNtfyPageValues(clientId, &root);
			break;

		case 0:
			for (JsonPair kv : root){
				if (kv.key() == "REBOOT" && kv.value() == "YES"){
					DEBUG_PRINTLN("Manual reboot requested via WebSocket");
					WebMessUpdate(clientId, "Rebooting — communications will be lost!", "color: red", true);
					SaveGenReport F("REBOOT - Manual reboot via web interface");
					delay(500);
					ESP.restart();
				}
				else if (kv.key() == "CAL"){ CalCommand(clientId, kv.value().as<String>()); }
				else if (kv.key() == "SIM"){ SimCommand(clientId, kv.value().as<String>()); }
			}
			break;
	}
}

void WebMessUpdate(uint32_t clientId, const String& Mess, const String& Att, bool Reset){
	webParamUpdate(Mess, "H03", Att);
	if (Reset){
		Lastnum        = clientId;
		LastnumUpdated = false;
		activateTimer(TIM_RESET_OPTION_MESS);
	}
}

void UpdateSelectMessDelay(){
	if (!LastnumUpdated){
		if (TimerExpired(TIM_RESET_OPTION_MESS)){
			WebMessUpdate(Lastnum, "Selection Option", "color:#3366ff", false);
			LastnumUpdated = true;
		}
	}
}

void append_FileAction(int ReqMess)
{
	const char* Message;
	const char* LColor;
	int ActMess = ReqMess;
	if (SetMess != 0) ActMess = SetMess;

	switch (ActMess)
	{
		case FA_MESS_01: Message = "Files on SPIFFS";                    LColor = COLOR_DEFAULT; break;
		case FA_MESS_02: Message = "Click on File to Download";          LColor = COLOR_BLUE;    break;
		case FA_MESS_03: Message = "Upload a File";                      LColor = COLOR_BLUE;    break;
		case FA_MESS_04: Message = "Click on File to Delete";            LColor = COLOR_BLUE;    break;
		case FA_MESS_06: Message = "Selection Option";                   LColor = COLOR_BLUE;    break;
		case FA_MESS_07: Message = "Files on SPIFFS";                    LColor = COLOR_BLUE;    break;
		case FA_MESS_08: Message = "Reports";                            LColor = COLOR_BLUE;    break;
		case FA_MESS_09: Message = "File Not Found";                     LColor = COLOR_RED;     break;
		case FA_MESS_10: Message = "NTFY Notifications";                 LColor = COLOR_BLUE;    break;
		case FA_MESS_RAIN: Message = "Rain Report";                      LColor = COLOR_BLUE;    break;
		case FA_MESS_TEMP: Message = "Temperature Report";               LColor = COLOR_BLUE;    break;
		case FA_MESS_WB:   Message = "Water Balance Report";             LColor = COLOR_BLUE;    break;
		case FA_MESS_RED: Message = SpesialStr.c_str();                  LColor = COLOR_RED;     break;
		case FA_MESS_GRN: Message = SpesialStr.c_str();                  LColor = COLOR_GREEN;   break;
		default:          Message = "";                                  LColor = COLOR_BLUE;    break;
	}

	appendHtmlStr("<h2 id=\"H01\">"); appendHtmlStr(Params->SysName.c_str()); appendHtmlStr("</h2>");
	appendHtmlStr("<h4 id=\"H02\">"); appendHtmlStr(DateTime.c_str());         appendHtmlStr("</h4>");
	appendHtmlStr("<h4 id=\"H03\" style="); appendHtmlStr(LColor); appendHtmlStr(">"); appendHtmlStr(Message); appendHtmlStr("</h4>");

	if (SimActive)
		appendHtmlStr("<div style='background:#dc2626;color:#fff;text-align:center;font-weight:800;letter-spacing:1px;"
		              "padding:4px;border-radius:4px;margin:4px 0'>SIMULATION MODE &mdash; "
		              "<a href='/simulate' style='color:#fff'>tips &amp; temperatures are simulated</a></div>");

	SetMess = 0;
}

// =============================================================================
// WT_* Table Builder Primitives
// =============================================================================

void WT_StartTable(const char* Hdr1, const char* Hdr2, const char* Hdr3){
	WT_Header(Hdr1);
	if (Hdr2 && *Hdr2) WT_Header(Hdr2);
	if (Hdr3 && *Hdr3) WT_Header(Hdr3);
}

void WT_Column(int WT_Type, int WT_Begin, int WT_Finish, const char* Type, const char* Labval, const char* ID, const char* Style, PageVar *ListObj, const char* ExtraAttrs){

	if (WT_Type == WT_LABEL) {
		if (!Labval || !*Labval) { WT_StartLabelEmpty(); return; }
		if (WT_Begin == WT_FIRST) WT_StartLabel();
		WT_TypeCell(Type);
		WT_TextLabel(Labval);
	} else if (WT_Type == WT_INPUT) {
		// Only skip (empty cell) when both Type and Labval are blank —
		// a text input with an empty value is still a valid rendered field.
		if ((!Type || !*Type) && (!Labval || !*Labval)) { WT_StartInputEmpty(); return; }
		if (!Labval) Labval = "";   // allow empty string through as input value
		if (WT_Begin == WT_FIRST) WT_StartInput();
		WT_TypeCell(Type);
		WT_ValueInput(Labval);
	}

	WT_IDCell(ID);

	const char* effectiveStyle = Style ? Style : "";
	char fullStyleBuf[128];
	if (ListObj && ListObj->Var) {
		const String& ta = ListObj->Var->GetAttrTextAlign();
		if (ta.length() > 0) {
			if (Style && *Style)
				snprintf(fullStyleBuf, sizeof(fullStyleBuf), "%s;text-align:%s", Style, ta.c_str());
			else
				snprintf(fullStyleBuf, sizeof(fullStyleBuf), "text-align:%s", ta.c_str());
			effectiveStyle = fullStyleBuf;
		}
	}

	WT_AttrsStart();
	WT_Attr("style", effectiveStyle);

	if (WT_Type == WT_INPUT && ListObj && ListObj->Var) {
		char numBuf[16];
		if (ListObj->Var->HasAttrMin())  { snprintf(numBuf, sizeof(numBuf), "%g", ListObj->Var->GetAttrMin());  WT_Attr("min",  numBuf); }
		if (ListObj->Var->HasAttrMax())  { snprintf(numBuf, sizeof(numBuf), "%g", ListObj->Var->GetAttrMax());  WT_Attr("max",  numBuf); }
		if (ListObj->Var->HasAttrStep()) { snprintf(numBuf, sizeof(numBuf), "%g", ListObj->Var->GetAttrStep()); WT_Attr("step", numBuf); }
		const String& ph = ListObj->Var->GetAttrPlaceholder();
		if (ph.length() > 0) WT_Attr("placeholder", ph.c_str());
		if (ListObj->Var->GetAttrReadOnly()) WT_Attr("readonly", "");
	}

	if (ExtraAttrs && *ExtraAttrs) {
		const char* p = ExtraAttrs;
		while (*p) {
			const char* comma = strchr(p, ',');
			size_t pairLen = comma ? (size_t)(comma - p) : strlen(p);
			const char* colon = (const char*)memchr(p, ':', pairLen);
			if (colon && colon > p) {
				char aname[32], aval[64];
				size_t nLen = (size_t)(colon - p);
				size_t vLen = pairLen - nLen - 1;
				if (nLen < sizeof(aname) && vLen < sizeof(aval)) {
					memcpy(aname, p, nLen); aname[nLen] = '\0';
					memcpy(aval, colon + 1, vLen); aval[vLen] = '\0';
					WT_Attr(aname, aval);
				}
			}
			if (!comma) break;
			p = comma + 1;
		}
	}

	WT_AttrsEnd();

	if (Type && strcmp(Type, "select") == 0 && ListObj && ListObj->Var) {
		WT_OptionsStart();
		int cnt = ListObj->Var->GetOptionCount();
		for (int i = 0; i < cnt; i++) WT_OptionsValues(ListObj->Var->GetOption(i).c_str());
		WT_OptionsEnd();
	}

	if (WT_Finish == WT_MORE) WT_NextColumn();
	else                      WT_EndCell();
}

void WT_AutoColumn(int WT_Type, int WT_Begin, int WT_Finish, PageVar *pv, const char* IDPrefix, const char* ExtraAttrs){
	String id = pv->Var->GetID(IDPrefix);
	if (WT_Type == WT_LABEL) {
		WT_Column(WT_LABEL, WT_Begin, WT_Finish, "text",
		          pv->Var->GetDescription().c_str(), id.c_str(),
		          pv->Var->GetFStyle().c_str(), pv, ExtraAttrs);
	} else {
		String typeStr = pv->Var->GetTypeStr();
		String nameStr = pv->Var->GetName();
		WT_Column(WT_INPUT, WT_Begin, WT_Finish,
		          typeStr.c_str(), nameStr.c_str(),
		          id.c_str(), pv->Var->GetFStyle().c_str(),
		          pv, ExtraAttrs);
	}
}

void WT_DisplayRow(PageVar *pv){
	String idL = pv->Var->GetID("L");
	String idV = pv->Var->GetID("V");
	String nameStr = pv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_MORE, "text",
	          pv->Var->GetDescription().c_str(), idL.c_str(),
	          pv->Var->GetFStyle().c_str(), pv);
	WT_Column(WT_LABEL, WT_NEXT, WT_LAST, "text",
	          nameStr.c_str(), idV.c_str(),
	          pv->Var->GetVarStyle().c_str(), pv);
	WT_Column(WT_INPUT, WT_FIRST, WT_LAST, "", "", "", "", pv);
	WT_EndLine();
}

void WT_EditRow(PageVar *pv, const char* typeOverride, const char* ExtraAttrs){
	String typeStr;
	const char* typeToUse;
	if (typeOverride && *typeOverride) {
		typeToUse = typeOverride;
	} else {
		typeStr   = pv->Var->GetTypeStr();
		typeToUse = typeStr.c_str();
	}
	String idL   = pv->Var->GetID("L");
	String idIn  = pv->Var->GetID("");
	String nameStr = pv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text",
	          pv->Var->GetDescription().c_str(), idL.c_str(),
	          "", pv);
	WT_Column(WT_INPUT, WT_FIRST, WT_LAST, typeToUse,
	          nameStr.c_str(), idIn.c_str(),
	          pv->Var->GetFStyle().c_str(), pv, ExtraAttrs);
	WT_EndLine();
}

void WT_CheckEditRow(PageVar *cbPv, PageVar *valPv, const char* typeOverride){
	String typeStr;
	const char* typeToUse;
	if (typeOverride && *typeOverride) {
		typeToUse = typeOverride;
	} else {
		typeStr   = valPv->Var->GetTypeStr();
		typeToUse = typeStr.c_str();
	}
	String idL   = valPv->Var->GetID("L");
	String idCb  = cbPv->Var->GetID("");
	String idIn  = valPv->Var->GetID("");
	String nameStr = valPv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text",
	          valPv->Var->GetDescription().c_str(), idL.c_str(),
	          "", valPv);
	WT_Column(WT_INPUT, WT_FIRST, WT_MORE, "checkbox",
	          cbPv->Var->GetName().c_str(), idCb.c_str(),
	          cbPv->Var->GetFStyle().c_str(), cbPv);
	WT_Column(WT_INPUT, WT_NEXT,  WT_LAST, typeToUse,
	          nameStr.c_str(), idIn.c_str(),
	          valPv->Var->GetFStyle().c_str(), valPv);
	WT_EndLine();
}

void WT_RightCheckRow(PageVar *pv){
	String idL  = pv->Var->GetID("L");
	String idIn = pv->Var->GetID("");
	String nameStr = pv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text",
	          pv->Var->GetDescription().c_str(), idL.c_str(), "", pv);
	WT_Column(WT_INPUT, WT_FIRST, WT_MORE, "hidden", " ", "", "", pv);
	WT_Column(WT_INPUT, WT_NEXT,  WT_LAST, "checkbox",
	          nameStr.c_str(), idIn.c_str(), pv->Var->GetFStyle().c_str(), pv);
	WT_EndLine();
}

void WT_StartPage(){
	appendToPageBuf("<script>const parameters = [");
}

void WT_EndPage(){
	appendToPageBuf("]; createLabelInputTable(parameters); </script>");
}

void WT_EndPageClass(const char* cls){
	appendToPageBuf("]; createLabelInputTable(parameters,'");
	appendToPageBuf(cls);
	appendToPageBuf("'); </script>");
}

void WT_Header(const char* Header){
	appendToPageBuf("{name: \""); appendToPageBuf(Header); appendToPageBuf("\", labels: [], inputs: [],},");
}

void WT_SectionHeader(const char* Header){
	appendToPageBuf("{sectionHeader: \""); appendToPageBuf(Header); appendToPageBuf("\"},");
}

void WT_StartLine(){
	appendToPageBuf("{");
}

void WT_EndLine(){
	appendToPageBuf("},");
}

void WT_StartLabel(){
	appendToPageBuf("labels: [{");
}

void WT_StartLabelEmpty(){
	appendToPageBuf("labels:[],");
}

void WT_StartInput(){
	appendToPageBuf("inputs:[{");
}

void WT_StartInputEmpty(){
	appendToPageBuf("inputs:[],");
}

void WT_NextColumn(){
	appendToPageBuf("},{");
}

void WT_EndCell(){
	appendToPageBuf("},],");
}

void WT_TypeCell(const char* Type){
	appendToPageBuf("type: \""); appendToPageBuf(Type); appendToPageBuf("\",");
}

void WT_TextLabel(const char* Txt){
	appendToPageBuf("text: \""); appendToPageBuf(Txt); appendToPageBuf("\",");
}

void WT_ValueInput(const char* Val){
	appendToPageBuf("value: \""); appendToPageBuf(Val); appendToPageBuf("\",");
}

void WT_IDCell(const char* ID){
	appendToPageBuf("id: \""); appendToPageBuf(ID); appendToPageBuf("\",");
}

void WT_AttrsStart(){
	appendToPageBuf("attributes:[");
}

void WT_Attr(const char* n, const char* v){
	appendToPageBuf("{n:\""); appendToPageBuf(n);
	appendToPageBuf("\",v:\""); appendToPageBuf(v);
	appendToPageBuf("\"},");
}

void WT_AttrsEnd(){
	appendToPageBuf("],");
}

void WT_OptionsStart(){
	appendToPageBuf("options: [");
}

void WT_OptionsValues(const char* AVal){
	appendToPageBuf("\""); appendToPageBuf(AVal); appendToPageBuf("\",");
}

void WT_OptionsEnd(){
	appendToPageBuf("],");
}
