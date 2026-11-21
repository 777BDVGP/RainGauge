# Rain Gauge — Printing and Assembly

## Printing the parts

All parts were printed in **ABS** on an FDM 3D printer.

**Print them exactly as they are in the STL files — do not rotate them.**
Every part is already saved in its print position: when you load an STL into a
slicer such as **Cura**, it lies on the build plate the right way up, with its
flat side down. Just place it on the plate and slice.

| File | Part | Size (X × Y × height) |
|---|---|---|
| `RM_Top.stl` | Top with the funnel — 146 mm across (= *Funnel Diameter* on the SETTINGS page) | 146 × 146 × 88.6 mm |
| `RM_Mid.stl` | Middle section | 146 × 146 × 74.5 mm |
| `RM_Base.stl` | Base | 139 × 115 × 65 mm |
| `RainMeterTipper.stl` | Tipping bucket | 98 × 23 × 28 mm |
| `PCBLift.stl` | PCB lift / spacer | 44 × 24 × 3 mm |
| `TempSupportPlug.stl` | Plug for the outside temperature sensor | 21 × 21 × 17.5 mm |

Notes:

- The top and middle need a build plate of at least **150 × 150 mm**.
- ABS warps and cracks when it cools unevenly: an enclosed printer (or a
  draught-free room) and a heated bed help.
- `RainMeterTipper.stl` touches the plate with only a small area — a **brim**
  helps it stay down.
- The tipping bucket must swing freely and the same way to both sides: clean
  any strings or blobs from the pivot after printing.
- If you change the funnel size, set the new inside diameter on the SETTINGS
  page (*Funnel Diameter*) — the rain in mm is worked out from it.

## Assembly

*(Assembly steps follow.)*

## After assembly

1. Install the firmware and set up WiFi — see the
   [install page](https://777bdvgp.github.io/RainGauge/).
2. Tip the bucket by hand and watch **Bucket Position** on the HOME page:
   it must change between **OPEN** (green) and **BROKEN** (red) on every tip.
3. **NTFY page → TEST IR SENSOR NOW** (with the beam OPEN) must report *OK*.
4. Stand the gauge level and **calibrate** it (SETTINGS → CALIBRATION).
5. **SETTINGS → START FRESH** to clear the test tips.
