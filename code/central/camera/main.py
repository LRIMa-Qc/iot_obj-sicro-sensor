import picamera2
import time
import datetime
import os
import argparse
import cv2, time

parser = argparse.ArgumentParser(description="The cli program to take pictures with the pi cam")
parser.add_argument("--destination", "-d")
parser.add_argument("--config", "-c", help="configures the camera")

args = parser.parse_args()

dest = args.destination

resolution = (2592, 1944)
#resolution = (9248, 6944)

config = args.config

# if more are added in the future, add before *others
# *others is there to prevent probleme reading from an older version of the script
exposure, analog_gain, colour_gains, autofocus, *others = config.split("-")

exposure = int(exposure.split("@")[1]) if exposure != "auto" else auto_exposure()
analog_gain = int(analog_gain.split("@")[1]) if analog_gain != "auto" else auto_analog_gain()
colour_gains = tuple(map(float, colour_gains.split("@")[1].split("_"))) if colour_gains != "auto" else auto_colour_gains()
autofocus = bool(autofocus.split("@")[1])

def auto_exposure():
    pass

def auto_analog_gain():
    pass

def auto_colour_gains():
    pass

def apply_text(request):
    # Text options
    colour = (255, 255, 255)
    origin = (0, 60)
    font = cv2.FONT_HERSHEY_SIMPLEX
    scale = 2
    thickness = 2
    text = time.strftime("%Y-%m-%d %X")
    with picamera2.MappedArray(request, "main") as m:
        cv2.putText(m.array, text, origin, font, scale, colour, thickness)


with picamera2.Picamera2() as camera:
    camera.pre_callback = apply_text

    #config = camera.create_still_configuration(main={"size": resolution}, raw={"format": "SRGGB10"})
    config = camera.create_still_configuration(main={"size": resolution})
    camera.configure(config)
    # TODO: Figure this out D:
    #camera.set_controls({"ExposureTime": exposure, "AnalogueGain": analog_gain, "ColourGains": colour_gains})
    camera.start()

    if autofocus:
        camera.autofocus_cycle()

    time.sleep(2)

    camera.capture_file(f"{dest}.jpg")
    #r = camera.capture_request(config)
    #r.save("main", "2.png")
    #r.save_dng("2.dng")
    #print(r.get_metadata())
    #r.release()

# Alter the image levels
# import time
# from picamera2 import Picamera2, Preview

#picam2 = Picamera2()
# picam2.start_preview(Preview.QTGL)

# preview_config = picam2.create_preview_configuration()
#capture_config = picam2.create_still_configuration(raw={}, display=None)
# picam2.configure(preview_config)

#picam2.start()
#time.sleep(2)

#r = picam2.switch_mode_capture_request_and_stop(capture_config)
#r.save("main", "full.jpg")
#r.save_dng("full.dng")
