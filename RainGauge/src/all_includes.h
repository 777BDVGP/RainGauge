// =============================================================================
// all_includes.h — Single Aggregate Include for All Firmware Modules
// =============================================================================
#pragma once

#include "01_Main.h"        // Global flags, libraries, pin aliases, watchdog
#include "02_Time.h"        // Software timer engine, NTP, date/time utilities
#include "03_Files.h"       // SPIFFS init, file manager route handlers
#include "04_WebPages.h"    // AsyncWebServer, WebSocket, PageVar/PageElement, WT_* builders
#include "05_ThisSystem.h"  // I/O map, debounce, nav constants, AddRoutes
#include "05_WiFi.h"        // Multi-network WiFi join + saved default network
#include "08_Home.h"        // Home page, MaintainHomePage, PageVar list
#include "12_Settings.h"    // AllParams class, settings page, PageBuf / HtmlBuf
#include "10_Reports.h"     // SaveGenReport, Reports page  (needs Params -> after 12)
#include "13_Ntfy.h"        // NTFY push-notification page
#include "15_About.h"       // OTA update service, About page, ArduinoOTA
#include "16_Rain.h"        // Tip counter, calibration, rainfall log
#include "17_Temperature.h" // DS18S20 internal / external temperatures
#include "18_Simulation.h"  // Simulation mode (tips + temperatures without hardware)
#include "19_Irrigation.h"  // "Irrigate today?" answer for an irrigation controller (/api/irrigate)
#include "20_WaterBalance.h" // FAO-56 soil-water balance behind that answer
#include "21_TempHistory.h" // Daily min/max temperature history + records
#include "22_Update.h"       // Firmware update from GitHub / uploaded .bin (ABOUT)
#include "23_Access.h"       // Remote access password (outside visitors)
#include "forward_decls.h"  // All cross-file forward declarations
