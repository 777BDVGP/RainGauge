# make_enclosure_zip.py - rebuild the 3D-print download ZIP from the STL files
# in Enclosure/, so it always matches the drawings.
#
#   python make_enclosure_zip.py
#
# Writes Enclosure/Enclosure_3D_Print_STL.zip  (all *.stl)
# Run it after changing an STL file, before committing. The ZIP is built
# with fixed file dates, so it only changes when an STL actually changed.
import glob, os, zipfile

HERE  = os.path.dirname(os.path.abspath(__file__))
ENCL  = os.path.join(HERE, "Enclosure")
NAME  = "Enclosure_3D_Print_STL.zip"
FIXED = (2026, 1, 1, 0, 0, 0)

files = sorted(glob.glob(os.path.join(ENCL, "*.stl")))
if not files:
    raise SystemExit("No STL files in %s" % ENCL)
path = os.path.join(ENCL, NAME)
with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
    for f in files:
        info = zipfile.ZipInfo(os.path.basename(f), FIXED)
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o644 << 16
        with open(f, "rb") as fh:
            z.writestr(info, fh.read())
print("%s  %d files  %d bytes" % (NAME, len(files), os.path.getsize(path)))
for f in files:
    print("    " + os.path.basename(f))
