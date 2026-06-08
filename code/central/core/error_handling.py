import sys
import subprocess
import time
from led_control import LedControl

class ErrorHandler:
  """Base class for all exceptions raised by the central core."""
  def __init__(self):
    self.led_control = LedControl()

  def log_and_restart_service(self, msg: str, led_sequence: list[int]):
    print(msg)
    
      # Make sure LED is under manual control once at start
    self.led_control._set_manual_led_mode()

    for delay in led_sequence:
      self.led_control._led_off()
      time.sleep(delay)
      self.led_control._led_on()
      time.sleep(delay)
    
    self.led_control._led_off()
    time.sleep(1)
    self.led_control._led_on()

    self._run_restart_command(["sudo", "systemctl", "restart", "bluetooth"])
    self._run_restart_command(["rfkill", "unblock", "all"])
    self._run_restart_command(["pm2", "restart", "all"])
    sys.exit(1)
  
  def log_no_restart(self, msg: str, led_sequence: list[int]):
    print(msg)
      
    # Make sure LED is under manual control once at start
    self.led_control._set_manual_led_mode()

    for delay in led_sequence:
      self.led_control._led_off()
      time.sleep(delay)
      self.led_control._led_on()
      time.sleep(delay)
    
    self.led_control._led_off()
    time.sleep(1)
    self.led_control._led_on()
  
  
  def _run_restart_command(self, command: list[str]) -> None:
    try:
      subprocess.run(command, check=True, timeout=10)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
      print(f"Restart command failed {command}: {exc}")

