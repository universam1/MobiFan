V_fb = 0.8
V_cal = 12.0                              # pot-set anchor (zero injection)
R_bottom = 8200.0                         # onboard on D-SUN module
R_top = (V_cal - V_fb) * R_bottom / V_fb  # effective pot position, ~135.3k
R_pwm = 30000.0                           # external summing resistor

def calc_vout(v_pwm):
    current_bottom = V_fb / R_bottom
    current_pwm = (v_pwm - V_fb) / R_pwm
    current_top = current_bottom - current_pwm
    v_out = V_fb + (current_top * R_top)
    return v_out

for pwm_percent in range(0, 101, 5):
    v_pwm = (pwm_percent / 100.0) * 3.3
    print(f"{pwm_percent}% PWM: {calc_vout(v_pwm):.2f}V")
