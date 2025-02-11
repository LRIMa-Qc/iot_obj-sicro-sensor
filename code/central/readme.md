# Steps to run the central device

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
