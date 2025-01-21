from gpiozero import Servo
import threading
from picamera import PiCamera
from picamera.array import PiRGBArray

try:
    camera.close() 
    del camera
except:
    print('ok')

# Up and down
servo1 = Servo(17)
# Left and right
servo2 = Servo(27)

camera = PiCamera()
camera.resolution = (640, 640) 
camera.framerate = 1
rawCapture = PiRGBArray(camera)
time.sleep(0.1)

MOVE_VALUE = 0.15

def up():
    servo1.value = max(min(1, servo1.value + MOVE_VALUE), -1)
    #servo1.max()
    print(servo1.value)
    
def down():
    servo1.value = max(min(1, servo1.value - MOVE_VALUE), -1)
    #servo1.min()
    print(servo1.value)
    
def left():
    servo2.value = max(min(1, servo2.value + MOVE_VALUE), -1)
    #servo2.max()
    print(servo2.value)
    
def right():
    servo2.value = max(min(1, servo2.value - MOVE_VALUE), -1)
    #servo2.min()
    print(servo2.value)

def pic():
    try:
        camera.capture(rawCapture, format="rgb")
        image = rawCapture.array

        camera.close() 
    except:
        print('Something went wrong')

try:
    t = threading.Thread()
    t.start()

except KeyboardInterrupt:
    print("Exiting...")
