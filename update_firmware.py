# update_firmware.py - put the newest PUBLIC build on the GitHub Pages site.
#
#   1. python sync_from_private.py          (copy the source, secret check)
#   2. cd RainGauge && pio run -e usb       (build; writes firmware_out/)
#   3. python update_firmware.py "short note about this version"
#   4. git add -A && git commit && git push  - GitHub Pages serves it; every
#      gauge's ABOUT page "CHECK FOR UPDATE" now offers this version.
#
# Writes docs/firmware/raingauge.bin       (program only - online update)
#        docs/firmware/raingauge_full.bin  (bootloader + partitions + program,
#                                           browser install of a NEW board)
#        docs/version.json                 (read by CHECK FOR UPDATE)
#        docs/manifest.json                (ESP Web Tools install button)
# Refuses *_PRIVATE.bin files (built with your own wifi_secrets.h).
import glob, json, os, re, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT  = os.path.join(HERE, "RainGauge", "firmware_out")
DOCS = os.path.join(HERE, "docs")

def vkey(path):
    m = re.search(r"_v([\d.]+)(_full)?\.bin$", path)
    return [int(x) for x in m.group(1).split(".")] if m else []

apps  = [f for f in glob.glob(os.path.join(OUT, "RainGauge_v*.bin")) if not f.endswith("_full.bin") and "_PRIVATE" not in f]
fulls = [f for f in glob.glob(os.path.join(OUT, "RainGauge_v*_full.bin")) if "_PRIVATE" not in f]
if not apps or not fulls:
    sys.exit("No public build in %s - run: cd RainGauge && pio run -e usb" % OUT)
app, full = max(apps, key=vkey), max(fulls, key=vkey)
version = re.search(r"_v([\d.]+)\.bin$", app).group(1)
if vkey(full) != vkey(app):
    sys.exit("Program and full file have different versions - build again")

notes = sys.argv[1] if len(sys.argv) > 1 else ""
os.makedirs(os.path.join(DOCS, "firmware"), exist_ok=True)
shutil.copyfile(app,  os.path.join(DOCS, "firmware", "raingauge.bin"))
shutil.copyfile(full, os.path.join(DOCS, "firmware", "raingauge_full.bin"))

json.dump({"version": version, "bin": "firmware/raingauge.bin", "notes": notes},
          open(os.path.join(DOCS, "version.json"), "w"), indent=2)
json.dump({
    "name": "Rain Gauge",
    "version": version,
    "new_install_prompt_erase": True,
    "builds": [{"chipFamily": "ESP32-S2",
                "parts": [{"path": "firmware/raingauge_full.bin", "offset": 0}]}],
}, open(os.path.join(DOCS, "manifest.json"), "w"), indent=2)
print("docs/ now holds version %s  (program %d bytes, full %d bytes)" %
      (version, os.path.getsize(app), os.path.getsize(full)))
print("Next: commit and push - GitHub Pages serves it in a minute or two.")
