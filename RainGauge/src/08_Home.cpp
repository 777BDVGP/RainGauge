// =============================================================================
// 08_Home.cpp — Home Page Route Handler & Real-Time Status Maintenance
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// handleRoot  (GET /)
// -----------------------------------------------------------------------------
void handleRoot(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;

	UpdateHomeValues();
	WT_HomePage();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendMainPageSel(PS_HOME);
	appendHtmlStr(webScriptTag);
	appendHtmlBuf(PageBuf, getPageBufLen());
	if (!IrrigationOff() && Params->IrriMode == IRRI_MODE_WB) appendIrrigationHelp();   // Water Balance only
	appendHtmlStr(webHTMLEnd);

	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// appendIrrigationHelp
// "Show help (how the irrigation answer works)" tick box + explanation — only
// in Water Balance mode.  Sent after the table, then moved by the script into
// a full-width row right below "Evaporation Yesterday (ET0)".  Same look as the NTFY help box; the numbers are this
// gauge's own SETTINGS.  Unticked by default (HOME is the everyday page); the
// choice is remembered in this browser (localStorage).
// -----------------------------------------------------------------------------
void appendIrrigationHelp(){
	String ctl = ControllerName();
	appendHtmlStr("<div id='irrihelpbox'><div style='margin:2px 0 0;font-size:13px;text-align:left'><label>"
	              "<input type='checkbox' id='irrihelpcb' onchange='irriHelp(this.checked,1)'> "
	              "Show help (how the irrigation answer works)</label></div>"
	              "<div id='irrihelp' style='display:none;background:#f1f5f9;border:1px solid #cbd5e1;border-radius:6px;padding:8px 10px;margin:4px 0 8px;font-size:12px;line-height:1.5;color:#334155'>");

	String s;
	{
		float taw = WB_TAW(), raw = WB_RAW();
		float use5 = Params->CropKc * 5.0f;                  // a typical summer day: ET0 5 mm
		int   days = use5 > 0 ? (int)(raw / use5 + 0.5f) : 0;
		s  = "<b style='color:#0284c7'>How the irrigation answer works (Water Balance)</b>"
		     "<ol style='margin:4px 0 4px 18px;padding:0'>";
		s += "<li><b>The controller asks once a day.</b> " + ctl + " is set to water every day at a fixed time &mdash; <i>Controller Irrigates At</i> on SETTINGS, now <b>" + Params->IrriTime +
		     "</b>. Before it starts, it asks the rain gauge <i>irrigate today?</i> &mdash; <i>yes</i> = it waters, "
		     "<i>no</i> = it skips today. The gauge's answer for the day is fixed at " + Params->IrriTime +
		     " and written in the General Log. <i>Controller Irrigates At</i> on SETTINGS must be the same time as the "
		     "start time set on " + ctl + ".</li>";
		s += "<li><b>The soil is a water tank.</b> " + Params->SoilType + " soil with " + String(Params->RootDepthMm) +
		     " mm deep roots holds <b>" + String(taw, 1) + " mm</b> of water the plants can use "
		     "(per metre of roots: sand 80, loam 150, clay 200 mm).</li>";
		s += "<li><b>When it waters.</b> Once <b>" + String(raw, 1) + " mm</b> of that is used (" + String(Params->DepletionPct) + "% of " + String(taw, 1) +
		     " mm &mdash; <i>Irrigate when Soil Water Used</i>). The <i>Soil Water</i> row above shows how much is used now; "
		     "0 mm = tank full.</li>";
		appendHtmlStr(s.c_str());

		s  = "<li><b>The garden drinks every day.</b> At midnight the gauge works out the day's evaporation (ET0) from the "
		     "lowest and highest <b>outside temperature</b> and the sun's strength for the date and Latitude (Hargreaves method). "
		     "Hot days with cool nights = more. ET0 &times; Garden Factor Kc " + String(Params->CropKc, 2) +
		     " = what the garden used. No temperature readings that day = the monthly average is used.</li>";
		s += "<li><b>Rain fills the tank.</b> A day's rain of at least " + String(Params->MinEffRainMm, 1) +
		     " mm counts; less only wets the leaves and evaporates. The tank can't overflow: a big storm only fills it to full.</li>";
		s += "<li><b>Rain counts at once, not only at midnight.</b> Midnight only works out the evaporation (it needs the whole "
		     "day's min / max). Rain is taken off the moment it falls: 20 mm at 05:00 already makes the " + Params->IrriTime +
		     " answer <i>Skip</i> (&quot;rain today&quot; in the text). Rain after " + Params->IrriTime +
		     " can't change today's answer any more (" + ctl + " has already watered or skipped), but it fills the tank "
		     "at midnight and can make tomorrow a <i>Skip</i>.</li>";
		s += "<li><b>Irrigation fills it too.</b> On a <i>yes</i> day the gauge assumes " + ctl + " added <b>" +
		     String(Params->IrriDepthMm, 1) + " mm</b> (<i>One Irrigation Adds</i> on SETTINGS &mdash; measure it with the cup test).</li>";
		s += "</ol>";
		if (days > 0)
			s += "<b>Example:</b> on a hot day (ET0 5 mm) the garden uses " + String(use5, 1) + " mm, so from a full tank "
			     "the first watering comes after about <b>" + String(days) + " dry days</b>.<br>";
		s += "<b>To water more often:</b> lower <i>Irrigate when Soil Water Used (%)</i>, a shallower Root Depth or Sand soil. "
		     "<b>Less often:</b> the opposite, or a lower Garden Factor Kc. All on SETTINGS &rarr; Irrigation. "
		     "Day by day figures: REPORTS &rarr; WATER BALANCE.";
	}
	s += "</div></div>"
	     "<script>function irriHelp(on,save){document.getElementById('irrihelp').style.display=on?'block':'none';"
	     "if(save){try{localStorage.setItem('irriHelp',on?'1':'0');}catch(e){}}}"
	     "(function(){var on=false;try{on=localStorage.getItem('irriHelp')==='1';}catch(e){}"
	     "document.getElementById('irrihelpcb').checked=on;irriHelp(on,0);"
	     "var b=document.getElementById('irrihelpbox'),l=document.getElementById('L" + String(HOME_ET0 + ID_HOME) + "'),"
	     "r=l?l.closest('tr'):null;if(r){var t=document.createElement('tr'),c=t.insertCell(0);c.colSpan=2;"
	     "c.style.cssText='text-align:left;white-space:normal;padding-right:6px';c.appendChild(b);r.after(t);}})();</script>";
	appendHtmlStr(s.c_str());
}

// -----------------------------------------------------------------------------
// appendMainPageSel
// Main row:  HOME | REPORTS | OTHER
// REPORTS opens RAIN; on its pages a second row RAIN | TEMPERATURE |
// WATER BALANCE shows the page (WATER BALANCE hidden for "No Irrigation").
// OTHER opens SETTINGS; on SETTINGS / FILES / NTFY / ABOUT (and CALIBRATION /
// SIMULATION, which count as SETTINGS) a second row SETTINGS | FILES | NTFY |
// ABOUT shows the page.
// -----------------------------------------------------------------------------
static void navTab(bool act, const char* href, const char* label){
	if (act) { appendHtmlStr("<a class='act'>"); }
	else     { appendHtmlStr("<a href='"); appendHtmlStr(href); appendHtmlStr("'>"); }
	appendHtmlStr(label);
	appendHtmlStr("</a>");
}

void appendMainPageSel(int Index){
	bool reports = (Index == PS_RAIN || Index == PS_TEMP || Index == PS_WATERBAL);
	bool other   = (Index == PS_SETTINGS || Index == PS_FILES || Index == PS_NTFY || Index == PS_ABOUT);
	appendHtmlStr("<div class='nav'>");
	navTab(Index == PS_HOME, "/", "HOME");
	appendHtmlStr(reports ? "<a class='act' href='/rain'>REPORTS</a>"  : "<a href='/rain'>REPORTS</a>");
	if (NavAuthed)                    // home network or logged in (23_Access.cpp)
		appendHtmlStr(other ? "<a class='act' href='/settings'>OTHER</a>" : "<a href='/settings'>OTHER</a>");
	else if (AccessPasswordSet())     // outside, not logged in: LOGIN (hidden without a password)
		appendHtmlStr(Index == PS_LOGIN ? "<a class='act'>LOGIN</a>" : "<a href='/login'>LOGIN</a>");
	appendHtmlStr("</div>");
	if (reports){
		appendHtmlStr("<div class='nav sub'>");
		navTab(Index == PS_RAIN, "/rain",        "RAIN");
		navTab(Index == PS_TEMP, "/temperature", "TEMPERATURE");
		if (!IrrigationOff()) navTab(Index == PS_WATERBAL, "/waterbalance", "WATER BALANCE");
		appendHtmlStr("</div>");
		return;
	}
	if (!other) return;
	appendHtmlStr("<div class='nav sub'>");
	navTab(Index == PS_SETTINGS, "/settings", "SETTINGS");
	navTab(Index == PS_FILES,    "/files",    "FILES");
	navTab(Index == PS_NTFY,     "/ntfy",     "NTFY");
	navTab(Index == PS_ABOUT,    "/about",    "ABOUT");
	if (NavOutside) appendHtmlStr("<a href='/logout'>LOGOUT</a>");   // logged in from outside
	appendHtmlStr("</div>");
}

// -----------------------------------------------------------------------------
// WT_HomePage
// Writes the home-page WT parameter table into PageBuf.
// -----------------------------------------------------------------------------
void WT_HomePage(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("DESCRIPTIONS", "INSTANT VALUES", "");

	if (!IrrigationOff()){
		WT_SectionHeader(("Irrigation (answer to " + ControllerName() + ")").c_str());
		WT_DisplayRow(&HomeList[HOME_IRRIGATE]);
		WT_DisplayRow(&HomeList[HOME_SOIL]);
		WT_DisplayRow(&HomeList[HOME_ET0]);
	}

	WT_SectionHeader("Rainfall");
	WT_DisplayRow(&HomeList[HOME_RAIN_BEAM]);
	for (int i = HOME_RAIN_TODAY; i <= HOME_RAIN_MMTIP; i++) WT_DisplayRow(&HomeList[i]);

	WT_SectionHeader("Temperature");
	WT_DisplayRow(&HomeList[HOME_TEMP_EXT]);
	WT_DisplayRow(&HomeList[HOME_TODAY_T]);
	if (Params->ShowInsideTemp){            // SETTINGS "Show Inside Temperature"
		WT_DisplayRow(&HomeList[HOME_TEMP_INT]);
		WT_DisplayRow(&HomeList[HOME_TODAY_TIN]);
	}

	WT_EndPageClass("info");   // display-only: labels 45%, long values wrap
}

// -----------------------------------------------------------------------------
// MapHomePage
// Allocates the HomeList[] PageVar array.  Called from InitFileSystem().
// -----------------------------------------------------------------------------
void MapHomePage(){
	HomeList = new PageVar[PAGE_VAR_HOME];
	for (int i = 0; i < PAGE_VAR_HOME; i++)
	{
		switch(i)
		{
			case HOME_IRRIGATE:
				HomeList[i].Var = new PageElement(&HomeIrrigate,  F("Irrigate Today?"),    "", &HomeIrriStyle, i+ID_HOME);
				break;
			case HOME_SOIL:
				HomeList[i].Var = new PageElement(&HomeSoil,      F("Soil Water"),                  "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_ET0:
				HomeList[i].Var = new PageElement(&HomeEt0,       F("Evaporation Yesterday (ET0)"), "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_TODAY_T:
				HomeList[i].Var = new PageElement(&HomeTodayT,    F("Outside Today Min / Max"),     "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_TODAY_TIN:
				HomeList[i].Var = new PageElement(&HomeTodayTIn,  F("Inside Today Min / Max"),      "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_RAIN_TODAY:
				HomeList[i].Var = new PageElement(&HomeRainToday, F("Rain Today"),         "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_RAIN_MONTH:
				HomeList[i].Var = new PageElement(&HomeRainMonth, F("Rain This Month"),    "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_RAIN_LASTTIP:
				HomeList[i].Var = new PageElement(&HomeLastTip,   F("Last Tip"),           "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_RAIN_TIPS:
				HomeList[i].Var = new PageElement(&HomeTips,      F("Tips Since Boot"),    "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_RAIN_MMTIP:
				HomeList[i].Var = new PageElement(&HomeMmPerTip,  F("Rain per Tip"),       "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_TEMP_EXT:
				HomeList[i].Var = new PageElement(&HomeTempExt,   F("Outside"), "", &HomeStyle, i+ID_HOME);
				break;
			case HOME_RAIN_BEAM:
				HomeList[i].Var = new PageElement(&HomeBeam,      F("Bucket Position (IR beam)"), "", &HomeBeamStyle, i+ID_HOME);
				break;
			case HOME_TEMP_INT:
				HomeList[i].Var = new PageElement(&HomeTempInt,   F("Inside (internal)"),  "", &HomeStyle, i+ID_HOME);
				break;
		}
	}
	MainPageObject = true;
}

// -----------------------------------------------------------------------------
// UpdateHomeValues
// Refreshes the variables bound to HomeList[] — SendHomePageValues() pushes
// whichever changed.
// -----------------------------------------------------------------------------
void UpdateHomeValues(){
	// The irrigation answer reads the whole rain log — refresh it once a
	// minute (and on every page load), not every 500 ms.
	static uint32_t lastIrri = 0;
	if (lastIrri == 0 || millis() - lastIrri >= 60000UL || HomeIrrigate.length() == 0){
		lastIrri = millis() | 1;
		IrriAnswer a  = IrrigationDecision();
		HomeIrrigate  = a.reason;
		HomeIrriStyle = a.irrigate ? ATT_GREEN : ATT_ORANGE;
	}
	HomeSoil   = WaterBalanceText();
	HomeEt0    = isnan(WB.lastEt0) ? String("- (first day not finished)") : String(WB.lastEt0, 2) + " mm";
	HomeTodayT   = TempTodayText(TH.out);
	HomeTodayTIn = TempTodayText(TH.in);
	HomeRainToday = String((long)lroundf(RainTodayMm())) + " mm";
	HomeRainMonth = String((long)lroundf(RainMonthMm())) + " mm";
	HomeLastTip   = RainLastTip;
	HomeTips      = (int)RainTipsSinceBoot;
	HomeMmPerTip  = String(RainMmPerTip(), 3) + " mm" + (Params->CalDate == "never" ? "  (NOT calibrated)" : "");
	HomeTempExt   = TempText(TempExternal);
	HomeTempInt   = TempText(TempInternal);
	UpdateHomeBeam();
}

// -----------------------------------------------------------------------------
// UpdateHomeBeam
// Live bucket position from the debounced IR pickup (HIGH = dark = beam
// broken).  Called every 50 ms so a hand tip shows at once.  The debounced
// value is frozen during the IR self-test, so the test never flickers it.
// -----------------------------------------------------------------------------
void UpdateHomeBeam(){
	bool broken   = InPort[IIDX_IR_PICKUP].Input;
	HomeBeam      = broken ? "BROKEN" : "OPEN";
	HomeBeamStyle = broken ? ATT_RED : ATT_GREEN;
}


// -----------------------------------------------------------------------------
// SendHomePageValues  (called every 50 ms from MaintainWebPageValues)
// -----------------------------------------------------------------------------
void SendHomePageValues(){
	UpdateHomeBeam();
	for (int i = 0; i < PAGE_VAR_HOME; i++){
		if (HomeList[i].Var->HasChanged()){
			webParamUpdate(HomeList[i].Var->GetName(), HomeList[i].Var->GetID("V"), HomeList[i].Var->GetVarStyle());
			HomeList[i].Var->UpdateLast();
		}
	}
}

// -----------------------------------------------------------------------------
// MaintainHomePage  (called every 500 ms from MaintainWebService)
// -----------------------------------------------------------------------------
void MaintainHomePage(){
	if (!HomePageReady) {
		if (!TimerExpired(TIM_STABILIZE_DELAY)) return;
		HomePageReady = true;
	}
	UpdateHomeValues();
}
