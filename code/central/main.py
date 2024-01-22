from aliot.aliot_obj import AliotObj
import asyncio
from bleak import BleakScanner
from device import Device
from bleakScanning import BleakScanning
import time

sensor_iot = AliotObj("sicro")


def handle_change_sleep(data):
    print("NEW SLEEP TIME : ")
    print(data)
    reader.updated_devices = []
    reader.new_sleep_value = data
    
def send_data(device:Device):
    device_data=device.data
    sensors_values = {
    1 : 99.99, # temp
    2 : 99.99, # hum
    3 : 99.99, # lum
    4 : 99.99, # gnd temp
    5 : 99.99, # gnd hum
    254 : 99.99 # bat
    }
    
    data_mapping = {
    1: (3, 4),
    2: (6, 7),
    3: (9, 10),
    4: (12, 13),
    5: (15, 16),
    254: (18, 19)
}

    for sensor, (index1, index2) in data_mapping.items():
        whole_val = device_data[index1]
        decimal_val = device_data[index2]

        # Set the negative value
        if decimal_val > 99:
            decimal_val = decimal_val - 100
            whole_val = whole_val * -1

        sensors_values[sensor] = round(whole_val + (decimal_val / 100), 2)


    print("Values received from device " + str(device.index))
    print(f"\tTemperature: {sensors_values[1]}")
    print(f"\tHumidity: {sensors_values[2]}")
    print(f"\tLuminosity: {sensors_values[3]}")
    print(f"\tGround temperature: {sensors_values[4]}")
    print(f"\tGround humidity: {sensors_values[5]}")
    print(f"\tBattery: {sensors_values[254]}")

    path = f'/doc/{device.index}'
    sensor_iot.update_doc({
        f'{path}/humidity' : sensors_values[2],
        f'{path}/temperature' : sensors_values[1],
        f'{path}/luminosite' : sensors_values[3],
        f'{path}/gnd_temperature' : sensors_values[4],
        f'{path}/gnd_humidity' : sensors_values[5],
        f'{path}/batterie' : sensors_values[254],
        f'{path}/id' : device.id
        })

def send_logs(msg: str):
    data = {
        "date":  time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()),
        "text": msg
    }

    print("\033[33m" + f"LOG: {data['date']} - {data['text']}" + "\033[0m")

    sensor_iot.update_component('log', data)


def start():
    '''Main function'''
    print("START MAIN")
    reader.new_sleep_value = sensor_iot.get_doc('/doc/sleep_time')

    # READING BLEAK
    reader.start_scanning()

reader = BleakScanning(send_data, send_logs, False)

# def handle_err(code: str, msg: str):
#     print("ERROR")
#     print(code)
#     print(msg)

def handle_end():
    print("END")


sensor_iot.on_start(callback=start)
sensor_iot.on_action_recv(action_id='change_sleep_time', callback=handle_change_sleep)
# sensor_iot.on_err(callback=handle_err)
sensor_iot.on_end(callback=handle_end)
sensor_iot.run()

# except KeyboardInterrupt as e:
    # pass
# except Exception as e:
#     print("ERROR")
#     print(e)

# start()
