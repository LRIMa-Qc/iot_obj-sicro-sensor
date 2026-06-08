# Documentation: https://alivecode.ca/docs/aliot
from aliot.aliot_obj import AliotObj
from test import up, down, left, right, pic
from io import BytesIO
# Création de l'objet à partir du fichier de configuration
camera_2 = AliotObj("camera-2")


def start():
    # Écrivez le code que vous voulez exécuter une fois que l'objet
    # est connecté au serveur
    pass
    camera_2.on_action_recv("down", down) 
    camera_2.on_action_recv("right", right) 
    camera_2.on_action_recv("left", left) 
    camera_2.on_action_recv("up", up) 
    camera_2.on_action_recv("pic", upload_image)

def potatio(data):
    print("aaaaa")
    print(data)
def upload_image(_):
    image = pic(2)
    camera_2.upload_image(image)

# Appel de la fonction start une fois que l'objet se connecte au serveur
camera_2.on_start(callback=start)

# Connection de l'objet au serveur ALIVEcode
camera_2.run()
