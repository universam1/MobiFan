# USB-C PD Sink Fan Voltage Control (CH224K)

Alternative to the MT3608 boost converter — uses a CH224K USB-C PD
sink/trigger chip (module: 3C3PDSink01 / HW-443) to request one of 4
fixed voltages from the USB-C power supply.

## Voltage steps

| Step | Voltage | Manual level | Auto behavior |
|------|---------|--------------|---------------|
| 0    | 5 V     | Level 0      | (below floor) |
| 1    | 9 V     | Level 1      | Cool temps    |
| 2    | 12 V    | Level 2      | Moderate      |
| 3    | 15 V    | Level 3      | Hot           |

The fan (Thermaltake Pure 20) is rated for 12 V; 15 V is a slight
overvoltage that increases airflow at the cost of some extra noise.
At 5 V the fan barely spins (there is no true off, same as the boost
variant).

## CH224K CFG pin truth table

| Voltage | CFG1 | CFG2 | CFG3 |
|---------|------|------|------|
| 5 V     | HIGH | x    | x    |
| 9 V     | LOW  | LOW  | LOW  |
| 12 V    | LOW  | LOW  | HIGH |
| 15 V    | LOW  | HIGH | HIGH |
| 20 V    | LOW  | HIGH | LOW  |

20 V is not used (too high for the fan).

## Wiring

```
ESP32-C3           CH224K module (HW-443)
─────────          ──────────────────────
GPIO0  ──────────► CFG1
GPIO1  ──────────► CFG2
GPIO2  ──────────► CFG3
                   VBUS out ──► Fan + (positive supply)
                   GND      ──► Fan - (ground)
```

- **No external resistors needed** between the GPIOs and CFG pins — the
  CH224K has internal pull-ups and its VDD is 3.3 V (from its internal LDO
  off VBUS), matching ESP32-C3 GPIO levels directly.
- The module's USB-C connector is the power input (connects to the van's
  USB-C PD charger/PSU).
- The module's output provides the negotiated voltage (5/9/12/15 V) to the
  fan.

## Boot / fail-safe behavior

- Before firmware runs (GPIOs floating): CH224K defaults to 5 V (CFG1
  pulled high internally). Fan spins at minimum — safe.
- `fan.begin()` drives CFG1 HIGH explicitly, then `setPowerPercent(0)`
  keeps 5 V.
- If firmware crashes: GPIOs go high-Z, CH224K falls back to 5 V. Safe.

## Dynamic switching

The CH224K renegotiates with the source whenever CFG pins change. The
renegotiation takes ~100–300 ms; during this time the output may briefly
drop. The fan's inertia carries it through — no special handling needed
in firmware (stall detection's 5-second timeout is far longer than any
renegotiation).

## USB-C source compatibility

Not all sources support all PDOs. If a requested voltage is unavailable,
the CH224K falls back to the highest available (or 5 V). The firmware has
no way to detect this — it sets CFG for 15 V but might get 12 V. This is
acceptable: the fan just runs slightly slower than "maximum."

## Build

```sh
pio run -e esp32c3-oled-pd          # build
pio run -e esp32c3-oled-pd -t upload  # flash
```

The PD environment extends `esp32c3-oled-ds18b20` (DS18B20 temp sensor)
and adds `-DFAN_CONTROL_PD`.
