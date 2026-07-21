R_top = 100000.0   # 100k
R_bottom = 7500.0  # 7.5k
R_pwm = 30000.0    # 30k
V_fb = 0.8

def calc_vout(v_pwm):
    current_bottom = V_fb / R_bottom
    current_pwm = (v_pwm - V_fb) / R_pwm
    current_top = current_bottom - current_pwm
    v_out = V_fb + (current_top * R_top)
    return v_out

for pwm_percent in range(0, 101, 5):
    v_pwm = (pwm_percent / 100.0) * 3.3
    print(f"{pwm_percent}% PWM: {calc_vout(v_pwm):.2f}V")
