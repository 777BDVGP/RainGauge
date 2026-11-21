#pragma once

	// NTFY page "IR SENSOR TEST" block (13_Ntfy.cpp)
	struct IrBeamView { String beam; String style; String can; };

// =============================================================================
// 13_Ntfy.h — NTFY.sh Push Notification Parameters & Page Declarations
// =============================================================================
// Same settings, page and MessageNTFY() call as the author's other controllers,
// but no NtfyTask: MessageNTFY() queues the text and MaintainNtfy() (loop)
// sends it as an HTTP POST to http://ntfy.sh/<topic> on an AsyncClient, which
// runs in the async_tcp task the web server already has.  loop() (tip
// counting) never waits on the network.  Settings live in /NTFY.json.
// =============================================================================

// =============================================================================
// NtfyParams
// =============================================================================
class NtfyParams
{
public:
	NtfyParams();
	~NtfyParams();
	void WriteData();
	void ReadData();

	String NtfyCode           = " ";     // NTFY topic name
	String NtfyPriority       = "urgent"; // alert level on the phone: urgent / high / default / low / min (X-Priority)
	bool   EnableIrFaultNotify = false;  // alert when the IR self-test fails / recovers

	// Outside temperature alerts (CheckTempAlerts, every reading = 10 s)
	bool   EnableTempHigh     = false;   // alert when outside rises to TempHighC
	float  TempHighC          = 35.0;
	bool   EnableTempLow      = false;   // alert when outside drops to TempLowC
	float  TempLowC           = 0.0;
};

// ─── NTFY Page Row Index Constants ───────────────────────────────────────────
#define NTFY_CODE            0   // NTFY topic name
#define NTFY_EN_IR_FAULT     1   // Enable checkbox — IR sensor fault alert
#define NTFY_EN_T_HIGH       2   // Enable checkbox — outside high temperature alert
#define NTFY_T_HIGH          3   // ...at this temperature (°C)
#define NTFY_EN_T_LOW        4   // Enable checkbox — outside low temperature alert
#define NTFY_T_LOW           5   // ...at this temperature (°C)
#define NTFY_PRIORITY        6   // Alert level (X-Priority) dropdown
#define PAGE_VAR_NTFY        7

#define TEMP_ALERT_HYST_C    1.0f  // must come back this far past the limit before it can alert again

#define ID_NTFY            200   // WebSocket IDs of the NTFY page (200+)
#define NTFY_HOST          "ntfy.sh"
#define NTFY_QUEUE          2     // messages waiting while one is being sent
#define NTFY_TIMEOUT_SEC   10     // no reply this long -> connection closed

// ─── Inline Globals ───────────────────────────────────────────────────────────
inline NtfyParams   *NtfyData       = nullptr;
inline PageVar      *NtfyList       = nullptr;
inline String        NtfyStyle      = "";
inline bool          NtfyPageObject = false;
inline volatile bool NtfyTestRequested = false;   // set by /testntfy (async_tcp), handled in loop
