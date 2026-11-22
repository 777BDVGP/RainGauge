// =============================================================================
// 12_Settings.cpp — System Parameters & Settings Page
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// AllParams — Object Factory & Persistence
// =============================================================================

void CreateAllParams(){
	Params = new AllParams;
	Params->ReadData();
	Params->WriteData();
	ApplyRainSettings();
	MapSettingsPage();
}

AllParams::AllParams()  {}
AllParams::~AllParams(){}

void AllParams::WriteData()
{
	String StrFileName = "/AllParams.json";
	File ChannelDataFile = SPIFFS.open(StrFileName, "w");
	DynamicJsonDocument doc(1024);

	doc["SysName"]          = SysName;
	doc["LogGeneralEvents"] = LogGeneralEvents;
	doc["ShowSysInfo"]      = ShowSysInfo;
	doc["ShowInsideTemp"]   = ShowInsideTemp;
	doc["RebootEnable1"]    = RebootEnable1;
	doc["RebootTime1"]      = RebootTime1;

	doc["FunnelDiaMm"]      = FunnelDiaMm;
	doc["MlPerTip"]         = MlPerTip;
	doc["TipDebounceMs"]    = TipDebounceMs;
	doc["CalWaterMl"]       = CalWaterMl;
	doc["CalDate"]          = CalDate;
	doc["IrTestMin"]        = IrTestMin;

	doc["IrriSkipMm"]       = IrriSkipMm;
	doc["IrriSkipHours"]    = IrriSkipHours;

	doc["TimeZone"]         = TimeZone;
	doc["IrriMode"]         = IrriMode;
	doc["Latitude"]         = Latitude;
	doc["CropKc"]           = CropKc;
	doc["SoilType"]         = SoilType;
	doc["RootDepthMm"]      = RootDepthMm;
	doc["DepletionPct"]     = DepletionPct;
	doc["MinEffRainMm"]     = MinEffRainMm;
	doc["IrriDepthMm"]      = IrriDepthMm;
	doc["IrriTime"]         = IrriTime;
	doc["ControllerName"]   = ControllerName;

	if (ChannelDataFile)
	{
		serializeJsonPretty(doc, ChannelDataFile);
		ChannelDataFile.close();
	}
}

void AllParams::ReadData()
{
	String StrFileName = "/AllParams.json";
	File ChannelDataFile = SPIFFS.open(StrFileName, "r");
	if (ChannelDataFile)
	{
		DynamicJsonDocument doc(1024);
		auto error = deserializeJson(doc, ChannelDataFile);
		if (!error)
		{
			if ( !doc["SysName"].isNull()          ){ SysName          = doc["SysName"].as<String>(); }
			if ( !doc["LogGeneralEvents"].isNull() ){ LogGeneralEvents = doc["LogGeneralEvents"].as<bool>(); }
			if ( !doc["ShowSysInfo"].isNull()      ){ ShowSysInfo      = doc["ShowSysInfo"].as<bool>(); }
			if ( !doc["ShowInsideTemp"].isNull()   ){ ShowInsideTemp   = doc["ShowInsideTemp"].as<bool>(); }
			if ( !doc["RebootEnable1"].isNull()    ){ RebootEnable1    = doc["RebootEnable1"].as<bool>(); }
			if ( !doc["RebootTime1"].isNull()      ){ RebootTime1      = doc["RebootTime1"].as<String>(); }

			if ( !doc["FunnelDiaMm"].isNull()      ){ FunnelDiaMm      = doc["FunnelDiaMm"].as<float>(); }
			if ( !doc["MlPerTip"].isNull()         ){ MlPerTip         = doc["MlPerTip"].as<float>(); }
			if ( !doc["TipDebounceMs"].isNull()    ){ TipDebounceMs    = doc["TipDebounceMs"].as<int>(); }
			if ( !doc["CalWaterMl"].isNull()       ){ CalWaterMl       = doc["CalWaterMl"].as<int>(); }
			if ( !doc["CalDate"].isNull()          ){ CalDate          = doc["CalDate"].as<String>(); }
			if ( !doc["IrTestMin"].isNull()        ){ IrTestMin        = doc["IrTestMin"].as<int>(); }

			if ( !doc["IrriSkipMm"].isNull()       ){ IrriSkipMm       = doc["IrriSkipMm"].as<float>(); }
			if ( !doc["IrriSkipHours"].isNull()    ){ IrriSkipHours    = doc["IrriSkipHours"].as<int>(); }

			if ( !doc["TimeZone"].isNull()         ){ TimeZone         = doc["TimeZone"].as<String>(); }
			if ( !doc["IrriMode"].isNull()         ){ IrriMode         = doc["IrriMode"].as<String>(); }
			if ( !doc["Latitude"].isNull()         ){ Latitude         = doc["Latitude"].as<float>(); }
			if ( !doc["CropKc"].isNull()           ){ CropKc           = doc["CropKc"].as<float>(); }
			if ( !doc["SoilType"].isNull()         ){ SoilType         = doc["SoilType"].as<String>(); }
			if ( !doc["RootDepthMm"].isNull()      ){ RootDepthMm      = doc["RootDepthMm"].as<int>(); }
			if ( !doc["DepletionPct"].isNull()     ){ DepletionPct     = doc["DepletionPct"].as<int>(); }
			if ( !doc["MinEffRainMm"].isNull()     ){ MinEffRainMm     = doc["MinEffRainMm"].as<float>(); }
			if ( !doc["IrriDepthMm"].isNull()      ){ IrriDepthMm      = doc["IrriDepthMm"].as<float>(); }
			if ( !doc["IrriTime"].isNull()         ){ IrriTime         = doc["IrriTime"].as<String>(); }
			if ( !doc["ControllerName"].isNull()   ){ ControllerName   = doc["ControllerName"].as<String>(); }
		}
		else
		{
			SaveGenReport F(" File Error open File AllParams.json");
		}
		ChannelDataFile.close();
	}
}

// =============================================================================
// Settings Page — PageVar Mapping
// =============================================================================

void MapSettingsPage(){
	SettingsList = new PageVar[PAGE_VAR_SETTINGS];
	for (int i=0; i < PAGE_VAR_SETTINGS; i++)
	{
		switch(i)
		{
			case SET_SYSNAME:
				SettingsList[i].Var = new PageElement(&Params->SysName, F("System Name"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_LOG_GENERAL:
				SettingsList[i].Var = new PageElement(&Params->LogGeneralEvents, F("Log General Events"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_REBOOT_EN1:
				SettingsList[i].Var = new PageElement(&Params->RebootEnable1, F("Enable Reboot"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_REBOOT_TIME1:
				SettingsList[i].Var = new PageElement(&Params->RebootTime1, F("Scheduled Reboot @ Time"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_FUNNEL_DIA:
				SettingsList[i].Var = new PageElement(&Params->FunnelDiaMm, F("Funnel Diameter (mm)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(50, 400, 0.1);
				break;
			case SET_ML_PER_TIP:
				SettingsList[i].Var = new PageElement(&Params->MlPerTip, F("Water per Tip (ml) - calibrated"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(0.5, 50, 0.01);
				break;
			case SET_TIP_DEBOUNCE:
				SettingsList[i].Var = new PageElement(&Params->TipDebounceMs, F("Tip Debounce (ms)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(20, 2000, 10);
				break;
			case SET_CAL_WATER:
				SettingsList[i].Var = new PageElement(&Params->CalWaterMl, F("Calibration Water (ml)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(100, 2000, 10);
				break;
			case SET_IRRI_SKIP_MM:
				SettingsList[i].Var = new PageElement(&Params->IrriSkipMm, F("Skip Irrigation if Rain >= (mm)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(0, 200, 0.1);
				break;
			case SET_IRRI_MODE:
				SettingsList[i].Var = new PageElement(&Params->IrriMode, F("Irrigation Answer Method"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetStringToSelect(3);   // must match the number of AddOption calls
				SettingsList[i].Var->AddOption(IRRI_MODE_WB);
				SettingsList[i].Var->AddOption(IRRI_MODE_SIMPLE);
				SettingsList[i].Var->AddOption(IRRI_MODE_NONE);
				break;
			case SET_LATITUDE:
				SettingsList[i].Var = new PageElement(&Params->Latitude, F("Latitude (deg, South = -)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(-60, 60, 0.01);
				break;
			case SET_CROP_KC:
				SettingsList[i].Var = new PageElement(&Params->CropKc, F("Garden Factor Kc (beds 0.6, lawn 0.8)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(0.2, 1.3, 0.05);
				break;
			case SET_SOIL_TYPE:
				SettingsList[i].Var = new PageElement(&Params->SoilType, F("Soil Type"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetStringToSelect(3);
				SettingsList[i].Var->AddOption(SOIL_SAND);
				SettingsList[i].Var->AddOption(SOIL_LOAM);
				SettingsList[i].Var->AddOption(SOIL_CLAY);
				break;
			case SET_ROOT_DEPTH:
				SettingsList[i].Var = new PageElement(&Params->RootDepthMm, F("Root Depth (mm)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(50, 1000, 10);
				break;
			case SET_DEPLETION_PCT:
				SettingsList[i].Var = new PageElement(&Params->DepletionPct, F("Irrigate when Soil Water Used (%)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(10, 90, 5);
				break;
			case SET_MIN_EFF_RAIN:
				SettingsList[i].Var = new PageElement(&Params->MinEffRainMm, F("Ignore Daily Rain below (mm)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(0, 20, 0.1);
				break;
			case SET_IRRI_DEPTH:
				SettingsList[i].Var = new PageElement(&Params->IrriDepthMm, F("One Irrigation Adds (mm)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(1, 50, 0.5);
				break;
			case SET_IRRI_TIME:
				SettingsList[i].Var = new PageElement(&Params->IrriTime, F("Controller Irrigates At"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_SHOW_SYSINFO:
				SettingsList[i].Var = new PageElement(&Params->ShowSysInfo, F("Show Memory & Tasks on ABOUT"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_SHOW_INSIDE:
				SettingsList[i].Var = new PageElement(&Params->ShowInsideTemp, F("Show Inside Temperature (HOME, REPORTS)"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_CTRL_NAME:
				SettingsList[i].Var = new PageElement(&Params->ControllerName, F("Irrigation Controller Name"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_IR_TEST_MIN:
				SettingsList[i].Var = new PageElement(&Params->IrTestMin, F("IR Self-Test every (min, 0 = off)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(0, 1440, 1);
				break;
			case SET_IRRI_SKIP_HRS:
				SettingsList[i].Var = new PageElement(&Params->IrriSkipHours, F("...in the Last (hours)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetRange(1, 240, 1);
				break;
			case SET_TIME_ZONE:
				SettingsList[i].Var = new PageElement(&Params->TimeZone, F("Time Zone (daylight saving automatic)"), "", &SetStyle, i+ID_SETTINGS);
				SettingsList[i].Var->SetStringToSelect(TZ_OPTION_COUNT);
				for (int k = 0; k < TZ_OPTION_COUNT; k++) SettingsList[i].Var->AddOption(TZ_OPTIONS[k].label);
				break;
		}
	}
}

// =============================================================================
// Settings Web Page — Route Handler
// =============================================================================

void handleSettings(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	WT_SETUP_Settings();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendMainPageSel(PS_SETTINGS);
	appendHtmlStr(webScriptTag);
	appendHtmlBuf(PageBuf, getPageBufLen());
	// One row of buttons.  <button>, not <input type=button>: UpdatePage()
	// sends every <input> on the page, and these must not be among them.
	appendHtmlStr("<div class='btns'>"
	              "<button class='btn btn-bl' onclick='UpdatePage()'>UPDATE</button>"
	              "<a class='btn btn-gr' href='/calibrate'>CALIBRATION</a>"
	              "<a class='btn btn-or' href='/simulate'>SIMULATION</a>"
	              "<button class='btn btn-rd' onclick='ButtonReboot()'>REBOOT</button>"
	              "</div>");
	appendWifiNetsSection();     // WiFi networks: list, DELETE, ADD (05_WiFi.cpp)
	// START FRESH — after installation / calibration: wipe the measured data,
	// keep every setting.  POST (not a link) so nothing can trigger it by loading a URL.
	appendHtmlStr("<div class='btns'>"
	              "<button class='btn btn-rd' onclick=\"StartFresh()\">START FRESH (after installation)</button>"
	              "</div>"
	              "<p style='text-align:center;color:#64748b;font-size:11px;margin:0 8px 8px'>"
	              "Clears rainfall, water balance, temperature history and the General Log. "
	              "Keeps all settings, the calibration, NTFY and WiFi. The gauge restarts.</p>"
	              "<script>function StartFresh(){"
	              "if(!confirm('START FRESH?\\n\\nThis deletes all rainfall, water balance, temperature history and the General Log.\\nSettings and calibration are kept.\\n\\nThis cannot be undone.'))return;"
	              "fetch('/startfresh',{method:'POST'}).then(function(){"
	              "document.body.innerHTML='<h3 style=\"text-align:center;margin-top:40px\">Starting fresh &mdash; the gauge is restarting&hellip;</h3>';"
	              "setTimeout(function(){location.href='/';},15000);});}</script>");
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// =============================================================================
// START FRESH  (POST /startfresh, SETTINGS button)
// Runs in async_tcp — only raises a flag; MaintainStartFresh() (loop) deletes
// the data files and restarts, so nothing in RAM can write old totals back.
// Kept: AllParams.json (settings + calibration), NTFY.json, WiFiDefault.json,
// About.json, FormatComplete.txt.
// =============================================================================
static volatile bool startFreshRequested = false;

void handleStartFresh(AsyncWebServerRequest *request){
	startFreshRequested = true;
	request->send(200, "text/plain", "OK");
}

// Called every 10 ms from loop().
void MaintainStartFresh(){
	if (!startFreshRequested) return;
	startFreshRequested = false;
	static const char* const files[] = {
		RAIN_LOG_FILE,  WB_LOG_FILE,  WB_STATE_FILE,  TEMP_HIST_FILE,  TEMP_STATE_FILE,
		SIM_RAIN_LOG_FILE, SIM_WB_LOG_FILE, SIM_WB_STATE_FILE, SIM_TEMP_HIST_FILE, SIM_TEMP_STATE_FILE,
	};
	for (const char* f : files) SPIFFS.remove(f);
	SPIFFS.remove(StrGeneralLog);
	SaveGenReport F("START FRESH: rainfall, water balance, temperature history and log cleared (settings + calibration kept)");
	delay(300);          // let the browser get its reply
	ESP.restart();
}

// =============================================================================
// Settings Page — WT_* Table Builder & WebSocket Handlers
// =============================================================================

void WT_SETUP_Settings(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("Parameter Description", "Value", "");

	for (int i=0; i < PAGE_VAR_SETTINGS_BASE; i++)
	{
		WT_EditRow(&SettingsList[i]);
	}
	WT_EditRow(&SettingsList[SET_TIME_ZONE]);
	WT_EditRow(&SettingsList[SET_SHOW_SYSINFO]);
	WT_EditRow(&SettingsList[SET_SHOW_INSIDE]);
	WT_CheckEditRow(&SettingsList[SET_REBOOT_EN1], &SettingsList[SET_REBOOT_TIME1], "time");

	WT_SectionHeader("Rain Gauge");
	for (int i = SET_FUNNEL_DIA; i <= SET_CAL_WATER; i++)
	{
		WT_EditRow(&SettingsList[i]);
	}
	WT_EditRow(&SettingsList[SET_IR_TEST_MIN]);

	// Method first; the rest of the irrigation settings only for the chosen method
	// (page reloads after UPDATE when the method changes — see UpdateSettingsPageValues).
	WT_SectionHeader("Irrigation");
	WT_EditRow(&SettingsList[SET_IRRI_MODE]);
	if (!IrrigationOff()){
		String ctrl = ControllerName();
		WT_EditRow(&SettingsList[SET_CTRL_NAME]);
		SettingsList[SET_IRRI_TIME].Var->SetDescription(ctrl + " Irrigates At");
		WT_EditRow(&SettingsList[SET_IRRI_TIME], "time");

		if (Params->IrriMode == IRRI_MODE_WB){
			WT_SectionHeader(("Water Balance  (answer for " + ctrl + ", /api/irrigate)").c_str());
			for (int i = SET_LATITUDE; i <= SET_IRRI_DEPTH; i++) WT_EditRow(&SettingsList[i]);
		} else {
			WT_SectionHeader(("Simple Rule  (answer for " + ctrl + ", /api/irrigate)").c_str());
			WT_EditRow(&SettingsList[SET_IRRI_SKIP_MM]);
			WT_EditRow(&SettingsList[SET_IRRI_SKIP_HRS]);
		}
	}

	WT_EndPage();
}

// -----------------------------------------------------------------------------
// ControllerName — Params->ControllerName for labels: trimmed, never empty,
// and without characters that would break the generated HTML / JavaScript.
// -----------------------------------------------------------------------------
String ControllerName(){
	String n = Params ? Params->ControllerName : String();
	String out;
	for (size_t i = 0; i < n.length() && out.length() < 32; i++){
		char c = n[i];
		if (c == '"' || c == '<' || c == '>' || c == '&' || c == 39 || c == 92 || c < 32) continue;
		out += c;
	}
	out.trim();
	return out.length() ? out : String("Irrigation Controller");
}

void SendSettingsPageValues(){
	for (int i=0; i < PAGE_VAR_SETTINGS; i++)
	{
		if(SettingsList[i].Var->HasChanged())
		{
			webParamUpdate(SettingsList[i].Var->GetName(),
			               SettingsList[i].Var->GetID(""),
			               SettingsList[i].Var->GetVarStyle());
			SettingsList[i].Var->UpdateLast();
		}
	}
}

void UpdateSettingsPageValues(uint32_t clientId, JsonObject *root){
	String oldMode = Params->IrriMode;
	String oldTz   = Params->TimeZone;

	for (JsonPair kv : *root){
		for (int i=0; i < PAGE_VAR_SETTINGS; i++)
		{
			if(SettingsList[i].Var->GetID("") == kv.key().c_str())
			{
				SettingsList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}

	WebMessUpdate(clientId, "Parameters Updated!", "color: red", true);
	Params->WriteData();
	ApplyRainSettings();
	if (Params->TimeZone != oldTz){
		ApplyTimeZone();     // new zone at once, no restart (02_Time.cpp)
		SaveGenReport("Time zone set to " + Params->TimeZone);
	}
	UpdateTimerTimes();
	// Method changed: the page shows other sections now — let the browser reload it.
	if (Params->IrriMode != oldMode) ws.text(clientId, "{\"type\":\"reload\"}");
}
