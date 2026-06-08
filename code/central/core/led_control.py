import subprocess

class LedControl:
  LED_PATH = "/sys/class/leds/PWR"

  def _set_manual_led_mode(self):
    try:
      subprocess.run(
        f"echo none > {self.LED_PATH}/trigger",
        shell=True,
        check=True
      )
    except Exception as e:
      print(f"Failed to set LED mode: {e}")

  def _led_on(self):
    try:
      with open(f"{self.LED_PATH}/brightness", "w") as f:
        f.write("1")
    except Exception as e:
      print(f"LED ON failed: {e}")


  def _led_off(self):
    try:
      with open(f"{self.LED_PATH}/brightness", "w") as f:
        f.write("0")
    except Exception as e:
      print(f"LED OFF failed: {e}")
