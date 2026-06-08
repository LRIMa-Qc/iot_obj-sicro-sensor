#!/bin/bash

set -e
echo 'started script'
.  /home/lrima/iot_obj-sicro-sensor/code/central/venv/bin/activate
echo 'activated venv'
cd /home/lrima/iot_obj-sicro-sensor/code/central || exit 1
echo 'changed directory'

/home/lrima/iot_obj-sicro-sensor/code/central/venv/bin/aliot run camera-2
