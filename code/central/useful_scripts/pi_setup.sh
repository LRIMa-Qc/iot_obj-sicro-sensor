#!/bin/bash
# Raspberry Pi 4 setup


# setup camera firmware
sudo tee /boot/firmware/config.txt > /dev/null <<'EOF'
# For more options and information see
# http://rptl.io/configtxt
# Some settings may impact device functionality. See link above for details

# Uncomment some or all of these to enable the optional hardware interfaces
#dtparam=i2c_arm=on
#dtparam=i2s=on
#dtparam=spi=on

# Enable audio (loads snd_bcm2835)
dtparam=audio=on

# Additional overlays and parameters are documented
# /boot/firmware/overlays/README

# Automatically load overlays for detected cameras
camera_auto_detect=0

# Automatically load overlays for detected DSI displays
display_auto_detect=1

# Automatically load initramfs files, if found
auto_initramfs=1

# Enable DRM VC4 V3D driver
dtoverlay=vc4-kms-v3d
max_framebuffers=2

# Don't have the firmware create an initial video= setting in cmdline.txt.
# Use the kernel's default instead.
disable_fw_kms_setup=1

# Run in 64-bit mode
arm_64bit=1

# Disable compensation for displays with overscan
disable_overscan=1

# Run as fast as firmware / board allows
arm_boost=1

[cm4]
# Enable host mode on the 2711 built-in XHCI USB controller.
# This line should be removed if the legacy DWC2 controller is required
# (e.g. for USB device mode) or if USB support is not required.
otg_mode=1

[cm5]
dtoverlay=dwc2,dr_mode=host

[all]
enable_uart=1
dtoverlay=imx708
EOF


#install camera firmware and modules

#required libs fot compiling
sudo apt install -y libboost-dev
sudo apt install -y libgnutls28-dev openssl libtiff-dev pybind11-dev
sudo apt install -y qtbase5-dev libqt5core5a libqt5widgets5
sudo apt install -y meson cmake
sudo apt install -y python3-yaml python3-ply
sudo apt install -y libglib2.0-dev libgstreamer-plugins-base1.0-dev

#Install libcamera to interface with arduicam camera modules
git clone https://github.com/raspberrypi/libcamera.git
cd libcamera
meson setup build --buildtype=release -Dgstreamer=enabled -Dpycamera=enabled
ninja -C build install


# Bluetooth setup (with dongle)
set -e

# Detect Config File
CONFIG_FILE="/boot/firmware/config.txt"
if [ ! -f "$CONFIG_FILE" ]; then
    CONFIG_FILE="/boot/config.txt"
fi
echo "Using $CONFIG_FILE"

# Disable the internal bluetooth on the Raspberry Pi to prevent conflict between bluetooth providers
if ! grep -q "dtoverlay=disable-bt" "$CONFIG_FILE"; then
    echo "Disabling internal Bluetooth..."
    sudo bash -c "echo 'dtoverlay=disable-bt' >> $CONFIG_FILE"
else
    echo "Internal Bluetooth already disabled"
fi

# Unblock external bluetooth
echo "Unblocking Bluetooth via rfkill..."
sudo rfkill unblock bluetooth

# Bring up Bluetooth dongle
echo "Bringing up hci0..."
sudo hciconfig hci0 up || echo "Warning: hci0 may not exist or already UP"

# Create persistent systemd service
SERVICE_NAME="bt-up"
SCRIPT_PATH="/usr/local/bin/setup.sh"
if [ ! -f "$SCRIPT_PATH" ]; then
    echo "Copying script to $SCRIPT_PATH..."
    sudo cp "$0" "$SCRIPT_PATH"
    sudo chmod +x "$SCRIPT_PATH"
fi

SERVICE_FILE="/etc/systemd/system/$SERVICE_NAME.service"
if [ ! -f "$SERVICE_FILE" ]; then
    echo "Creating systemd service $SERVICE_FILE..."
    sudo bash -c "cat > $SERVICE_FILE" <<EOL
[Unit]
Description=Bring up USB Bluetooth dongle on boot
After=bluetooth.service

[Service]
Type=oneshot
ExecStart=$SCRIPT_PATH

[Install]
WantedBy=multi-user.target
EOL
    sudo systemctl daemon-reexec
    sudo systemctl enable "$SERVICE_NAME.service"
else
    echo "Systemd service already exists"
fi

# Verification
echo "=== Status Check ==="
hciconfig
rfkill list bluetooth

# Optional BLE test (needs bleak)
echo "=== Optional BLE scan test ==="
python3 - << 'EOF' || echo "Python BLE scan failed (ignore if bleak not installed)"
import asyncio
from bleak import BleakScanner

async def main():
    try:
        devices = await BleakScanner(adapter="hci0").discover(timeout=5)
        print("Discovered BLE devices:")
        for d in devices:
            print(d)
    except Exception as e:
        print("BLE scan error:", e)

asyncio.run(main())
EOF

# End flag
echo "Setup complete! Reboot for changes to fully apply."
