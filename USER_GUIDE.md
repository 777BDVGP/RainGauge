# Rain Gauge — User Guide

How to set up and use the rain gauge once it is built and the firmware is
installed (see the [README](README.md) for building and installing).

Everything is done from the gauge's own web pages: open
`http://raingauge.local/` (or the gauge's fixed IP) in a browser on the same
WiFi — phone or PC. The pages need no login — anyone on your WiFi can open them.

---

## 1. First setup

1. **WiFi** — a new gauge opens the hotspot **RainGauge-Setup** (LED blinking
   fast). Connect a phone, pick your network, type the password, choose
   **Automatic (DHCP)** or a **fixed IP** and press **SAVE & RESTART**. Use a
   fixed IP when an irrigation controller must read the gauge.
2. **Time zone** — SETTINGS → **Time Zone**. The gauge starts on Greenwich
   (GMT); pick your zone and press **UPDATE** — the time changes at once.
   Set it before the gauge collects rain: the reports are kept in local time.
3. **Check the sensor** — tip the bucket by hand and watch **Bucket Position**
   on HOME change between **OPEN** (green) and **BROKEN** (red) on every tip.
4. **Calibrate** — SETTINGS → **CALIBRATION** (section 5).
5. **Start fresh** — SETTINGS → **START FRESH** clears the test tips.
6. **Irrigation** — choose the **Irrigation Answer Method** and set your
   latitude, soil and garden on SETTINGS (section 4), or choose
   **No Irrigation** for a plain rain meter.
7. **Phone alerts** (optional) — NTFY page (section 7).

## 2. The pages

The menu at the top: **HOME** | **REPORTS** | **OTHER**. REPORTS and OTHER
each open a second row of tabs:

| Tab | Second row |
|---|---|
| REPORTS | **RAIN** \| **TEMPERATURE** \| **WATER BALANCE** (hidden with *No Irrigation*) |
| OTHER | **SETTINGS** \| **FILES** \| **NTFY** \| **ABOUT** |

CALIBRATION and SIMULATION are opened from SETTINGS. Where this guide says
e.g. "SETTINGS" or "NTFY page", find it under OTHER.

Visitors from **outside** your home network (e.g. through a port forward on the
router) see only **HOME** | **REPORTS**, plus **LOGIN** when a password is set —
see section 15.

## 3. HOME — live values

| Section | Shows |
|---|---|
| Rainfall | **Rain Today**, **Rain This Month**, **Last Tip**, **Tips Since Boot**, **Rain per Tip** |
| Bucket Position (IR beam) | **OPEN** (green) / **BROKEN** (red) — changes on every tip |
| Temperature | **Outside** now, **Outside Today Min / Max** (inside temperature too, if switched on) |
| Irrigation | **Irrigate Today?** with the reason, **Soil Water**, **Evaporation Yesterday (ET0)** — hidden with *No Irrigation*. With *Water Balance*, tick **Show help (how the irrigation answer works)** below it for an explanation with your own settings |

The values update by themselves while the page is open.

## 4. The irrigation answer

The gauge tells an irrigation controller whether the garden needs water
today. Choose the method on SETTINGS → **Irrigation Answer Method**:

| Method | How it decides |
|---|---|
| **Water Balance** | Keeps a daily soil water balance (FAO-56): rain in, evaporation out (worked out from the temperatures and your **latitude**), irrigation in. Waters when the soil has used **Irrigate when Soil Water Used (%)** of what the root zone holds |
| **Simple Rule** | Skips watering when at least **Skip Irrigation if Rain >= (mm)** fell **...in the Last (hours)** |
| **No Irrigation** | Plain rain meter: the irrigation parts disappear from the pages; a controller that still asks gets *irrigate* |

Water Balance settings:

| Setting | Meaning |
|---|---|
| Latitude (deg, South = −) | your position, for the sun's energy, e.g. −26.1 for Gauteng |
| Garden Factor Kc | how thirsty the garden is: beds 0.6, lawn 0.8, vegetables 1.0 |
| Soil Type | Sand / Loam / Clay — how much water the soil holds |
| Root Depth (mm) | beds about 300, lawn about 200 |
| Irrigate when Soil Water Used (%) | default 50 |
| Ignore Daily Rain below (mm) | small showers that never reach the roots |
| One Irrigation Adds (mm) | how much one watering by the controller gives (measure it with cups) |
| Controller Irrigates At | the time the controller waters — today's answer is fixed then |
| Irrigation Controller Name | the name used on the pages |

**For the controller:** `GET http://<gauge ip>/api/irrigate` returns e.g.
`{"irrigate":false,"reason":"Skip - soil still has water: …","rain_mm":12.4,…}`.
Any device that can make a web request can read it; only `"irrigate"`
(true / false) matters. Check the answer against your own garden for a while
before relying on it.

## 5. Calibration

The gauge must learn how much water one tip of the bucket is.

1. Set the funnel size (**Funnel Diameter (mm)**, default 146) and the amount
   of water you will pour (**Calibration Water (ml)**, default 500) on SETTINGS.
2. Stand the gauge **perfectly level**; water must drain freely at the bottom.
3. SETTINGS → **CALIBRATION** → **START** (no rainfall is logged meanwhile).
4. Pour exactly that amount of water **slowly** into the funnel (a few minutes).
5. When it has all drained, press **CALCULATE**, then **SAVE**.

While calibrating, an orange **CALIBRATING** banner shows the running time
and an **auto-stop** countdown that goes back to 3:00 on every tip, so a slow
pour of any length is fine; a bar shows roughly how much water has gone
through. The page also shows the live **Bucket Position**.

Counting stops by itself 3 minutes after the last tip; **STOP** ends it at
once, and so does leaving the CALIBRATION page (the tips are kept, so
CALCULATE still works when you come back). The result shows as
"ml per tip (bucket size) = mm of rain per tip", and on SETTINGS as
**Water per Tip (ml) - calibrated**.
Calibrate again after moving or re-levelling the gauge.

## 6. Reports

REPORTS has three pages:

| Page | Report | Shows |
|---|---|---|
| RAIN | Rainfall per Month | the total of every month, newest first |
| RAIN | Rainfall per Day | every day with rain in this and last month, newest first |
| RAIN | Rainfall per Hour | every hour with rain today and yesterday, newest first |
| TEMPERATURE | Temperature records | lowest and highest outside temperature ever, with the date; **RESET TEMPERATURE RECORDS** starts only these again |
| TEMPERATURE | Temperature per Day | daily min / max |
| WATER BALANCE | Water balance | day by day: rain, evaporation, soil water, the answer (with *Water Balance*; tab hidden with *No Irrigation*) |

The **General Log** (what the gauge did, e.g. restarts, WiFi, alerts) is on
the FILES page as `GeneralLog.txt`.

## 7. Phone alerts (NTFY) and the sensor test

The gauge can send a push message to your phone through the free
[ntfy](https://ntfy.sh) app.

1. Install the ntfy app and **subscribe** to a topic name of your own (make it
   unusual — anyone who knows the name can read it).
2. NTFY page: enter that name as **NTFY Topic Name**, choose the **Alert
   Level (on the phone)** (urgent … min), tick the alerts you want, press
   **UPDATE**, then **TEST NTFY** — the test message must appear on the phone.

| Alert | When |
|---|---|
| Alert on IR Sensor Fault | the beam self-test fails twice in a row (and an "OK again" when it recovers) |
| Alert when Outside reaches (°C) | at or above your limit (default 35 °C) — once, and again only after it dropped 1 °C |
| Alert when Outside drops to (°C) | at or below your limit (default 0 °C) |

**IR sensor test:** the gauge checks its own infrared beam every
**IR Self-Test every (min)** minutes (SETTINGS, default 10, 0 = off) while
the bucket is still. The NTFY page shows the bucket position and the last
result; **TEST IR SENSOR NOW** runs it at once (only with the beam OPEN).

## 8. Settings

| Section | Setting | What it does |
|---|---|---|
| System | System Name | the name at the top of every page |
| | Log General Events | keep the General Log |
| | Time Zone (daylight saving automatic) | 26 zones; Greenwich out of the box; applied at once |
| | Enable Reboot + Scheduled Reboot @ Time | a daily restart (default 00:05) |
| | Show Memory & Tasks on ABOUT | technical readout |
| | Show PC Upload (OTA) on ABOUT | for developers who upload from PlatformIO; off = rows hidden (normal updates are on ABOUT → CHECK FOR UPDATE) |
| | Show Inside Temperature (HOME, TEMPERATURE) | for a second sensor inside the box |
| Rain Gauge | Funnel Diameter, Water per Tip, Calibration Water | section 5 |
| | Tip Debounce (ms) | how long the beam must be steady to count a tip (default 250) |
| | IR Self-Test every (min, 0 = off) | section 7 |
| Irrigation | method and its settings | section 4 |
| WiFi Networks | list, ADD NETWORK, DELETE | section 9 |
| | **START FRESH (after installation)** | clears rainfall, water balance, temperature history and the General Log; keeps settings, calibration, NTFY and WiFi; restarts |
| | CALIBRATION / SIMULATION | sections 5 and 10 |

Press **UPDATE** to save.

## 9. WiFi networks

SETTINGS → **WiFi Networks** lists the saved networks (up to 8, passwords
never shown) with **DELETE**, and **ADD NETWORK** for another one —
automatic IP or fixed IP (an empty gateway is learned from the router). The
**Networks in range** list above the name box shows the networks the gauge can
see right now (strongest first, open / locked, already saved) — pick one
instead of typing the name; **SCAN AGAIN** refreshes it. At
start-up the gauge tries the networks in range, strongest first. When several
access points share one network name (mesh / multi-AP), it joins the one with
the strongest signal; the General Log shows which access point (MAC), channel
and signal it chose. If none can
be reached for about 5 minutes, the **RainGauge-Setup** hotspot opens for
5 minutes so you can add one from a phone, then the gauge restarts and tries
again.

**Fast rejoin:** after a restart (not after a power cut) the gauge first joins
the access point it used last, directly and without scanning — usually in well
under a second ("WiFi: fast rejoin …" in the General Log). Only if that fails
within 8 seconds does it do the full search above. After a power cut it always
does the full search, so it picks the strongest access point again.

## 10. Simulation

Try every page without the PCB or without rain: SETTINGS → **SIMULATION**,
tick **Simulation Mode**, **UPDATE**. The pickup and temperature sensors are
then ignored and everything goes to separate *Sim* files — the real
rainfall log is never touched.

- **1 TIP / 10 TIPS / POUR CALIBRATION WATER** — make tips by hand.
- **Simulated Rain (mm/h)** — steady rain; **Outside Min / Max**,
  **Inside** — made-up temperatures.
- **GENERATE 90 DAYS HISTORY** — fills the reports with made-up data;
  **CLEAR SIM LOG** removes it.

Switch Simulation Mode off again before real use.

## 11. ABOUT — updates and network

- **CHECK FOR UPDATE** compares the gauge with the version on GitHub;
  **INSTALL FROM GITHUB** downloads it (about 20 s) and restarts. Settings,
  calibration and rain history are kept; a failed download leaves the old
  firmware running.
- No internet at the gauge? Download `raingauge.bin` from the
  [install page](https://777bdvgp.github.io/RainGauge/) and use **UPLOAD &
  INSTALL**.
- **Network:** WiFi network, signal, IP and MAC address. **REBOOT** restarts
  the gauge.

## 12. Files

FILES lists, views, downloads, uploads and deletes the files on the gauge:
the rainfall log, temperature and water balance history, the General Log and
the settings. Download them now and then as a backup. The WiFi file is never
shown.

## 13. What the gauge does by itself

- Counts every tip and keeps the rainfall per hour, day and month.
- Reads the outside temperature every 10 seconds; keeps daily min / max and
  records.
- Works out the water balance once a day and fixes today's irrigation answer
  at **Controller Irrigates At**.
- Tests its infrared beam every few minutes.
- Reconnects to WiFi after a dropout; restarts once a day (if enabled).
- Pings the router every 15 seconds and restarts if it gets no answer for
  1 minute (the gauge has dropped off the network).
- Restarts if it has to refuse pages ("Low memory — retrying") for 30 seconds
  in a row. Every restart is written to the General Log with the reason.
- Takes the time from the internet.

## 14. Limits

| What | Limit |
|---|---|
| Saved WiFi networks | 8 (2.4 GHz only) |
| Time zone | 26 zones to choose from (SETTINGS); not every country is in the list |
| Web pages / updates over WiFi | no password on your home network — keep the gauge on a WiFi you trust. From outside: password, plain http (section 15) |

## 15. Remote access (from outside your home)

If the router forwards a port to the gauge, anybody on the internet can open it.
The gauge tells visitors apart by their address:

| Visitor | Menu | Can change things? |
|---|---|---|
| Your home network | HOME \| REPORTS \| OTHER — no password | yes |
| Outside, not logged in | HOME \| REPORTS \| **LOGIN** | no — HOME and REPORTS only, they still update live |
| Outside, logged in | HOME \| REPORTS \| OTHER, with **LOGOUT** in the OTHER row | yes |

1. **Set the password** at home: SETTINGS → **REMOTE ACCESS** (at the bottom),
   6 to 64 characters, **SET PASSWORD**. This block is only shown on your home
   network — the password can never be changed from outside. **CLEAR** removes it.
2. **Without a password** (the default) the LOGIN tab is hidden: outside visitors
   can only ever see HOME and REPORTS.
3. **From outside:** LOGIN → password → you stay logged in for **1 day** on that
   browser, or until LOGOUT. A restart of the gauge logs everybody out.
4. **Wrong passwords:** after 5 in a row the login is locked for 15 minutes.
   Every outside login and every wrong password is written to the General Log
   with the visitor's address.
5. **Check it:** ABOUT → **Your connection** shows "Home network" or "Outside"
   with the address the gauge sees. Try it once from a phone on mobile data
   (WiFi off): the menu must show only HOME | REPORTS. If it shows OTHER, your
   router hides the visitor's address on the port forward — then do not forward
   a port to the gauge.

The password is stored scrambled (salted hash) in `/Access.json`, which, like
`/WiFiConfig.json`, cannot be listed or downloaded on FILES. The gauge uses plain
**http**, not https: the password crosses the internet unencrypted, so use one you
do not use anywhere else. `/api/irrigate` stays open, so an irrigation controller
on your network keeps working.
