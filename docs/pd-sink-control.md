# USB-C PD Sink Fan Voltage Control (CH224K)

How MobiFan sets the fan's supply voltage: a CH224K USB-C PD sink/trigger chip
(module: 3C3PDSink01 / HW-443) requests one of 4 fixed voltages from the USB-C
power supply, selected by the ESP32 driving the chip's CFG1-3 pins.

This is the only fan-drive path in the firmware. It replaced an MT3608 boost
converter steered by PWM feedback-current injection (removed in `4e51d83`);
there is no longer any PWM, RC filter, trim pot, or continuous voltage control.

## Voltage steps

The four PD steps and the manual levels that map onto them
([src/config.h](../src/config.h) `MANUAL_VOLTS`, `PD_VOLTS`):

| Step | Voltage | Manual level | Notes |
|------|---------|--------------|-------|
| 0    | 5 V     | 1            | fan barely spins — the floor, there is no true off |
| 1    | 9 V     | 2            | quiet |
| 2    | 12 V    | 3            | the fan's rated voltage (also the boot manual level) |
| 3    | 15 V    | 4            | slight overvoltage: more airflow, more noise |

The fan (Thermaltake Pure 20) is rated for 12 V. In auto mode the levels select a
temperature *ramp* rather than a step, and the resulting continuous power % is
quantized onto these same four voltages by `FanControl::setPowerPercent()` in
equal 25 % bands — see the auto table in the [main README](../README.md).

## CH224K CFG pin truth table

| Voltage | CFG1 | CFG2 | CFG3 |
|---------|------|------|------|
| 5 V     | HIGH | x    | x    |
| 9 V     | LOW  | LOW  | LOW  |
| 12 V    | LOW  | LOW  | HIGH |
| 15 V    | LOW  | HIGH | HIGH |
| 20 V    | LOW  | HIGH | LOW  |

20 V is never requested (too high for the fan) — `PD_VOLTS` stops at 15 V.
This table is mirrored in `FanControl::applyCfg()`; keep both in sync.

## Wiring

Note the pin order: **CFG1 is GPIO2 and CFG3 is GPIO0**, not the other way
around (`PIN_PD_CFG1/2/3` in [src/config.h](../src/config.h)).

```
ESP32-C3           CH224K module (HW-443)
─────────          ──────────────────────
GPIO2  ──────────► CFG1
GPIO1  ──────────► CFG2
GPIO0  ──────────► CFG3
                   VBUS out ──► Fan + (positive supply)
                   GND      ──► Fan - (ground)
```

- **No external resistors needed** between the GPIOs and CFG pins — the
  CH224K has internal pull-ups and its VDD is 3.3 V (from its internal LDO
  off VBUS), matching ESP32-C3 GPIO levels directly. The GPIOs are plain
  push-pull outputs.
- The module's USB-C connector is the power input (connects to the van's
  USB-C PD charger/PSU).
- The module's output provides the negotiated voltage (5/9/12/15 V) to the
  fan. Two fans go in parallel; the tach wire is taken from one of them only.
- **The ESP32 board must have its own fixed 5 V supply.** The PD output is the
  fan rail and gets renegotiated up to 15 V, so it cannot power the board.

## Boot / fail-safe behavior

- Before firmware runs (GPIOs floating): CH224K defaults to 5 V (CFG1
  pulled high internally). Fan spins at minimum — safe.
- `fan.begin()` drives CFG1 HIGH explicitly, then `setPowerPercent(0)`
  keeps 5 V.
- If firmware crashes: GPIOs go high-Z, CH224K falls back to 5 V. Safe.

Unlike the old boost variant — where a dead GPIO left the output at the trim
pot's 12 V anchor — the safe state here is the lowest voltage, so `fan.begin()`
being first in `setup()` is insurance rather than a hard requirement.

## Dynamic switching

The CH224K renegotiates with the source whenever CFG pins change. The
renegotiation takes ~100–300 ms; during this time the output may briefly
drop. The fan's inertia carries it through — no special handling needed
in firmware (stall detection's 5-second timeout is far longer than any
renegotiation). `setPowerPercent()` only touches the CFG pins when the
quantized step actually changes, so holding a steady power level never
renegotiates.

## USB-C source compatibility

Not all sources support all PDOs. If a requested voltage is unavailable,
the CH224K falls back to the highest available (or 5 V). The firmware has
no way to detect this — it sets CFG for 15 V but might get 12 V. This is
acceptable: the fan just runs slightly slower than "maximum". Be aware that
the voltage on the display and in the serial log is the *requested* voltage
(`FanControl::targetVolts()`), never a measured one.

## Build

The PD sink needs no build flag of its own — it is the only fan-drive
implementation, compiled into every env:

```sh
pio run                 # build (default env: esp32c3-oled-ds18b20)
pio run -t upload       # flash
```
