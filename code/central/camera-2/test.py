from gpiozero import Device, Servo
from gpiozero.pins.pigpio import PiGPIOFactory 
from picamera2 import Picamera2
from time import sleep
import logging
import sys
from io import BytesIO 
from libcamera import controls
Device.pin_factory = PiGPIOFactory()
camera = Picamera2()
camera.resolution = (1920, 1080) 
camera.framerate = 1
camera.set_controls({"AfMode": controls.AfModeEnum.Continuous})
#camera.set_controls({"AfTrigger": 0})
camera.format = "RGB888"
#format ne sert a rien
camera.start()

sleep(0.1)


logging.basicConfig(
    filename="cam.log",
    level=logging.DEBUG,
    format="%(asctime)s - %(levelname)s - %(message)s",
    filemode="a")

# Up and down
servo1 = Servo(17)
# Left and right
servo2 = Servo(27)

MOVE_VALUE = 0.15

def up(_):
    servo1.value = max(min(1, servo1.value + MOVE_VALUE), -1)
    print(servo1.value)
    
def down(_):
    servo1.value = max(min(1, servo1.value - MOVE_VALUE), -1)
    print(servo1.value)
    
def left(_):
    servo2.value = max(min(1, servo2.value + MOVE_VALUE), -1)
    print(servo2.value)
    
def right(_):
    servo2.value = max(min(1, servo2.value - MOVE_VALUE), -1)
    
    print(servo2.value)

def premove(args = sys.argv):
    sleep(1)
    for arg in args:
        sleep(2)
        match(arg):
            case "u":
                up()
                logging.info("Moving camera up")
            case "d":
                down()
                logging.info("Moving camera down")
            case "l":
                left()
                logging.info("Moving camera left")
            case "r":
                right()
                logging.info("Moving camera right")
            case _:
                logging.info("invalid")
def pic(_):
    try:
        buffer = BytesIO()
        #image = camera.capture_file(buffer, format="jpeg")
        image = camera.capture_file(buffer, format="jpeg")
        buffer.seek(0)
        return buffer
        #print(len(image))
        #image = bytes(image)
        #print(len(image))
    except Exception as e:
        print(f'Something went wrong {e=}')
    finally:
        #return image
        pass
logging.info("Started")
