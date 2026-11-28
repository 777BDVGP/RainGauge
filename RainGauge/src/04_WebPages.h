#pragma once

// =============================================================================
// 04_WebPages.h — Web Server, WebSocket, Page Elements & HTML Templates
// =============================================================================

// ─── WebSocket Variable ID Base Addresses ────────────────────────────────────
#define ID_HOME       50    // Home page      — variables  (50+)
#define ID_SETTINGS  100    // Settings page  — variables  (100+)

#define WS_CMD_QUEUE    4      // WebSocket commands waiting for loop()
#define WS_CMD_MAX_LEN  2048   // longer frames are dropped (SETTINGS UPDATE is ~1 KB)

// ─── Web Server & WebSocket ───────────────────────────────────────────────────
inline AsyncWebServer server(80);
inline AsyncWebSocket ws("/ws");

// ─── HTML Colour Style Strings ────────────────────────────────────────────────
inline const char COLOR_DEFAULT[] = "color:#227c91";
inline const char COLOR_RED[]     = "color:#ff0000";
inline const char COLOR_BLUE[]    = "color:#3366ff";
inline const char COLOR_GREEN[]   = "color:#33cc33";
inline const char COLOR_VIOLET[]  = "color:#ff00ff";

// ─── Status Header Message Constants (FA_MESS_*) ─────────────────────────────
#define FA_MESS_01    1
#define FA_MESS_02    2
#define FA_MESS_03    3
#define FA_MESS_04    4
#define FA_MESS_06    6
#define FA_MESS_07    7
#define FA_MESS_08    8
#define FA_MESS_09    9
#define FA_MESS_10   10
#define FA_MESS_11   11
#define FA_MESS_RED  12
#define FA_MESS_GRN  13
#define FA_MESS_RAIN 14
#define FA_MESS_TEMP 15
#define FA_MESS_WB   16
#define FA_MESS_LOGIN 17

inline int Delmess = FA_MESS_06;

// ─── WebSocket Connection Tracking ────────────────────────────────────────────
inline uint32_t Lastnum       = 0;
inline bool     LastnumUpdated = true;

// Header - Required on all pages (holds title, time, action & style)
inline char webHeadPage[] PROGMEM = R"=====(
<html>
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<link rel="icon" href="data:,">
	<style>
		*{box-sizing:border-box;margin:0;padding:0}
		:root{--ok:#16a34a;--warn:#c2410c;--alert:#92400e;--err:#b91c1c;--mute:#475569}
		body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#f8fafc;color:#0f172a;max-width:600px;margin:0 auto;padding:8px;font-size:13px}
		h2{font-size:20px;font-weight:800;letter-spacing:1px;color:#0284c7;text-align:center;padding:10px 0 2px}
		h4{font-size:12px;color:#475569;text-align:center;margin:2px 0}
		p{text-align:left;margin-left:8px;margin-top:0;margin-bottom:0}
		.nav{display:flex;flex-wrap:wrap;border:1px solid #0284c7;border-radius:6px;overflow:hidden;margin:6px 0}
		.nav a{flex:1 1 auto;min-width:55px;padding:10px 4px;background:#bae6fd;color:#0369a1;text-align:center;text-decoration:none;font-size:12px;font-weight:700;border-right:1px solid #0284c7;white-space:nowrap}
		.nav a:last-child{border-right:none}
		.nav a:hover:not(.act):not(.warn){background:#7dd3fc}
		.nav a.act{background:#0284c7;color:#fff}
		.nav a.warn{background:#dc2626;color:#fff}
		.nav a.warn:hover{background:#b91c1c}
		.nav.sub{margin:-2px 0 6px;border-color:#7dd3fc}
		.nav.sub a{padding:6px 4px;font-size:11px;background:#e0f2fe;border-right-color:#7dd3fc}
		.nav.sub a.act{background:#38bdf8;color:#fff}
		@media(max-width:400px){.nav a{font-size:11px;padding:10px 2px;min-width:0}.nav.sub a{font-size:10px;padding:6px 2px}}
		table{width:100%;border-collapse:collapse;font-size:13px;margin:4px 0}
		.hd td,.hd th{color:#64748b;font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.5px;border-bottom:2px solid #cbd5e1;padding:4px 6px;text-align:left}
		.hd th:last-child,.hd td:last-child{text-align:right}
		td,th{padding:5px 6px;border-bottom:1px solid #e2e8f0;word-break:break-word;overflow-wrap:break-word}
		table.compact td,table.compact th{padding:3px 4px}table.compact input[type=text],table.compact input[type=number],table.compact input[type=time],table.compact input[type=date],table.compact select{padding:1px 2px;font-size:11px}
		tr:not(.hd) td:first-child{width:100%}
		table.susp th:not(:first-child){min-width:65px;text-align:center}
		table.susp tr:not(.hd) td:not(:first-child){min-width:65px;text-align:center}
		tr:not(.hd) td:last-child{text-align:right;white-space:nowrap;padding-right:0}
		tr:not(.hd) td:last-child label{display:inline-block;vertical-align:middle}
		tr:not(.hd) th:first-child{width:100%}
		tr:not(.hd) th:last-child:not(:first-child){text-align:right;white-space:nowrap;padding-right:0}
		tr:not(.hd) th:last-child:not(:first-child) p{text-align:right;margin-left:0}
		table.grid tr:not(.hd) td:first-child{width:auto;text-align:left}
		table.grid tr:not(.hd) td:not(:first-child){padding-left:10px;padding-right:10px;text-align:center}
		table.grid tr:not(.hd) td:last-child{text-align:center;white-space:normal;padding-right:10px}
		table.grid tr:not(.hd) td{text-align:center}
		table.grid tr th{text-align:center;white-space:nowrap;padding-left:10px;padding-right:10px}
		table.grid tr th:first-child{text-align:left;white-space:normal;padding-left:6px;padding-right:6px}
		table.grid tr th:last-child:not(:first-child){text-align:center;white-space:nowrap;padding-right:10px}
		table.grid .hd th{text-align:center}
		table.grid .hd th:first-child{text-align:left}
		table.grid .hd th:last-child{text-align:center}
		table.info tr:not(.hd) td:first-child,table.info tr:not(.hd) th:first-child{width:45%}
		table.info tr:not(.hd) td:last-child{white-space:normal;text-align:right}
		input[type=time],input[type=text],input[type=number],input[type=date]{background:#fff;border:1px solid #cbd5e1;border-radius:4px;color:#0f172a;padding:3px 5px;font-size:12px;width:120px;max-width:45vw}
		select{background:#fff;border:1px solid #cbd5e1;border-radius:4px;color:#0f172a;padding:3px 5px;font-size:12px;width:120px;max-width:45vw}
		input[type=checkbox]{accent-color:#0284c7;cursor:pointer;width:16px;height:16px}
		input[type=time]:focus,input[type=text]:focus,input[type=number]:focus,input[type=date]:focus,select:focus{outline:1px solid #0284c7}
		input[type=button],input[type=submit]{background:#0284c7;border:none;color:#fff;padding:5px 14px;font-size:13px;cursor:pointer;border-radius:4px}
		input[type=button]:hover,input[type=submit]:hover{background:#0369a1}
		.btn{display:inline-block;padding:7px 18px;border-radius:5px;font-size:13px;font-weight:700;border:none;cursor:pointer;margin:4px;text-decoration:none;font-family:inherit;line-height:1.2;vertical-align:middle}
		.btn-bl{background:#1d4ed8;color:#fff}.btn-bl:hover{background:#2563eb}
		.btn-rd{background:#dc2626;color:#fff}.btn-rd:hover{background:#ef4444}
		.btn-or{background:#d97706;color:#fff}.btn-or:hover{background:#f59e0b}
		.btn-gr{background:#15803d;color:#fff}.btn-gr:hover{background:#16a34a}
		.btns{text-align:center;padding:8px 0}
		label{color:#0f172a}
	</style>
</head>

<body onload="javascript:init()">

)=====";

// WebSocket / page client script — served once at /wss.js (handleWssJs) and
// pulled into every page via webScriptTag so the browser caches it.
// NOTE: pure JavaScript, no <script> wrapper.
inline char webSocketScript[] PROGMEM = R"=====(
	function init() {
		StartWebSocket();
	}

	function setOtaStatus(msg, color) {
		var el = document.getElementById("ota_status");
		if (el) { el.value = msg; el.style.color = color; }
	}

	var Socket;
	var wsReconnectPending = false;
	function scheduleWsReconnect() {
		// One pending reconnect at a time. onerror and onclose both fire on a
		// drop, and a slow/again-dropping ESP must not be hammered — a fixed 3 s
		// gap keeps this from becoming a reconnect storm that starves the device.
		if (wsReconnectPending) return;
		wsReconnectPending = true;
		setTimeout(function () { wsReconnectPending = false; StartWebSocket(); }, 3000);
	}
	function StartWebSocket() {
		Socket = new WebSocket('ws://' + window.location.host + '/ws');

		Socket.onerror = function(event) {
			console.log("Socket.onerror");
			scheduleWsReconnect();
		};

		Socket.onclose = function(event) {
			console.log("Socket.onclose");
			scheduleWsReconnect();              // device rebooted / dropped — retry, gently
		};

		// Update web page elements from ESP32
		Socket.onmessage = function(event) {
			const jsonObject = JSON.parse(event.data);
			if (jsonObject.type === "ota_start")    { setOtaStatus("Upload starting...",                     "orange"); return; }
			if (jsonObject.type === "ota_progress") { setOtaStatus("Uploading... " + jsonObject.percent + "%", "orange"); return; }
			if (jsonObject.type === "ota_end")      { setOtaStatus("Busy rebooting...", "red"); setTimeout(function(){ location.reload(); }, 5000); return; }
			if (jsonObject.type === "ota_error")    { setOtaStatus("Update failed!",                         "red");    return; }
			if (jsonObject.type === "reload")       { setTimeout(function(){ location.reload(); }, 1500); return; }
			var ElementAtt;
			for (const [key, value] of Object.entries(jsonObject)) {
				if (key === "Attribute") {
					const attrName = jsonObject["AttributeName"] || "style";
					if (ElementAtt !== null) { ElementAtt.setAttribute(attrName, value); }
				} else if (key === "AttributeName") {
					// consumed by the "Attribute" handler above
				} else {
					const Element = document.getElementById(key);
					if (Element === null) { ElementAtt = null; continue; }
					if (Element.tagName === 'SELECT') {
						Element.value = value;
						ElementAtt = Element;
					} else {
						if (Element !== null) {
							if (Element.type === "checkbox") {
								Element.checked = (value === "true");
								ElementAtt = null;
							} else {
								Element.value = value;
								Element.innerHTML = value;
								ElementAtt = Element;
							}
						} else {
							ElementAtt = null;
						}
					}
				}
			}
		}
	}

	// Read all inputs and send to ESP32 via WebSocket
	function UpdatePage() {
		const jsonObject = {};
		for (const input of document.querySelectorAll("input")) {
			if (input.closest(".noupd")) continue;   // e.g. SETTINGS "Add a network" (own button, not UPDATE)
			jsonObject[input.id] = (input.type === "checkbox") ? (input.checked ? "true" : "false") : input.value;
		}
		for (const select of document.querySelectorAll("select")) {
			jsonObject[select.id] = select.value;
		}
		Socket.send(JSON.stringify(jsonObject));
	}

	function CalCmd(c) {
		// Calibration page buttons — wait for the socket if it isn't open yet.
		if (!Socket || Socket.readyState !== 1) { setTimeout(function () { CalCmd(c); }, 300); return; }
		Socket.send(JSON.stringify({ CAL: c }));
	}

	function SimCmd(c) {
		// Simulation page buttons — wait for the socket if it isn't open yet.
		if (!Socket || Socket.readyState !== 1) { setTimeout(function () { SimCmd(c); }, 300); return; }
		Socket.send(JSON.stringify({ SIM: c }));
	}

	function ButtonReboot() {
		Socket.send(JSON.stringify({"REBOOT": "YES"}));
	}

	// Build label+input table below header
	function createLabelInputTable(parameters, tableClass) {
		const tableElement = document.createElement("table");
		if (tableClass) tableElement.className = tableClass;
		const headerRow = document.createElement("tr");

		for (const parameter of parameters) {
			if (!parameter.name) continue;
			const headerCell = document.createElement("th");
			headerCell.textContent = parameter.name;
			headerRow.appendChild(headerCell);
		}

		if (headerRow.textContent.trim() !== '') {
			// Value header right-aligned, over the right-aligned values.
			const ths = headerRow.querySelectorAll('th');
			ths.forEach((cell, i) => { cell.style.textAlign = (i > 0 && i === ths.length - 1) ? 'right' : 'left'; });
			tableElement.appendChild(headerRow);
		}

		for (const parameter of parameters) {
			if (parameter.name !== undefined) continue;
			if (parameter.sectionHeader !== undefined) {
				const row = document.createElement("tr");
				const cell = document.createElement("td");
				cell.colSpan = 2;
				cell.textContent = parameter.sectionHeader;
				cell.style.cssText = "font-weight:bold;padding:4px 6px;text-align:left;";
				row.appendChild(cell);
				tableElement.appendChild(row);
				continue;
			}
			const row = document.createElement("tr");

			for (const label of parameter.labels) {
				const cell = document.createElement("td");
				const labelElement = document.createElement("label");
				labelElement.textContent = label.text;
				labelElement.id = label.id;
				for (const a of (label.attributes || [])) { labelElement.setAttribute(a.n, a.v); }
				cell.appendChild(labelElement);
				row.appendChild(cell);
			}

			for (const input of parameter.inputs) {
				const cell = document.createElement("td");

				if (input.type === "select") {
					const inputElement = document.createElement("select");
					inputElement.id = input.id;
					for (const a of (input.attributes || [])) { inputElement.setAttribute(a.n, a.v); }
					for (const optionText of input.options) {
						const option = document.createElement("option");
						option.value = optionText;
						option.textContent = optionText;
						inputElement.appendChild(option);
					}
					inputElement.value = input.value;
					cell.appendChild(inputElement);
				} else {
					const inputElement = document.createElement("input");
					inputElement.type = input.type;
					inputElement.id = input.id;
					inputElement.value = input.value;
					if (input.type === "checkbox") {
						inputElement.checked = (input.value === "true");
					}
					for (const a of (input.attributes || [])) { inputElement.setAttribute(a.n, a.v); }
					cell.appendChild(inputElement);
				}

				row.appendChild(cell);
			}

			tableElement.appendChild(row);
		}

		// Rows with fewer columns than the widest one (e.g. SETTINGS has a
		// 3-cell "Scheduled Reboot" row): stretch the last cell over the rest,
		// so every value lines up at the table's right edge.
		const rows = [...tableElement.rows];
		const span = r => [...r.cells].reduce((n, c) => n + c.colSpan, 0);
		const maxCols = Math.max(...rows.map(span));
		rows.forEach(r => { const n = span(r); if (n && n < maxCols) r.cells[r.cells.length - 1].colSpan += maxCols - n; });

		document.body.appendChild(tableElement);
	}

)=====";

// Tag emitted by every page instead of inlining webSocketScript.  The ?v=
// number is the cache-buster — bump it whenever webSocketScript changes so
// browsers re-fetch /wss.js.
#define webScriptTag "<script src='/wss.js?v=10'></script>\n"

inline char webHTMLEnd[] PROGMEM = R"=====(
</body>
</html>
)=====";

inline char webUpateButton[] PROGMEM = R"=====(
	<div><br></div>
	<div>
		<b><a><input type="button" id="Update" onclick="UpdatePage()" value="UPDATE" style="font-size:14px"></a></b>
	</div>
)=====";

// ─── SelectOptions ────────────────────────────────────────────────────────────
class SelectOptions {
public:
	String Option = "";
};

// ─── PageElement Variable Type Tags ──────────────────────────────────────────
#define VTYPE_STRING 1
#define VTYPE_DOUBLE 2
#define VTYPE_FLOAT  3
#define VTYPE_INT    4
#define VTYPE_BOOL   5

// =============================================================================
// PageElement
// =============================================================================
class PageElement {
public:
	void* operator new(size_t sz)  { void* p = ps_malloc(sz); return p ? p : malloc(sz); }
	void  operator delete(void* p) { free(p); }

	PageElement(String* InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(double* InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(float*  InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(int*    InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(bool*   InName, String Desc, String FStyle, String* VStyle, int ID);
	~PageElement();

	void   SetName(String InName);
	String GetName();
	String GetNameLast();
	void   UpdateLast();
	bool   HasChanged();

	const String& GetVarStyle();
	const String& GetDescription();
	void          SetDescription(String desc);
	const String& GetFStyle();

	String GetTypeStr();
	String GetID(const char* Type);

	void          SetStringToSelect(int Options);
	void          AddOption(String Option);
	const String& GetOption(int Idx);
	int           GetOptionCount();

	void SetTextAlign(String align);
	void SetRange(float mn, float mx, float step = NAN);
	void SetPlaceholder(String ph);
	void SetReadOnly(bool ro = true);

	const String& GetAttrTextAlign();
	bool          HasAttrMin();
	bool          HasAttrMax();
	bool          HasAttrStep();
	float         GetAttrMin();
	float         GetAttrMax();
	float         GetAttrStep();
	const String& GetAttrPlaceholder();
	bool          GetAttrReadOnly();

private:
	String* NameS;
	double* NameD;
	float*  NameF;
	int*    NameI;
	bool*   NameB;
	int     Type;

	String NameSLast;
	double NameDLast;
	float  NameFLast;
	int    NameILast;
	String NameBLast;

	String  VarDesc  = "";
	String  FixStyle = "";
	String* VarStyle;
	int     VarID = 0;
	bool    NameStringSelect = false;
	int     OptionCount = 0;
	int     OptionIndex = 0;
	SelectOptions* OptionList;

	String AttrTextAlign   = "";
	float  AttrMin         = NAN;
	float  AttrMax         = NAN;
	float  AttrStep        = NAN;
	String AttrPlaceholder = "";
	bool   AttrReadOnly    = false;
};

inline PageElement::PageElement(String* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameS = InName; NameSLast = *NameS;
	Type = VTYPE_STRING; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(double* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameD = InName; NameDLast = *NameD;
	Type = VTYPE_DOUBLE; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(float* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameF = InName; NameFLast = *NameF;
	Type = VTYPE_FLOAT; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(int* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameI = InName; NameILast = *NameI;
	Type = VTYPE_INT; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(bool* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameB = InName; NameBLast = (*NameB ? "true" : "false");
	Type = VTYPE_BOOL; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::~PageElement() {
	delete[] OptionList;
}

inline void PageElement::SetName(String InName) {
	switch (Type) {
		case VTYPE_STRING: *NameS = InName;              break;
		case VTYPE_DOUBLE: *NameD = InName.toDouble();   break;
		case VTYPE_FLOAT:  *NameF = InName.toFloat();    break;
		case VTYPE_INT:    *NameI = InName.toInt();      break;
		case VTYPE_BOOL:   *NameB = (InName == "true");  break;
	}
}

inline String PageElement::GetName() {
	switch (Type) {
		case VTYPE_STRING: return *NameS;
		case VTYPE_DOUBLE: return String(*NameD);
		case VTYPE_FLOAT:  return String(*NameF);
		case VTYPE_INT:    return String(*NameI);
		case VTYPE_BOOL:   return (*NameB ? "true" : "false");
	}
	return "";
}

inline const String& PageElement::GetVarStyle() {
	return *VarStyle;
}

inline String PageElement::GetNameLast() {
	switch (Type) {
		case VTYPE_STRING: return NameSLast;
		case VTYPE_DOUBLE: return String(NameDLast);
		case VTYPE_FLOAT:  return String(NameFLast);
		case VTYPE_INT:    return String(NameILast);
		case VTYPE_BOOL:   return NameBLast;
	}
	return "";
}

inline void PageElement::UpdateLast() {
	switch (Type) {
		case VTYPE_STRING: NameSLast = *NameS;                         break;
		case VTYPE_DOUBLE: NameDLast = *NameD;                         break;
		case VTYPE_FLOAT:  NameFLast = *NameF;                         break;
		case VTYPE_INT:    NameILast = *NameI;                         break;
		case VTYPE_BOOL:   NameBLast = (*NameB ? "true" : "false");    break;
	}
}

inline const String& PageElement::GetDescription()         { return VarDesc;  }
inline void          PageElement::SetDescription(String d) { VarDesc = d;      }
inline const String& PageElement::GetFStyle()              { return FixStyle;  }

inline bool PageElement::HasChanged() {
	switch (Type) {
		case VTYPE_STRING: return *NameS != NameSLast;
		case VTYPE_DOUBLE: return *NameD != NameDLast;
		case VTYPE_FLOAT:  return *NameF != NameFLast;
		case VTYPE_INT:    return *NameI != NameILast;
		case VTYPE_BOOL:   return NameBLast != (*NameB ? "true" : "false");
	}
	return false;
}

inline String PageElement::GetID(const char* Type) {
	if (Type && *Type) { String r = Type; r += VarID; return r; }
	return String(VarID);
}

inline void PageElement::SetStringToSelect(int Options) {
	NameStringSelect = true;
	OptionCount = Options;
	OptionList = new SelectOptions[OptionCount];
}

inline void PageElement::AddOption(String Option) {
	OptionList[OptionIndex].Option = Option;
	if (OptionIndex < (OptionCount - 1)) OptionIndex++;
}

inline const String& PageElement::GetOption(int Idx)  { return OptionList[Idx].Option; }
inline int            PageElement::GetOptionCount()   { return OptionCount; }

inline String PageElement::GetTypeStr() {
	switch (Type) {
		case VTYPE_STRING: return (NameStringSelect ? "select" : "text");
		case VTYPE_DOUBLE:
		case VTYPE_FLOAT:
		case VTYPE_INT:    return "number";
		case VTYPE_BOOL:   return "checkbox";
	}
	return "";
}

inline void   PageElement::SetTextAlign(String align)               { AttrTextAlign = align; }
inline void   PageElement::SetRange(float mn, float mx, float step) { AttrMin = mn; AttrMax = mx; AttrStep = step; }
inline void   PageElement::SetPlaceholder(String ph)                { AttrPlaceholder = ph; }
inline void   PageElement::SetReadOnly(bool ro)                     { AttrReadOnly = ro; }

inline const String& PageElement::GetAttrTextAlign()   { return AttrTextAlign;   }
inline bool          PageElement::HasAttrMin()         { return !isnan(AttrMin); }
inline bool          PageElement::HasAttrMax()         { return !isnan(AttrMax); }
inline bool          PageElement::HasAttrStep()        { return !isnan(AttrStep); }
inline float         PageElement::GetAttrMin()         { return AttrMin;         }
inline float         PageElement::GetAttrMax()         { return AttrMax;         }
inline float         PageElement::GetAttrStep()        { return AttrStep;        }
inline const String& PageElement::GetAttrPlaceholder() { return AttrPlaceholder; }
inline bool          PageElement::GetAttrReadOnly()    { return AttrReadOnly;    }

// ─── PageVar ──────────────────────────────────────────────────────────────────
struct PageVar {
	PageElement* Var     = nullptr;
	bool         deleted = true;
};
