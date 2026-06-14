import subprocess


class LedControl:
    LED_PATH = "/sys/devices/platform/leds/leds/PWR"

    def _set_manual_led_mode(self):
        try:
            subprocess.run(
                f"echo none | sudo tee {LedControl.LED_PATH}/trigger > /dev/null",
                shell=True,
                check=True,
            )
        except subprocess.CalledProcessError as _:
            try:
                subprocess.run(
                    f"sudo chmod 606 -R {LedControl.LED_PATH}", shell=True, check=True
                )
            except Exception as e:
                print(f"Failed to set LED mode: {e}")

    def _led_on(self):
        try:
            subprocess.run(
                f"echo 255 | sudo tee {LedControl.LED_PATH}/brightness > /dev/null",
                shell=True,
                check=True,
            )
        except Exception as e:
            print(f"LED ON failed: {e}")

    def _led_off(self):
        try:
            subprocess.run(
                f"echo 0 | sudo tee {LedControl.LED_PATH}/brightness > /dev/null",
                shell=True,
                check=True,
            )
        except Exception as e:
            print(f"LED OFF failed: {e}")
