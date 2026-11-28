#pragma once

// =============================================================================
// 23_Access.h — Remote access password (1.12.0)
// =============================================================================
// The gauge can be reached from the internet through a port forward on the
// router.  Visitors are told apart by the IP address the request comes from:
//
//   LOCAL   (10.x, 172.16-31.x, 192.168.x, 169.254.x, 127.x) — home network:
//           everything open, no password, as before.
//   OUTSIDE (any other address) — only HOME and REPORTS (RAIN / TEMPERATURE /
//           WATER BALANCE).  With a password set, a LOGIN tab appears; after
//           a correct password that browser gets OTHER (SETTINGS, FILES, NTFY,
//           ABOUT) for ACCESS_SESSION_MS (1 day) or until LOGOUT.  Without a
//           password, outside visitors can never get further than HOME/REPORTS.
//
// Protected: every page / action route except "/", the report pages, /wss.js,
// /api/irrigate, /login, /logout (AccessGuard() in AddRoutes), the upload data
// handlers, and WebSocket commands (UPDATE buttons, REBOOT …): the WebSocket
// handshake request carries the login cookie, so webSocketEvent() remembers per
// client id whether it may send commands (AccessWsAllowed()).
//
// Password: set / cleared on SETTINGS only from the home network (POST
// /setpass).  Stored as salted SHA-256 in /Access.json (hidden on FILES, like
// /WiFiConfig.json) — never in plain text, never sent to a browser.
// Sessions: random 128-bit key in the cookie "rgsid" (HttpOnly), kept in RAM
// (ACCESS_SESSIONS slots) — a restart logs everybody out.
// Wrong passwords: after ACCESS_MAX_FAILS the login is locked for
// ACCESS_LOCK_MS for everybody.  Logins / failures / lock are written to the
// General Log by MaintainAccess() (loop) — handlers only count.
//
// The site is plain http: the password crosses the internet unencrypted.
// Use one that is not used anywhere else.
//
// If the router replaces the visitor's address with its own (some do on a port
// forward), every visitor looks LOCAL.  ABOUT shows "Your connection" so this
// can be checked from a phone on mobile data.
// =============================================================================

	#define ACCESS_SESSIONS     6
	#define ACCESS_SESSION_MS   (24UL * 60UL * 60UL * 1000UL)   // 1 day
	#define ACCESS_MAX_FAILS    5
	#define ACCESS_LOCK_MS      (15UL * 60UL * 1000UL)          // 15 min
	#define ACCESS_PASS_MIN     6                               // shortest password accepted
	#define ACCESS_PASS_MAX     64

	inline const char* StrAccessFile = "/Access.json";

	// Set by the page wrappers just before a page is built (all page handlers run
	// one after the other in async_tcp) — read by appendMainPageSel() and the
	// pages that hide buttons.
	inline bool NavAuthed  = true;    // may see OTHER / change things
	inline bool NavOutside = false;   // request came from outside the home network
