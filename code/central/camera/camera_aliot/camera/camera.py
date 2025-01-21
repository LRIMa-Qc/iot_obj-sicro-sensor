from aliot.aliot_obj import AliotObj
from time import sleep
from camera_utils import up, down, left, right, pic

camera = AliotObj("camera")
sleep(5)

def start():
    while True:
        sleep(1)
     
def go_up(data):
    up()
    
def go_down(data):
    down()
    
def go_left(data):
    left()
    
def go_right(data):
    right()
    
def take_pic(data):
    pic()

camera.on_action_recv(action_id="up", callback=go_up)
camera.on_action_recv(action_id="down", callback=go_down)
camera.on_action_recv(action_id="left", callback=go_left)
camera.on_action_recv(action_id="right", callback=go_right)
camera.on_action_recv(action_id="pic", callback=take_pic)
camera.on_start(callback=start)
camera.run()