// =============================================================================
// 15_About.cpp — OTA Update Service & About Page
// =============================================================================
// Implements ArduinoOTA setup (with watchdog guard), the About web page,
// and About.json persistence (build stamp).
// The About page is read-only: an OTA / network table, a REBOOT button and a
// centred project-info block.  System Name is edited on the Settings page.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// ─── SPIFFS path for About.json ───────────────────────────────────────────────
static constexpr const char* ABOUT_FILE_PATH = "/About.json";

// ─── OTA State ────────────────────────────────────────────────────────────────
bool OTAInProgress      = false;
int  OTAPercentComplete = 0;

// -----------------------------------------------------------------------------
// SetupAbout
// Compares the current build time (__DATE__ " " __TIME__) with the stored
// buildStamp in /About.json.  If they differ, a new build has been uploaded,
// so we update buildStamp.  Called once from setup() after WiFi is connected.
// -----------------------------------------------------------------------------
void SetupAbout() {
    String currentBuild = String(__DATE__) + " " + String(__TIME__);
    DynamicJsonDocument doc(512);
    bool changed = false;

    File fr = SPIFFS.open(ABOUT_FILE_PATH, "r");
    if (fr) {
        deserializeJson(doc, fr);
        fr.close();
        if (!doc.containsKey("buildStamp") || doc["buildStamp"].as<String>() != currentBuild) {
            changed = true;
        }
    } else {
        changed = true;
    }

    if (changed) {
        doc["buildStamp"]   = currentBuild;
        File fw = SPIFFS.open(ABOUT_FILE_PATH, "w");
        if (fw) {
            serializeJson(doc, fw);
            fw.close();
            DEBUG_PRINTLN("About.json updated with new buildStamp");
        }
    }
}

// =============================================================================
// OTA Setup
// =============================================================================

// -----------------------------------------------------------------------------
// SetupOTA
// Configures ArduinoOTA (hostname = WiFi/mDNS hostname) and registers callbacks.
// The watchdog Ticker is detached during flash writes and re-attached on end/error.
// Called once from setup() after WiFi connects.
// -----------------------------------------------------------------------------
void SetupOTA() {
    ArduinoOTA.setPort(3232);
    ArduinoOTA.setHostname(WiFiHostName().c_str());
    ArduinoOTA.setMdnsEnabled(false);   // mDNS already started in SetupWiFi()

    ArduinoOTA
        .onStart([]() {
            OTAInProgress      = true;
            OTAPercentComplete = 0;
            secondTick.detach();   // Disable watchdog — flash writes block > 5 s
            ws.textAll("{\"type\":\"ota_start\"}");
            ws.closeAll();         // drop all WebSocket clients so they don't
                                   // compete with the flash stream for heap/CPU
            DEBUG_PRINTLN("OTA Start");
        })
        .onEnd([]() {
            DEBUG_PRINTLN("\nOTA End");
            ws.textAll("{\"type\":\"ota_end\"}");
            OTAInProgress = false;
            secondTick.attach(1, ISRwatchdog);
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            int pct = (progress / (total / 100));
            if (pct != OTAPercentComplete && pct % 5 == 0) {
                String msg = "{\"type\":\"ota_progress\",\"percent\":" + String(pct) + "}";
                ws.textAll(msg);
            }
            OTAPercentComplete = pct;
            DEBUG_PRINTF("OTA Progress: %u%%\r", OTAPercentComplete);
        })
        .onError([](ota_error_t error) {
            DEBUG_PRINTF("OTA Error[%u]\n", error);
            ws.textAll("{\"type\":\"ota_error\"}");
            OTAInProgress = false;
            secondTick.attach(1, ISRwatchdog);
        });

    ArduinoOTA.begin();
    DEBUG_PRINTLN("OTA Ready — " + WiFi.localIP().toString());
    DEBUG_PRINTLN("PIO upload: pio run -e ota -t upload");
}

// =============================================================================
// About Page — HTML Builders
// =============================================================================

// One read-only "label | value" row (ListObj nullptr — Type is never "select").
static void aboutRow(const char* idL, const char* label, const char* idV, const char* value, const char* style = ""){
    WT_StartLine();
    WT_Column(WT_LABEL, WT_FIRST, WT_MORE, "text", label, idL, "",    nullptr);
    WT_Column(WT_LABEL, WT_NEXT,  WT_LAST, "text", value, idV, style, nullptr);
    WT_Column(WT_INPUT, WT_FIRST, WT_LAST, "", "", "", "", nullptr);
    WT_EndLine();
}

// =============================================================================
// Memory & Task Readout  (ABOUT page + GET /api/sysinfo)
// =============================================================================
// Heap = internal RAM only (PSRAM is listed separately).  "Stack free (min)"
// is each task's high-water mark — the least free stack it has ever had.

#define SYSINFO_MAX_TASKS  24
static TaskStatus_t sysTasks[SYSINFO_MAX_TASKS];   // static: the readout runs in async_tcp

static const char* taskStateName(eTaskState s){
    switch (s){
        case eRunning:   return "running";
        case eReady:     return "ready";
        case eBlocked:   return "blocked";
        case eSuspended: return "suspended";
        case eDeleted:   return "deleted";
        default:         return "?";
    }
}

static String uptimeText(){
    uint32_t s = millis() / 1000;
    char buf[32];
    snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
             (unsigned long)(s / 86400), (unsigned long)(s / 3600 % 24),
             (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    return buf;
}

// Fills sysTasks[]; returns how many.
static UBaseType_t readTasks(){
    return uxTaskGetSystemState(sysTasks, SYSINFO_MAX_TASKS, nullptr);
}

static String taskLine(const TaskStatus_t& t){
    char buf[64];
    snprintf(buf, sizeof(buf), "prio %u, stack free (min) %lu B, %s",
             (unsigned)t.uxCurrentPriority, (unsigned long)t.usStackHighWaterMark,
             taskStateName(t.eCurrentState));
    return buf;
}

// GET /api/sysinfo — plain text, deliberately NOT behind the 503 low-heap
// guard, so the numbers can still be read while pages are being refused.
void handleApiSysInfo(AsyncWebServerRequest *request){
    String s;
    s.reserve(1200);
    s += "uptime          " + uptimeText() + "\n";
    s += "heap free       " + String(ESP.getFreeHeap())    + "\n";
    s += "heap min free   " + String(ESP.getMinFreeHeap()) + "\n";
    s += "heap max block  " + String(ESP.getMaxAllocHeap()) + "\n";
    s += "psram free      " + String(ESP.getFreePsram()) + " of " + String(ESP.getPsramSize()) + "\n";
    UBaseType_t n = readTasks();
    s += "tasks           " + String((unsigned)n) + "\n";
    for (UBaseType_t i = 0; i < n; i++){
        char name[24];
        snprintf(name, sizeof(name), "  %-16s ", sysTasks[i].pcTaskName);
        s += name + taskLine(sysTasks[i]) + "\n";
    }
    request->send(200, "text/plain", s);
}

// -----------------------------------------------------------------------------
// buildAboutTable
// Writes the read-only OTA + network info table into PageBuf.
// -----------------------------------------------------------------------------
static void buildAboutTable() {
    String ip     = WiFi.localIP().toString();
    String otaCmd = "pio run -e ota -t upload --upload-port " + ip;
    String ssid   = WiFi.SSID() + "  (" + WiFiSource + ")";
    String gw     = WiFi.gatewayIP().toString() + "  /  " + WiFi.subnetMask().toString();
    String mac    = WiFi.macAddress();
    String mdns   = "http://" + WiFiHostName() + ".local/";
    String rssi   = String(WiFi.RSSI()) + " dBm";   // at page load

    resetPageBuf();
    WT_StartPage();
    WT_StartTable("PARAMETER", "VALUE", "");

    // Row 1 — OTA status (live: JS setOtaStatus() updates value + colour via WebSocket)
    WT_StartLine();
    WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text", "OTA (Over-The-Air) Updates", "OTA_L1",     "",          nullptr);
    WT_Column(WT_INPUT, WT_FIRST, WT_LAST, "text", "Ready to Update",            "ota_status", COLOR_GREEN, nullptr, "readonly:");
    WT_EndLine();

    aboutRow("OTA_L2", "OTA Terminal Command", "OTA_V2", otaCmd.c_str(), "font-size:11px");

    WT_SectionHeader("Network");
    aboutRow("NET_L1", "WiFi Network",       "NET_V1", ssid.c_str());
    aboutRow("NET_L6", "Signal (dBm)",       "NET_V6", rssi.c_str());
    aboutRow("NET_L2", "IP Address",         "NET_V2", ip.c_str());
    aboutRow("NET_L3", "Gateway / Subnet",   "NET_V3", gw.c_str());
    aboutRow("NET_L4", "mDNS",               "NET_V4", mdns.c_str());
    aboutRow("NET_L5", "MAC Address",        "NET_V5", mac.c_str());

    // Memory + Tasks: diagnostics, only with SETTINGS "Show Memory & Tasks on ABOUT".
    // (/api/sysinfo always works, whatever this setting.)
    if (Params->ShowSysInfo){
    // Snapshot at page load — reload the page (or GET /api/sysinfo) to refresh.
    String up     = uptimeText();
    String hFree  = String(ESP.getFreeHeap())     + " B";
    String hMin   = String(ESP.getMinFreeHeap())  + " B";
    String hBlk   = String(ESP.getMaxAllocHeap()) + " B";
    String psram  = String(ESP.getFreePsram() / 1024) + " of " + String(ESP.getPsramSize() / 1024) + " KB";
    WT_SectionHeader("Memory  (at page load - /api/sysinfo for plain text)");
    aboutRow("MEM_L0", "Uptime",                     "MEM_V0", up.c_str());
    aboutRow("MEM_L1", "Heap Free",                  "MEM_V1", hFree.c_str());
    aboutRow("MEM_L2", "Heap Lowest since Boot",     "MEM_V2", hMin.c_str());
    aboutRow("MEM_L3", "Heap Largest Block",         "MEM_V3", hBlk.c_str(),
             ESP.getMaxAllocHeap() < HEAP_MAXBLK_MIN ? COLOR_RED : "");
    aboutRow("MEM_L4", "PSRAM Free",                 "MEM_V4", psram.c_str());

    UBaseType_t n = readTasks();
    String hdr = "Tasks  (" + String((unsigned)n) + ")";
    WT_SectionHeader(hdr.c_str());
    for (UBaseType_t i = 0; i < n; i++){
        char idL[12], idV[12];
        snprintf(idL, sizeof(idL), "TSK_L%u", (unsigned)i);
        snprintf(idV, sizeof(idV), "TSK_V%u", (unsigned)i);
        String line = taskLine(sysTasks[i]);
        aboutRow(idL, sysTasks[i].pcTaskName, idV, line.c_str(), "font-size:12px");
    }
    }   // ShowSysInfo

    WT_EndPageClass("info");   // display-only: labels 45%, long values wrap
}

// -----------------------------------------------------------------------------
// buildProjectInfoDiv
// Appends a centred HTML block showing project name, build date, and PCB type.
// -----------------------------------------------------------------------------
static void buildProjectInfoDiv() {
    const char* bd = __DATE__;
    char bDay[3] = { (bd[4]==' ' ? '0' : bd[4]), bd[5], '\0' };
    char bMon[4] = { bd[0], bd[1], bd[2], '\0' };
    char bYr[5]  = { bd[7], bd[8], bd[9], bd[10], '\0' };
    char buildDate[32];
    snprintf(buildDate, sizeof(buildDate), "%s %s %s %s", bDay, bMon, bYr, __TIME__);

    appendHtmlStr("<div><br></div>");
    appendHtmlStr("<div style='font-size:14px; text-align:center;'>");
    appendHtmlStr("<b>Project:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</b>Rain Gauge (tipping bucket)");
    appendHtmlStr("<br><b>Firmware:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</b>" FW_VERSION);
    appendHtmlStr("<br><b>Build Date:&nbsp;&nbsp;&nbsp;&nbsp;</b>");
    appendHtmlStr(buildDate);
    appendHtmlStr("<br><b>PCB:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</b>RAINMETER_V1");
    appendHtmlStr("<br><b>Board:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</b>WEMOS LOLIN S2 Mini");
    // IP and MAC are in the Network section above — not repeated here.
    appendHtmlStr("</div>");
}

// =============================================================================
// About Page — Route Handler
// =============================================================================

// -----------------------------------------------------------------------------
// handleAbout  (GET /about)
// -----------------------------------------------------------------------------
void handleAbout(AsyncWebServerRequest *request) {
    if (!beginHtmlStream(request)) return;
    buildAboutTable();
    resetHtmlBuf();
    appendHtmlStr(webHeadPage);
    append_FileAction(FA_MESS_06);
    appendMainPageSel(PS_ABOUT);
    appendHtmlStr(webScriptTag);
    appendHtmlBuf(PageBuf, getPageBufLen());
    webRebootButton();
    // WiFi setup: restart into the "RainGauge-Setup" hotspot (05_WiFi.cpp).
    appendHtmlStr("<div class='btns'><button class='btn btn-or' onclick=\"if(confirm("
                  "'Restart into WiFi setup?\\n\\nThe gauge leaves your WiFi and opens the hotspot RainGauge-Setup.\\n"
                  "Connect a phone to it to choose the network (5 minutes, then it returns to the old setting).'))"
                  "fetch('/wifisetup',{method:'POST'})\">WIFI SETUP</button></div>");
    appendFwUpdateBlock();      // 22_Update.cpp
    buildProjectInfoDiv();
    appendHtmlStr(webHTMLEnd);
    sendHtmlPage(request);
}
