# MCU-Controlled MT3608 Boost Converter (Fan DC Speed Control)

- **Goal**: Digitally control an MT3608 boost module's output voltage
  (~5.5 V to 14.0 V) using the ESP32-C3, to speed-control a 3-pin DC fan
  (Thermaltake Pure 20) by varying its supply voltage.
- **Input voltage (Vin)**: 5 V DC (USB)
- **Method**: PWM current injection into the regulator's feedback (FB) node,
  **keeping the module's onboard trim pot and pull-down resistor**.

## 1. Core concept

The MT3608 uses an internal reference voltage of **0.6 V** on its feedback
pin. It constantly adjusts its switching duty cycle to force the FB node to
exactly 0.6 V.

By using an ESP32-C3 GPIO to inject or sink current through a summing resistor
connected to this node, the voltage divider dynamics change. The control loop
is **inverted**:

- **High MCU voltage (injected current)**: tricks the chip into thinking Vout
  is too high → **Vout decreases**.
- **Low MCU voltage (sunk current)**: tricks the chip into thinking Vout is
  too low → **Vout increases**.

### The anchor trick — bidirectional injection

Whenever the filter node voltage equals the 0.6 V reference — GPIO
high-impedance (boot!) or duty ≈ 18.2% — R_PWM carries **zero current** and
Vout is purely what the onboard pot is set to. That pot setting is the
*anchor*, and injection works in **both directions** around it:

- Duty **above** ~18.2% (V_node > 0.6 V) *sources* current into FB → Vout
  drops **below** the anchor.
- Duty **below** ~18.2% (V_node < 0.6 V) *sinks* current out of FB → Vout
  rises **above** the anchor.

This is why the anchor is deliberately set to the fan's rated **12.0 V**
rather than the 14 V maximum: the anchor is what the fan sees at boot and in
any firmware-dead/GPIO-floating state, so the fail-safe voltage is benign,
while the 14 V maximum is still available on command (~4.8% duty, sinking).

## 2. Circuit schematic

Onboard on the MT3608 module: 100 kΩ trim pot (Vout → FB) and R_bottom
(2.2 kΩ). External: only the RC filter and R_PWM.

```
                       [ Vout (5.5V to 14V) ]
                                 |
                       [onboard 100k trim pot]   <- calibrated to 12.0 V
                                 |
 ESP32-C3 GPIO ---[1k]-----+---[R_PWM: 8.2k]---+-------> [ MT3608 FB Pin ]
                           |                   |
                        [2.2uF]        [R_bottom: 2.2k onboard]
                           |                   |
                          GND                 GND
```

## 3. Component values

| Part | Value | Role |
|---|---|---|
| Trim pot (onboard) | 100 kΩ, set to 12.0 V | Upper divider Vout → FB; effective ≈ 41.8 kΩ |
| R_bottom (onboard) | 2.2 kΩ | Lower divider FB → GND (MT3608 module stock) |
| R_PWM (external) | **8.2 kΩ, 1%** | Summing resistor, filter output → FB |
| Low-pass filter (external) | **1 kΩ + 2.2 μF** ceramic to GND | Converts the PWM into smooth DC (fc ≈ 72 Hz) |

## 4. Calibration procedure

1. With the GPIO floating (ESP32 unpowered, or before wiring R_PWM), adjust
   the onboard pot until **Vout = 12.0 V**.
2. If your measured anchor differs, enter it as `BOOST_VOUT_CAL` in
   [src/config.h](../src/config.h) — the firmware derives the effective
   R_top from it.
3. Fix the pot with a dab of lacquer (van vibration).

## 5. Transfer function

KCL at the FB node (held at 0.6 V by the regulator), with V_node being the
filter output voltage and R_top the effective pot resistance:

```
Vout  = 0.6 + R_top * ( 0.6 / R_bottom  -  (V_node - 0.6) / R_PWM )
R_top = (Vout_cal - 0.6) * R_bottom / 0.6      ~= 41.8k for 12.0 V anchor (of the 100k pot)
```

Solved for the filter node voltage needed for a target Vout:

```
V_node = 0.6 + R_PWM * ( 0.6 / R_bottom  -  (Vout - 0.6) / R_top )
       = 0.6 + (Vout_cal - Vout) * R_PWM / R_top
```

**V_node is not the GPIO's average voltage.** R_FILT and R_PWM form a divider
between the pin and the FB node, which the regulator holds at 0.6 V, so the
pin must overdrive by the drop across R_FILT. From
`(V_gpio - V_node)/R_FILT = (V_node - 0.6)/R_PWM`:

```
V_gpio = V_node + R_FILT * (V_node - 0.6) / R_PWM
duty   = V_gpio / 3.3
```

Both steps are what `FanControl::applyVolts()` implements. Skipping the
second one would offset every commanded voltage by up to ~11% here (and
scales with R_FILT/R_PWM, so it is not optional).

### Operating truth table (12 V anchor, 8.2 kΩ R_PWM, 1 kΩ R_FILT)

| PWM duty | Vout | Injection |
|---|---|---|
| 0% | ≈ 14.7 V | sinking (headroom, unused) |
| **~4.8%** | **14.0 V (commandable max)** | sinking |
| ~18.2% | 12.0 V (anchor, zero injection) | none |
| high-Z (boot) | 12.0 V (anchor) | none |
| 40% | ≈ 8.7 V | sourcing |
| **~61.5%** | **5.5 V (commandable min)** | sourcing |
| 80% | ≈ 2.7 V (unreachable — floor is ~Vin) | sourcing |
| 100% | < 0 V (unreachable) | sourcing |

The firmware clamps commands to `BOOST_VOUT_MIN`/`BOOST_VOUT_MAX` (5.5–14 V),
so only the ~4.8–61.5% duty band is ever actually commanded — the rows
outside that range describe what the *unclamped* formula would ask for, not
anything the module can physically do (it can't boost below its own input,
nor sensibly above the divider's linear range).

### Note on the RC filter sizing

The filter's `R_FILT · C` product is set by the **Vout ripple budget**, and
it does *not* simply track R_PWM. Ripple at the output scales with
`R_top × i_FB` (the injected ripple current times the upper divider
resistance), not with `i_FB` alone — so sizing the filter by "keep the FB
ripple current constant" alone would badly underestimate the cap needed,
since R_top here is ~41.8 kΩ. With 1 kΩ + 2.2 μF (fc ≈ 72 Hz, 346x below the
25 kHz injection PWM) the residual Vout ripple is ≈ 31 mV pk.

R_FILT's value trades off two things: lowering it stiffens the GPIO drive
into the filter (and raises peak pin current — 3.3 mA at 1 kΩ) but demands a
proportionally larger cap for the same ripple. It does **not** by itself
change the ripple; only the RC product does. Its effect on the DC transfer
is exactly compensated in the transfer function above, so it can be chosen
freely on ripple/drive grounds.

## 6. Firmware rules

- **PWM frequency**: 20–50 kHz via the LEDC peripheral to minimize analog
  ripple after the 1 kΩ/2.2 μF filter (we use 25 kHz, `BOOST_PWM_FREQ_HZ`).
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
  the MT3608's EN pin to a GPIO — not currently done).
- **Calibration coupling**: `BOOST_VOUT_CAL` must match the actual pot
  setting, or every commanded voltage is offset accordingly.

All values live in [src/config.h](../src/config.h) (`BOOST_*` and `FAN_V_*`);
the mapping code is [src/FanControl.cpp](../src/FanControl.cpp). A quick
truth-table check lives in [calc.py](../calc.py).
