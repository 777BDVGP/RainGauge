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
	appendHtmlStr(webHTMLEnd);

	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// appendMainPageSel
// Builds the top navigation tab strip: HOME, REPORTS, SETTINGS, FILES, NTFY, ABOUT.
// -----------------------------------------------------------------------------
void appendMainPageSel(int Index){
	appendHtmlStr("<div class='nav'>");
	appendHtmlStr(Index == PS_HOME     ? "<a class='act'>HOME</a>"     : "<a href='/'>HOME</a>");
	appendHtmlStr(Index == PS_REPORTS  ? "<a class='act'>REPORTS</a>"  : "<a href='/reports'>REPORTS</a>");
	appendHtmlStr(Index == PS_SETTINGS ? "<a class='act'>SETTINGS</a>" : "<a href='/settings'>SETTINGS</a>");
	appendHtmlStr(Index == PS_FILES    ? "<a class='act'>FILES</a>"    : "<a href='/files'>FILES</a>");
	appendHtmlStr(Index == PS_NTFY     ? "<a class='act'>NTFY</a>"     : "<a href='/ntfy'>NTFY</a>");
	appendHtmlStr(Index == PS_ABOUT    ? "<a class='act'>ABOUT</a>"    : "<a href='/about'>ABOUT</a>");
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
// webRebootButton
// Returns an HTML button that calls ButtonReboot() via WebSocket when clicked.
// -----------------------------------------------------------------------------
void webRebootButton()
{
	appendHtmlStr("<div><br></div><div><b><a>");
	appendHtmlStr("<b><a><input type=\"button\" id=\"REBOOT\" onclick=\"ButtonReboot()\" style=\"font-size:14px;background-color:#dc2626;color:#fff;border:none;padding:6px 14px;border-radius:4px;cursor:pointer\" value=\"REBOOT\"</a></b></div>");
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
