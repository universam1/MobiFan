# MCU-Controlled MP1584EN Buck Converter (Fan DC Speed Control)

- **Goal**: Digitally control a D-SUN MP1584EN buck module's output voltage
  (~2.4 V to 14.0 V) using the ESP32-C3, to speed-control a 3-pin DC fan
  (Thermaltake Pure 20) by varying its supply voltage.
- **Input voltage (Vin)**: 20 V DC
- **Method**: PWM current injection into the regulator's feedback (FB) node,
  **keeping the module's onboard trim pot and pull-down resistor**.

## 1. Core concept

The MP1584EN uses an internal reference voltage of **0.8 V** on its feedback
pin (pin 5). It constantly adjusts its switching duty cycle to force the FB
node to exactly 0.8 V.

By using an ESP32-C3 GPIO to inject or sink current through a summing resistor
connected to this node, the voltage divider dynamics change. The control loop
is **inverted**:

- **High MCU voltage (injected current)**: tricks the chip into thinking Vout
  is too high → **Vout decreases**.
- **Low MCU voltage (sunk current)**: tricks the chip into thinking Vout is
  too low → **Vout increases**.

### The anchor trick — bidirectional injection

Whenever the filtered PWM voltage equals the 0.8 V reference — GPIO
high-impedance (boot!) or duty ≈ 24% — R_PWM carries **zero current** and
Vout is purely what the onboard pot is set to. That pot setting is the
*anchor*, and injection works in **both directions** around it:

- Duty **above** ~24% (V_PWM > 0.8 V) *sources* current into FB → Vout
  drops **below** the anchor.
- Duty **below** ~24% (V_PWM < 0.8 V) *sinks* current out of FB → Vout
  rises **above** the anchor.

This is why the anchor is deliberately set to the fan's rated **12.0 V**
rather than the 14 V maximum: the anchor is what the fan sees at boot and in
any firmware-dead/GPIO-floating state, so the fail-safe voltage is benign,
while the 14 V maximum is still available on command (~8% duty, sinking).

## 2. Circuit schematic

Onboard on the D-SUN module: trim pot (Vout → FB) and R_bottom (8.2 kΩ).
External: only the RC filter and R_PWM.

```
                       [ Vout (2.4V to 14V) ]
                                 |
                        [onboard trim pot]        <- calibrated to 12.0 V
                                 |
 ESP32-C3 GPIO ---[1k]---+---[R_PWM: 30k]---+-------> [ MP1584EN FB Pin ]
                         |                  |
                       [1uF]       [R_bottom: 8.2k onboard]
                         |                  |
                        GND                GND
```

## 3. Component values

| Part | Value | Role |
|---|---|---|
| Trim pot (onboard) | set to 12.0 V | Upper divider Vout → FB; effective ≈ 115 kΩ |
| R_bottom (onboard) | 8.2 kΩ | Lower divider FB → GND (D-SUN stock) |
| R_PWM (external) | **30 kΩ, 1%** | Summing resistor, filter output → FB |
| Low-pass filter (external) | 1 kΩ + 1 μF ceramic to GND | Converts the PWM into smooth DC |

## 4. Calibration procedure

1. With the GPIO floating (ESP32 unpowered, or before wiring R_PWM), adjust
   the onboard pot until **Vout = 12.0 V**.
2. If your measured anchor differs, enter it as `BUCK_VOUT_CAL` in
   [src/config.h](../src/config.h) — the firmware derives the effective
   R_top from it.
3. Fix the pot with a dab of lacquer (van vibration).

## 5. Transfer function

KCL at the FB node (held at 0.8 V by the regulator), with V_PWM being the
filtered GPIO voltage (0–3.3 V) and R_top the effective pot resistance:

```
Vout  = 0.8 + R_top * ( 0.8 / R_bottom  -  (V_PWM - 0.8) / R_PWM )
R_top = (Vout_cal - 0.8) * R_bottom / 0.8        ~= 114.8k for 12.0 V anchor
```

Solved for the PWM voltage needed for a target Vout (this is what
`FanControl::applyVolts()` implements):

```
V_PWM = 0.8 + R_PWM * ( 0.8 / R_bottom  -  (Vout - 0.8) / R_top )
      = 0.8 + (Vout_cal - Vout) * R_PWM / R_top
duty  = V_PWM / 3.3
```

### Operating truth table (12 V anchor, 30k R_PWM)

| PWM duty | Filtered V_PWM | Vout | Injection |
|---|---|---|---|
| 0% | 0.0 V | ≈ 15.1 V | sinking (headroom, unused) |
| **8%** | 0.28 V | **14.0 V (commandable max)** | sinking |
| 24% | 0.8 V | 12.0 V (anchor, zero injection) | none |
| high-Z (boot) | — | 12.0 V (anchor) | none |
| 72% | 2.37 V | 6.0 V | sourcing |
| 84% | 2.76 V | 4.5 V | sourcing |
| 94% | 3.10 V | 3.2 V (fan stop) | sourcing |
| 100% | 3.3 V | ≈ 2.4 V (floor) | sourcing |

The firmware clamps commands to `BUCK_VOUT_MAX` (14 V), leaving the
0–8% duty region (up to ~15.1 V) as unused headroom.

## 6. Firmware rules

- **PWM frequency**: 20–50 kHz via the LEDC peripheral to minimize analog
  ripple after the 1k/1μF filter (we use 25 kHz, `BUCK_PWM_FREQ_HZ`).
- **Push-pull output**: the GPIO must source *and* sink current into the
  filter — do not configure the pin open-drain.
- **Boot safety**: at boot the GPIO is high-impedance, so the converter sits
  at the pot anchor — deliberately the fan's rated **12 V** — until the PWM
  starts (`fan.begin()` is the first call in `setup()`). The same holds if
  the firmware ever dies with the pin floating: the fan runs at rated
  voltage, a benign fail state.
- **Inverted logic**: output voltage falls as duty rises. All inversion is
  contained in `FanControl` — the rest of the firmware deals only in fan
  power % (0–100).
- **No true off**: 100% duty still leaves ~2.4 V at the output. Fan power 0%
  commands `BUCK_VOUT_MIN` (3.2 V), below the fan's start/run voltage, so the
  fan stops. (A true off would require wiring the MP1584EN EN pin to a GPIO —
  not currently done.)
- **Calibration coupling**: `BUCK_VOUT_CAL` must match the actual pot
  setting, or every commanded voltage is offset accordingly.

All values live in [src/config.h](../src/config.h) (`BUCK_*` and `FAN_V_*`);
the mapping code is [src/FanControl.cpp](../src/FanControl.cpp). A quick
truth-table check lives in [calc.py](../calc.py).
