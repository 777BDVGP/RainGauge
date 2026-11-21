#pragma once
#include <memory>

// =============================================================================
// 12_Settings.h — System Parameters Class, Page Buffer & Settings Page Globals
// =============================================================================
// Declares the AllParams class (system name, logging flag, scheduled reboot)
// with JSON persistence, and the static PageBuf / PSRAM HtmlBuf used by all
// page builders to avoid heap fragmentation from String += operations.
// =============================================================================

// =============================================================================
// AllParams
// Holds global system configuration that is stored in /AllParams.json.
// =============================================================================
class AllParams
{
public:
	AllParams();    // Constructor
	~AllParams();   // Destructor
	void WriteData(); // Serialise settings to /AllParams.json
	void ReadData();  // Deserialise settings from /AllParams.json

	// ── System Settings ───────────────────────────────────────────────────────
	String SysName             = "Rain Gauge"; // Displayed in the H01 page header
	bool   LogGeneralEvents    = true;         // Append timestamped entries to GeneralLog.txt
	bool   ShowSysInfo         = false;        // ABOUT: show Memory + Tasks sections (diagnostics)
	bool   ShowInsideTemp      = false;        // HOME / REPORTS / SIMULATION: show the inside (internal) temperature

	// ── Scheduled Reboot ──────────────────────────────────────────────────────
	bool   RebootEnable1       = true;    // Armed/disarmed
	String RebootTime1         = "00:05"; // Reboot time (HH:MM)

	// ── Rain gauge (16_Rain.cpp) ─────────────────────────────────────────────
	float  FunnelDiaMm         = 146.0;   // funnel inlet diameter (mm)
	float  MlPerTip            = 5.0;     // water per tip — set by CALIBRATION (5 ml = placeholder)
	int    TipDebounceMs       = 250;     // pickup must be steady this long to count a tip
	int    CalWaterMl          = 500;     // water poured for a calibration (ml)
	String CalDate             = "never"; // when MlPerTip was last calibrated
	int    IrTestMin           = 10;      // IR beam self-test every N minutes (0 = off)

	// ── Irrigation answer for the controller (19_Irrigation.cpp) ────────────
	float  IrriSkipMm          = 10.0;    // skip irrigation if at least this much rain...
	int    IrriSkipHours       = 48;      // ...fell in the last this-many hours

	// ── Water balance (20_WaterBalance.cpp) ──────────────────────────────────
	String IrriMode            = "Water Balance"; // "Water Balance", "Simple Rule" or "No Irrigation"
	float  Latitude            = -26.09;  // Gauteng, South Africa (example) — set your own on SETTINGS; for the sun's energy (Ra)
	float  CropKc              = 0.6;     // garden factor: flower/shrub beds 0.5-0.7, lawn 0.8, veg 1.0
	String SoilType            = "Loam";  // Sand / Loam / Clay
	int    RootDepthMm         = 300;     // root zone depth (beds ~300 mm, lawn ~200 mm)
	int    DepletionPct        = 50;      // irrigate once this % of the soil water is used
	float  MinEffRainMm        = 2.0;     // daily rain below this does not reach the roots
	float  IrriDepthMm         = 6.5;     // water one controller irrigation adds (mm) — 10 min of spray
	                                      // heads at ~40 mm/h; measure it with the cup test (Notes)
	String IrriTime            = "10:00"; // when the controller irrigates — today's answer is latched then

	// ── The irrigation controller that reads /api/irrigate (e.g. "Prima3") ───
	String ControllerName      = "Irrigation Controller"; // shown on HOME / SETTINGS labels
};

// Controller name for page labels — never empty, no characters that break the page.
String ControllerName();

	inline AllParams *Params = nullptr;   // Global system parameters object

// ─── Settings Page Row Index Constants ───────────────────────────────────────
	#define SET_SYSNAME           0   // System name text input
	#define SET_LOG_GENERAL       1   // Log general events checkbox

	#define PAGE_VAR_SETTINGS_BASE  2  // Rows rendered by the generic per-row loop

	#define SET_REBOOT_EN1        2   // Scheduled Reboot — enable checkbox
	#define SET_REBOOT_TIME1      3   // Scheduled Reboot — HH:MM

	#define SET_FUNNEL_DIA        4   // Funnel diameter (mm)
	#define SET_ML_PER_TIP        5   // ml per tip (calibrated)
	#define SET_TIP_DEBOUNCE      6   // Tip debounce (ms)
	#define SET_CAL_WATER         7   // Calibration water (ml)

	#define SET_IRRI_SKIP_MM      8   // Skip irrigation if rain >= (mm)
	#define SET_IRRI_SKIP_HRS     9   // ...in the last (hours)

	#define SET_IRRI_MODE        10   // Water Balance / Simple Rule
	#define SET_LATITUDE         11
	#define SET_CROP_KC          12
	#define SET_SOIL_TYPE        13
	#define SET_ROOT_DEPTH       14
	#define SET_DEPLETION_PCT    15
	#define SET_MIN_EFF_RAIN     16
	#define SET_IRRI_DEPTH       17
	#define SET_IRRI_TIME        18

	#define SET_IR_TEST_MIN      19   // IR self-test interval (min, 0 = off)

	#define SET_CTRL_NAME        20   // Irrigation controller name (labels only)

	#define SET_SHOW_SYSINFO     21   // ABOUT: show Memory + Tasks checkbox

	#define SET_SHOW_INSIDE      22   // Show inside temperature checkbox

	#define PAGE_VAR_SETTINGS    23   // Total number of settings page PageVar entries

// =============================================================================
// Static Page Build Buffer  (WT_* table content only)
// =============================================================================
	inline char   PageBuf[12000];
	inline size_t PageBufPos = 0;

	inline void   resetPageBuf()                   { PageBufPos = 0; }
	inline void   appendToPageBuf(const char *s)   { while (*s && PageBufPos + 1 < sizeof(PageBuf)) PageBuf[PageBufPos++] = *s++; }
	inline void   appendToPageBuf(const String &s) { for (size_t i = 0; i < s.length() && PageBufPos + 1 < sizeof(PageBuf); ++i) PageBuf[PageBufPos++] = s[i]; }
	inline size_t getPageBufLen()                  { PageBuf[PageBufPos] = '\0'; return PageBufPos; }

// =============================================================================
// HTML Assembly Buffer  (full page — used by every page handler)
// =============================================================================
// HtmlBuf is allocated from the S2 Mini's 2 MB PSRAM once in InitFileSystem()
// via ps_malloc — costs only 4 bytes of DRAM for the pointer.
// Every page handler calls resetHtmlBuf(), fills via appendHtml*(), then
// sends with sendHtmlPage().
// =============================================================================
	inline constexpr size_t HTML_BUF_SIZE = 40000;   // PSRAM — REPORTS page is the largest
	inline char*   HtmlBuf    = nullptr;   // points into PSRAM after InitFileSystem()
	inline size_t  HtmlBufPos = 0;

	inline void   resetHtmlBuf()  { HtmlBufPos = 0; if (HtmlBuf) HtmlBuf[0] = '\0'; }
	inline size_t getHtmlBufLen() { if (HtmlBuf) HtmlBuf[HtmlBufPos] = '\0'; return HtmlBufPos; }

	// Append a null-terminated C-string into HtmlBuf (memcpy — fast)
	inline void appendHtmlStr(const char *s) {
		if (!HtmlBuf) return;
		size_t n = strlen(s);
		if (HtmlBufPos + n < HTML_BUF_SIZE - 1) {
			memcpy(HtmlBuf + HtmlBufPos, s, n);
			HtmlBufPos += n;
			HtmlBuf[HtmlBufPos] = '\0';
		}
	}

	// Append exactly n bytes (for PageBuf content which has no null at position n)
	inline void appendHtmlBuf(const char *s, size_t n) {
		if (!HtmlBuf || !n) return;
		if (HtmlBufPos + n < HTML_BUF_SIZE - 1) {
			memcpy(HtmlBuf + HtmlBufPos, s, n);
			HtmlBufPos += n;
			HtmlBuf[HtmlBufPos] = '\0';
		}
	}

	inline PageVar* SettingsList;                          // PageVar array for the Settings web page
	inline String   SetStyle = "color: black; font-size:14px"; // Default label style for settings rows

// =============================================================================
// sendHtmlPage — Stream page from a PSRAM snapshot; zero DRAM copy.
// =============================================================================
// ps_malloc() copies HtmlBuf into a fresh PSRAM buffer for this response.
// The filler callback feeds it to ESPAsyncWebServer in TCP-sized chunks with
// Content-Length set so the browser renders immediately (no chunked encoding).
// std::shared_ptr frees the snapshot when the response is delivered or
// cancelled.
// =============================================================================
	inline void sendHtmlPage(AsyncWebServerRequest *request) {
		size_t len = getHtmlBufLen();
		if (!HtmlBuf || !len) { request->send(500); return; }
		char* raw = (char*)ps_malloc(len);
		if (!raw) raw = (char*)malloc(len);
		if (!raw)  { request->send(500); return; }
		memcpy(raw, HtmlBuf, len);
		auto snap = std::shared_ptr<char>(raw, [](char* p){ free(p); });
		request->send("text/html", len,
			[snap, len](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
				size_t remaining = len - index;
				if (!remaining) return 0;
				size_t n = remaining < maxLen ? remaining : maxLen;
				memcpy(buf, snap.get() + index, n);
				return n;
			}
		);
	}
