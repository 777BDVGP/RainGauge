// =============================================================================
// forward_decls.h — Cross-File Function Forward Declarations
// =============================================================================
#pragma once

// ─── 01_Main.cpp ─────────────────────────────────────────────────────────────
void InitHardware();
void WatchdogSetup();
void ISRwatchdog();
void LEDToggle();
void MaintainGatewayWatch();
void Heartbeat(bool on);
void HeartbeatToggle();
void UpdateTimerTimes();

// ─── 02_Time.cpp ─────────────────────────────────────────────────────────────
void SetupTime();
void MaintainTimers();
void MaintainScheduledReboot();
void MaintainHeapGuard();
void LogWatchdogReboot();
void UpdateWebPageTime();
void CompileDateTime();
void reduce_file(String filename, int Entries);
void activateTimer(int index);
void deactivateTimer(int index);
bool TimerExpired(int index);
void SetValueAndAtivate(String Units, int TimerUnits, int index);
void SetTimerValue(int Units, int index);
int  RemainingSec(int index);
int  RemainingMin(int index);

String GetCurrentDateAndTime();
String GetCurrentDate(int daysOffset);
String GetCurrentTime();
String IncrementDateWithOneDay(String date);
String AddTimeToCurrentDateTime(String baseDateTime, String period);

// ─── 03_Files.cpp ─────────────────────────────────────────────────────────────
extern String FSInitLog;
void   InitFileSystem();
void   FilesPage(AsyncWebServerRequest *request);
void   handleApiFiles(AsyncWebServerRequest *request);
void   handleFileView(AsyncWebServerRequest *request);
void   handleFileDeleteJson(AsyncWebServerRequest *request);
void   File_Download(AsyncWebServerRequest *request);
void   handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

// ─── 04_WebPages.cpp ──────────────────────────────────────────────────────────
bool   beginHtmlStream(AsyncWebServerRequest *request);
void   SetupWebPage();
void   MaintainWebPage();
void   MaintainWebService();
void   handleWssJs(AsyncWebServerRequest *request);
void   webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void   webParamUpdate(const String& Name, const String& ID, const String& ATT, const char* ATTName = "style");
void   UnpackJsonPayload(uint32_t clientId, uint8_t * payload, size_t length);
void   WebMessUpdate(uint32_t clientId, const String& Mess, const String& Att, bool Reset);
void   UpdateSelectMessDelay();
void   append_FileAction(int ReqMess);
void   WT_StartTable(const char* Hdr1, const char* Hdr2, const char* Hdr3);
void   WT_Column(int WT_Type, int WT_Begin, int WT_Finish, const char* Type, const char* Labval, const char* ID, const char* Style, PageVar *ListObj, const char* ExtraAttrs = "");
void   WT_AutoColumn(int WT_Type, int WT_Begin, int WT_Finish, PageVar *pv, const char* IDPrefix = "", const char* ExtraAttrs = "");
void   WT_DisplayRow(PageVar *pv);
void   WT_EditRow(PageVar *pv, const char* typeOverride = "", const char* ExtraAttrs = "");
void   WT_CheckEditRow(PageVar *cbPv, PageVar *valPv, const char* typeOverride = "");
void   WT_RightCheckRow(PageVar *pv);
void   WT_StartPage();
void   WT_EndPage();
void   WT_EndPageClass(const char* cls);
void   WT_Header(const char* Header);
void   WT_SectionHeader(const char* Header);
void   WT_StartLine();
void   WT_EndLine();
void   WT_StartLabel();
void   WT_StartLabelEmpty();
void   WT_StartInput();
void   WT_StartInputEmpty();
void   WT_NextColumn();
void   WT_EndCell();
void   WT_TypeCell(const char* Type);
void   WT_TextLabel(const char* Txt);
void   WT_ValueInput(const char* Val);
void   WT_IDCell(const char* ID);
void   WT_AttrsStart();
void   WT_Attr(const char* n, const char* v);
void   WT_AttrsEnd();
void   WT_OptionsStart();
void   WT_OptionsValues(const char* AVal);
void   WT_OptionsEnd();

// ─── 05_ThisSystem.cpp ────────────────────────────────────────────────────────
void SetupSystemIOs();
void PinDebounce();
void MaintainWebPageValues();
void AddRoutes();
void SetIrLed(bool on);
void MaintainIrSelfTest();
bool GetIrLed();

// ─── 05_WiFi.cpp ──────────────────────────────────────────────────────────────
bool   SetupWiFi();
String WiFiHostName();
void   appendWifiNetsSection();
void   handleWifiNetAdd(AsyncWebServerRequest *request);
void   handleWifiNetDelete(AsyncWebServerRequest *request);
void   MaintainWifiNets();
void   handleWifiScan(AsyncWebServerRequest *request);

// ─── 22_Update.cpp ────────────────────────────────────────────────────────────
void   handleFwCheck(AsyncWebServerRequest* r);
void   handleFwInstall(AsyncWebServerRequest* r);
void   handleFwUploadData(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final);
void   handleFwUploadDone(AsyncWebServerRequest* request);
void   MaintainFwUpdate();
void   SendFwValues();
void   appendFwUpdateBlock();

// ─── 08_Home.cpp ──────────────────────────────────────────────────────────────
void   handleRoot(AsyncWebServerRequest *request);
void   appendMainPageSel(int Index);
void   appendIrrigationHelp();
void   WT_HomePage();
void   MapHomePage();
void   UpdateHomeBeam();
void   UpdateHomeValues();
void   SendHomePageValues();
void   MaintainHomePage();
void   webRebootButton();

// ─── 10_Reports.cpp ───────────────────────────────────────────────────────────
void   handleReports(AsyncWebServerRequest *request);

// ─── 12_Settings.cpp ──────────────────────────────────────────────────────────
void   CreateAllParams();
void   MapSettingsPage();
void   handleSettings(AsyncWebServerRequest *request);
void   WT_SETUP_Settings();
void   SendSettingsPageValues();
void   UpdateSettingsPageValues(uint32_t clientId, JsonObject *root);
void   handleStartFresh(AsyncWebServerRequest *request);
void   MaintainStartFresh();

// ─── 13_Ntfy.cpp ──────────────────────────────────────────────────────────────
void   CreateNtfyParams();
void   MapNtfyPage();
void   handleNtfy(AsyncWebServerRequest *request);
void   WT_SETUP_Ntfy();
void   SendNtfyPageValues();
void   UpdateNtfyPageValues(uint32_t clientId, JsonObject *root);
void   MaintainNtfy();
void   MaintainWsCommands();
void   MaintainTempRecordsReset();
void   MessageNTFY(String message);
void   appendNtfyTestButton();
void   handleTestNtfy(AsyncWebServerRequest *request);
void   handleIrTestNow(AsyncWebServerRequest *request);
void   CheckTempAlerts(float outside);
IrBeamView IrBeamNow();

// ─── 15_About.cpp ─────────────────────────────────────────────────────────────
void   SetupAbout();
void   SetupOTA();
void   handleAbout(AsyncWebServerRequest *request);
void   handleApiSysInfo(AsyncWebServerRequest *request);

// ─── 16_Rain.cpp ──────────────────────────────────────────────────────────────
bool   RainTimeValid();
float  RainMmPerTip();
float  RainTodayMm();
float  RainMonthMm();
void   ApplyRainSettings();
void   MaintainRain();
void   CalCommand(uint32_t clientId, const String& cmd);
void   SendCalibrationValues(bool force);
void   handleCalibrate(AsyncWebServerRequest *request);

// ─── 17_Temperature.cpp ───────────────────────────────────────────────────────
void   SetupTemperature();
void   MaintainTemperature();
String TempText(float c);

void   RainForceRecalc();

// ─── 18_Simulation.cpp ────────────────────────────────────────────────────────
void        CreateSimulation();
const char* RainLogFile();
float       SimOutsideTemp();
bool        TempIsF();
const char* TempUnitSym();
float       TempDisp(float c);
float       TempDeltaDisp(float dc);
float       TempFromDisp(float v);
String      TempNum(float c);
String      TempNumStr(const String& cText);
void        RefreshTempUnitLabels();
bool        SimFireTip();
void        MaintainSimulation();
void        SimCommand(uint32_t clientId, const String& cmd);
void        UpdateSimPageValues(uint32_t clientId, JsonObject *root);
void        SendSimulationValues(bool force);
void        handleSimulate(AsyncWebServerRequest *request);

// ─── 19_Irrigation.cpp ────────────────────────────────────────────────────────
float      RainInLastHours(int hours);
IrriAnswer IrrigationDecision();
void       handleApiIrrigate(AsyncWebServerRequest *request);

// ─── 20_WaterBalance.cpp ──────────────────────────────────────────────────────
float      WB_TAW();
float      WB_RAW();
float      WB_Dr();
void       WaterBalanceSave();
void       WaterBalanceLoad();
void       WaterBalanceTrackTemp(float c);
void       MaintainWaterBalance();
IrriAnswer WaterBalanceAnswer();
String     WaterBalanceText();
void       WaterBalanceReplay(int days);
void       appendWaterBalanceTable();

// ─── 21_TempHistory.cpp ───────────────────────────────────────────────────────
const char* TempHistFile();
void        TempHistorySave();
void        TempHistoryLoad();
void        TempHistoryTrack(float outside, float inside);
void        handleResetTempRecords(AsyncWebServerRequest *request);
String      TempTodayText(const MinMax& m);
void        TempHistoryGenerate(int days);
void        appendTempHistoryTables();

// ─── main.cpp ─────────────────────────────────────────────────────────────────
void PrintTestMsg(String Message, uint32_t seconds);
