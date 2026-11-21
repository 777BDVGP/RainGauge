# sync_from_private.py - copy the firmware source from the private project
# (../RainGauge_PIO) into this public copy (RainGauge/), WITHOUT the private
# WiFi file, then check that no private WiFi name or password slipped in.
#
#   python sync_from_private.py
#
# Copied: src/ (all except wifi_secrets.h), tools/export_bin.py,
#         partitions_ota.csv.  platformio.ini is NOT copied: the public one has
#         no personal upload addresses.
# The check reads the private src/wifi_secrets.h (only on this PC) and refuses
# when any SSID or password from it occurs in the public copy.
import os, re, shutil, sys

HERE    = os.path.dirname(os.path.abspath(__file__))
PRIVATE = os.path.join(os.path.dirname(HERE), "RainGauge_PIO")
PUBLIC  = os.path.join(HERE, "RainGauge")
SKIP    = {"wifi_secrets.h"}

def copy_tree(src, dst):
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    os.makedirs(dst)
    for name in sorted(os.listdir(src)):
        if name in SKIP:
            print("  skipped (private): %s" % name)
            continue
        s = os.path.join(src, name)
        if os.path.isfile(s):
            shutil.copyfile(s, os.path.join(dst, name))

if not os.path.isdir(PRIVATE):
    sys.exit("Private project not found: %s" % PRIVATE)

print("Copying %s -> %s" % (PRIVATE, PUBLIC))
copy_tree(os.path.join(PRIVATE, "src"), os.path.join(PUBLIC, "src"))
os.makedirs(os.path.join(PUBLIC, "tools"), exist_ok=True)
shutil.copyfile(os.path.join(PRIVATE, "tools", "export_bin.py"), os.path.join(PUBLIC, "tools", "export_bin.py"))
shutil.copyfile(os.path.join(PRIVATE, "partitions_ota.csv"), os.path.join(PUBLIC, "partitions_ota.csv"))

# ── Secret check ──────────────────────────────────────────────────────────────
secrets = []
sec_file = os.path.join(PRIVATE, "src", "wifi_secrets.h")
if os.path.exists(sec_file):
    text = open(sec_file, encoding="utf-8", errors="replace").read()
    text = re.sub(r"//[^\n]*", "", text)                     # ignore comments
    for s in re.findall(r'"([^"]{4,})"', text):              # SSIDs and passwords
        secrets.append(s)
bad = []
for root, _, files in os.walk(PUBLIC):
    if ".pio" in root or "firmware_out" in root:
        continue
    for f in files:
        p = os.path.join(root, f)
        data = open(p, encoding="utf-8", errors="replace").read()
        for s in secrets:
            if s in data:
                bad.append(os.path.relpath(p, HERE))
if os.path.exists(os.path.join(PUBLIC, "src", "wifi_secrets.h")):
    bad.append("RainGauge/src/wifi_secrets.h")
if bad:
    print("STOP - private WiFi names / passwords found in: %s" % ", ".join(sorted(set(bad))))
    sys.exit(1)
print("OK - %d private strings checked, none in the public copy" % len(secrets))
