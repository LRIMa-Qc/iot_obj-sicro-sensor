#!/usr/bin/env bash

set -euo pipefail

WORKSPACE_PATH=/opt/LRIMa
FILENAME=iot_obj-sicro-sensor
STANDARD_LOGS=/var/log/LRIMa/standard.log
ERROR_LOGS=/var/log/LRIMa/error.log
CONTROLLER_NAME="centrale"

sudo useradd --system --no-create-home lrima 2>/dev/null || true
sudo mkdir -p /var/log/LRIMa

sudo apt-get install bluez bluetooth python3 bluez-tools python3-pip python3-venv git -q -y
sudo systemctl daemon-reexec
sudo systemctl daemon-reload
sudo systemctl restart bluetooth
sudo systemctl enable bluetooth



RASP_VERSION="$(cat /sys/firmware/devicetree/base/model)"
IS_RASP_VERSION_5=false
if [[ $RASP_VERSION == *"Raspberry Pi 5"* ]]; then
	IS_RASP_VERSION_5=true
fi

mkdir -p "$WORKSPACE_PATH"
if [[ ! -d "$WORKSPACE_PATH/$FILENAME/.git" ]]; then
	git clone https://github.com/LRIMa-Qc/iot_obj-sicro-sensor.git "$WORKSPACE_PATH/$FILENAME"
fi
cd "$WORKSPACE_PATH/$FILENAME/code/central"

python3 -m venv venv
if [[ $IS_RASP_VERSION_5 == true ]]; then
	venv/bin/pip install -r requirements_pi5.txt
else
	venv/bin/pip install -r requirements_pi4.txt
fi


sudo tee /etc/systemd/system/LRIMa-central.service >/dev/null <<EOF
[Unit]
Description=Serre aliot-py
Wants=network-online.target bluetooth.target
After=network-online.target bluetooth.target

[Service]
Type=simple
User=lrima
Group=lrima
WorkingDirectory=$WORKSPACE_PATH/$FILENAME/code/central
ExecStart=$WORKSPACE_PATH/$FILENAME/code/central/venv/bin/python -u main.py
Restart=on-failure
RestartSec=60
StandardOutput=append:$STANDARD_LOGS
StandardError=append:$ERROR_LOGS

[Install]
WantedBy=multi-user.target
EOF

sudo tee /etc/bluetooth/main.conf >/dev/null <<EOF
# /etc/bluetooth/main.conf

[Policy]
# Automatically power on adapters when BlueZ starts
AutoEnable=true

[General]
# Name announced to other devices
Name = $CONTROLLER_NAME

# Default adapter class (0x000000 = uncategorized)
Class = 0x000000

# How long to remain discoverable (0 = always, value in seconds)
DiscoverableTimeout = 0

# How long to remain pairable (0 = always)
PairableTimeout = 0

# Enable LE (Bluetooth Low Energy) support
ControllerMode = dual

# Disable BNEP (Network) profile if not needed
#DisabledPlugins = network
EOF

chmod +x pi_setup.sh
./pi_setup.sh

sudo systemctl daemon-reexec
sudo systemctl daemon-reload
sudo systemctl enable --now LRIMa-central
