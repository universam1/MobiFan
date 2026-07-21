# MCU-Controlled MP1584EN Buck Converter (Fan DC Speed Control)

- **Goal**: Digitally control an MP1584EN buck converter's output voltage from
  3.0 V to 14.0 V using the ESP32-C3, to speed-control a 3-pin DC fan
  (Thermaltake Pure 20) by varying its supply voltage.
- **Input voltage (Vin)**: 20 V DC
- **Method**: PWM current injection into the regulator's feedback (FB) node.

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

## 2. Circuit schematic

```
                        [ Vout (3V to 14V) ]
                                 |
                            [R_top: 100k]
                                 |
 ESP32-C3 GPIO ---[1k]---+---[R_PWM: 30k]---+-------> [ MP1584EN FB Pin ]
                         |                  |
                       [1uF]          [R_bottom: 7.5k]
                         |                  |
                        GND                GND
```

## 3. Component values

| Part | Value | Role |
|---|---|---|
| R_top | 100 kΩ | Upper divider Vout → FB (replaces the stock trim pot) |
| R_bottom | 7.5 kΩ | Lower divider FB → GND |
| R_PWM | 30 kΩ | Summing resistor, filter output → FB |
| Low-pass filter | 1 kΩ + 1 μF ceramic to GND | Converts the PWM into smooth DC |

## 4. Transfer function

KCL at the FB node (held at 0.8 V by the regulator), with V_PWM being the
filtered GPIO voltage (0–3.3 V):

```
Vout = 0.8 + R_top * ( 0.8 / R_bottom  -  (V_PWM - 0.8) / R_PWM )
```

Solved for the PWM voltage needed for a target Vout (this is what
`FanControl::applyVolts()` implements):

```
V_PWM = 0.8 + R_PWM * ( 0.8 / R_bottom  -  (Vout - 0.8) / R_top )
duty  = V_PWM / 3.3
```

### Operating truth table

| PWM duty | Filtered V_PWM | Vout |
|---|---|---|
| 0% | 0.0 V | ≈ 14.1 V |
| 50% | 1.65 V | ≈ 8.6 V |
| 100% | 3.3 V | ≈ 3.1 V |

## 5. Firmware rules

- **PWM frequency**: 20–50 kHz via the LEDC peripheral to minimize analog
  ripple after the 1k/1μF filter (we use 25 kHz, `BUCK_PWM_FREQ_HZ`).
- **Push-pull output**: the GPIO must source *and* sink current into the
  filter — do not configure the pin open-drain.
- **Boot safety**: at boot the GPIO is high-impedance/0 V, so the converter
  outputs its **maximum voltage (~14 V)** until the PWM starts. `fan.begin()`
  is therefore the first call in `setup()`, and connected loads must tolerate
  the brief transient.
- **Inverted logic**: output voltage is inversely proportional to duty cycle.
  All inversion is contained in `FanControl` — the rest of the firmware deals
  only in fan power % (0–100).
- **No true off**: 100% duty still leaves ~3.1 V at the output. Fan power 0%
  commands `BUCK_VOUT_MIN`, which is below the fan's start/run voltage, so the
  fan stops. (A true off would require wiring the MP1584EN EN pin to a GPIO —
  not currently done.)

All values live in [src/config.h](../src/config.h) (`BUCK_*` and `FAN_V_*`);
the mapping code is [src/FanControl.cpp](../src/FanControl.cpp).
