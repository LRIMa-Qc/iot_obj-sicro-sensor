# Steps to run the central device

## 0. Manually install BlueZ
We currently need to manually install BlueZ to get the latest version. The version in the apt repository is too old (`5.66`) and has some bugs.
We know that currently BlueZ `5.82` works well with the project. These steps can therefore be skipped if you have already installed BlueZ `5.82` or later.

Check the version of BlueZ installed on your system with the following command:

```bash
bluetoothd --version
```

If the version is less than `5.82`, you can follow these steps to install the latest version of BlueZ:

Install the dependencies:
```bash
sudo apt update
sudo apt install -y libdbus-1-dev libglib2.0-dev libudev-dev libical-dev libreadline-dev libusb-dev
```

Download the `5.82` version of BlueZ from the official website:
```bash
cd ~
wget https://www.kernel.org/pub/linux/bluetooth/bluez-5.82.tar.xz
tar xvf bluez-5.82.tar.xz
cd bluez-5.82
```

Compile and install BlueZ:
```bash
./configure --prefix=/usr --mandir=/usr/share/man \
				--sysconfdir=/etc --localstatedir=/var
make -j$(nproc)
sudo make install
```

Reload the Bluetooth service:
```bash
sudo systemctl daemon-reexec
sudo systemctl daemon-reload
sudo systemctl restart bluetooth
sudo systemctl enable bluetooth
```

Verify the installation:
```bash
sudo systemctl status bluetooth
```
> Should show `active (running)` if the installation was successful.

```
bluetoothd --version
```
> Should show `5.82` or later if the installation was successful.


## 1. Initialize the project and the venv

```bash
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

## 2. Run the project with the following command

```bash
python main.py
```


# USING SERVICES

## 1. Install PM2

First download npm
```bash
sudo apt-get install npm
```

Then download pm2 globally using npm
```bash
sudo npm i -g pm2
```

## 2. For using PM2 as the service manager, modify the file pm2.json to your needs and run the following command
### NOTE: You must have pm2 installed globally and the venv must be created and the requirements must be installed

```bash
pm2 start pm2.json
```

### To automatically start the service on boot, run the following command.
### NOTE : Read the output of `pm2 startup` !! You will have a command to paste in the terminal!

```bash
pm2 save
pm2 startup
```

# KNOWN ERRORS

# If there is an error saying the ble device is not found, try the following steps

## 1. try to run the following command

```bash
sudo bluetoothctl power on
```

### if the above command works, then you can add the following line to the file /etc/rc.local

```bash
sudo bluetoothctl power on
```
