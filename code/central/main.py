from aliot.aliot_obj import AliotObj

from device import Device
from reader import Reader
import time

sensor_iot = AliotObj("sirco")

def start():
    '''Main function'''

    #Start the serial port reader
    reader = Reader("/dev/ttyACM0", 115200, send_data, send_logs, False)
    print("Serial port reader started")

def send_data(device:Device):
    sensors_values = {
        1 : 99.99, # temp
        2 : 99.99, # hum
        3 : 99.99, # lum
        4 : 99.99, # gnd temp
        5 : 99.99, # gnd hum
        254 : 99.99 # bat
    }

    data_queue = device.getDataQueue()
    while not data_queue.empty() and data_queue.qsize() >= 3:
        val_id = data_queue.get()
        whole_val = data_queue.get()
        decimal_val = data_queue.get()

        # Set the negative value
        if decimal_val > 99:
            decimal_val = decimal_val - 100
            whole_val = whole_val * -1

        sensors_values[val_id] = whole_val + (decimal_val / 100)

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

sensor_iot.on_start(callback=start)
sensor_iot.run()
