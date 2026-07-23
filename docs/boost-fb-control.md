# MCU-Controlled XL6009E1 Boost Converter (Fan DC Speed Control)

- **Goal**: Digitally control an XL6009E1 boost module's output voltage
  (~5.5 V to 14.0 V) using the ESP32-C3, to speed-control a 3-pin DC fan
  (Thermaltake Pure 20) by varying its supply voltage.
- **Input voltage (Vin)**: 5 V DC (USB)
- **Method**: PWM current injection into the regulator's feedback (FB) node,
  **keeping the module's onboard trim pot and pull-down resistor**.

## 1. Core concept

The XL6009E1 uses an internal reference voltage of **1.25 V** on its feedback
pin. It constantly adjusts its switching duty cycle to force the FB node to
exactly 1.25 V. This is the same core mechanism as a buck converter's FB
loop — the KCL at the node is topology-agnostic, only the reference voltage
and divider resistors differ.

By using an ESP32-C3 GPIO to inject or sink current through a summing resistor
connected to this node, the voltage divider dynamics change. The control loop
is **inverted**:

- **High MCU voltage (injected current)**: tricks the chip into thinking Vout
  is too high → **Vout decreases**.
- **Low MCU voltage (sunk current)**: tricks the chip into thinking Vout is
  too low → **Vout increases**.

### The anchor trick — bidirectional injection

Whenever the filtered PWM voltage equals the 1.25 V reference — GPIO
high-impedance (boot!) or duty ≈ 38% — R_PWM carries **zero current** and
Vout is purely what the onboard pot is set to. That pot setting is the
*anchor*, and injection works in **both directions** around it:

- Duty **above** ~38% (V_PWM > 1.25 V) *sources* current into FB → Vout
  drops **below** the anchor.
- Duty **below** ~38% (V_PWM < 1.25 V) *sinks* current out of FB → Vout
  rises **above** the anchor.

This is why the anchor is deliberately set to the fan's rated **12.0 V**
rather than the 14 V maximum: the anchor is what the fan sees at boot and in
any firmware-dead/GPIO-floating state, so the fail-safe voltage is benign,
while the 14 V maximum is still available on command (~30% duty, sinking).

## 2. Circuit schematic

Onboard on the XL6009E1 module: 10 kΩ trim pot (Vout → FB) and R_bottom
(330 Ω). External: only the RC filter and R_PWM.

```
                       [ Vout (5.5V to 14V) ]
                                 |
                        [onboard 10k trim pot]   <- calibrated to 12.0 V
                                 |
 ESP32-C3 GPIO ---[330R]---+---[R_PWM: 390R]---+-------> [ XL6009E1 FB Pin ]
                           |                   |
                        [4.7uF]        [R_bottom: 330R onboard]
                           |                   |
                          GND                 GND
```

## 3. Component values

| Part | Value | Role |
|---|---|---|
| Trim pot (onboard) | 10 kΩ, set to 12.0 V | Upper divider Vout → FB; effective ≈ 2.84 kΩ |
| R_bottom (onboard) | 330 Ω | Lower divider FB → GND (XL6009E1 module stock) |
| R_PWM (external) | **390 Ω, 1%** | Summing resistor, filter output → FB |
| Low-pass filter (external) | 330 Ω + 4.7 μF ceramic to GND | Converts the PWM into smooth DC |

## 4. Calibration procedure

1. With the GPIO floating (ESP32 unpowered, or before wiring R_PWM), adjust
   the onboard pot until **Vout = 12.0 V**.
2. If your measured anchor differs, enter it as `BOOST_VOUT_CAL` in
   [src/config.h](../src/config.h) — the firmware derives the effective
   R_top from it.
3. Fix the pot with a dab of lacquer (van vibration).

## 5. Transfer function

KCL at the FB node (held at 1.25 V by the regulator), with V_PWM being the
filtered GPIO voltage (0–3.3 V) and R_top the effective pot resistance:

```
Vout  = 1.25 + R_top * ( 1.25 / R_bottom  -  (V_PWM - 1.25) / R_PWM )
R_top = (Vout_cal - 1.25) * R_bottom / 1.25      ~= 2.84k for 12.0 V anchor (of the 10k pot)
```

Solved for the PWM voltage needed for a target Vout (this is what
`FanControl::applyVolts()` implements):

```
V_PWM = 1.25 + R_PWM * ( 1.25 / R_bottom  -  (Vout - 1.25) / R_top )
      = 1.25 + (Vout_cal - Vout) * R_PWM / R_top
duty  = V_PWM / 3.3
```

### Operating truth table (12 V anchor, 390Ω R_PWM)

| PWM duty | Filtered V_PWM | Vout | Injection |
|---|---|---|---|
| 0% | 0.00 V | ≈ 21.1 V | sinking (headroom, unused) |
| 20% | 0.66 V | ≈ 16.3 V | sinking (headroom, unused) |
| **~29.6%** | 0.98 V | **14.0 V (commandable max)** | sinking |
| ~37.9% | 1.25 V | 12.0 V (anchor, zero injection) | none |
| high-Z (boot) | — | 12.0 V (anchor) | none |
| 50% | 1.65 V | ≈ 9.1 V | sourcing |
| **~65.0%** | 2.14 V | **5.5 V (commandable min)** | sourcing |
| 80% | 2.64 V | ≈ 1.9 V (unreachable — floor is ~Vin) | sourcing |
| 100% | 3.30 V | < 0 V (unreachable) | sourcing |

The firmware clamps commands to `BOOST_VOUT_MIN`/`BOOST_VOUT_MAX` (5.5–14 V),
so only the ~29.6–65.0% duty band is ever actually commanded — the rows
outside that range describe what the *unclamped* formula would ask for, not
anything the module can physically do (it can't boost below its own input,
nor sensibly above the divider's linear range).

**Note on sensitivity**: R_PWM (390 Ω) is now close in magnitude to
R_bottom (330 Ω), unlike the old buck design where R_PWM (30 kΩ) was much
larger than R_bottom (8.2 kΩ). This makes the duty→Vout slope roughly 3x
steeper than before (~0.36 V per % duty vs ~0.13 V/%). The 10-bit PWM still
gives ample resolution (~0.02 V/step across the 5.5–14 V span), but it's
worth bench-verifying actual voltages against this table since resistor
tolerance and GPIO logic-level drift have a larger effect on absolute
accuracy than in the old design.

## 6. Firmware rules

- **PWM frequency**: 20–50 kHz via the LEDC peripheral to minimize analog
  ripple after the 330 Ω/4.7 μF filter (we use 25 kHz, `BOOST_PWM_FREQ_HZ`).
  The new filter's cutoff (~103 Hz) is still >200x below the PWM frequency.
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
- **No true off**: a boost converter can't output less than roughly its own
  input voltage (~5 V here), so fan power 0% commands `BOOST_VOUT_MIN`
  (5.5 V) — the same voltage as the low end of the power>0 range
  (`FAN_V_MIN`). The fan effectively never stops spinning; this matches the
  project's existing "no true off" design (a true off would require wiring
  the XL6009E1's EN pin to a GPIO — not currently done).
- **Calibration coupling**: `BOOST_VOUT_CAL` must match the actual pot
  setting, or every commanded voltage is offset accordingly.

All values live in [src/config.h](../src/config.h) (`BOOST_*` and `FAN_V_*`);
the mapping code is [src/FanControl.cpp](../src/FanControl.cpp). A quick
truth-table check lives in [calc.py](../calc.py).
