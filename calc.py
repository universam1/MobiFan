V_fb = 0.6                                # MT3608 internal FB reference
V_cal = 12.0                              # pot-set anchor (zero injection)
R_bottom = 2200.0                         # onboard on the MT3608 module
R_top = (V_cal - V_fb) * R_bottom / V_fb  # effective pot position, ~41.8k (of 100k)
R_pwm = 8200.0                            # external summing resistor
R_filt = 1000.0                           # external RC series resistor (with 2.2uF)
V_logic = 3.3

def calc_vout(v_gpio):
    """Vout for a given GPIO average voltage (R_filt/R_pwm divider included)."""
    v_node = (v_gpio / R_filt + V_fb / R_pwm) / (1.0 / R_filt + 1.0 / R_pwm)
    current_bottom = V_fb / R_bottom
    current_pwm = (v_node - V_fb) / R_pwm
    return V_fb + (current_bottom - current_pwm) * R_top

def calc_duty(v_out):
    """Inverse: duty needed for a target Vout -- what FanControl::applyVolts does."""
    v_node = V_fb + R_pwm * (V_fb / R_bottom - (v_out - V_fb) / R_top)
    v_gpio = v_node + R_filt * (v_node - V_fb) / R_pwm
    return v_gpio / V_logic

for pwm_percent in range(0, 101, 5):
    print(f"{pwm_percent}% PWM: {calc_vout(pwm_percent / 100.0 * V_logic):.2f}V")

print()
for v in (14.0, 12.0, 5.5):
    print(f"{v:.1f}V target -> duty {calc_duty(v) * 100:.2f}%")
