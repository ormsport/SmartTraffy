from asyncio.log import logger
import os
import sys
import inspect
import argparse
from pathlib import Path
from tokenize import String
from typing import Optional
from functions import *
import yaml
import asyncio
from asyncio_mqtt import Client, ProtocolVersion, MqttError 
import logging
import multiprocessing as mp

# system global variable
sys_config = ""
HwMac = ""

# mqtt global vriable
client = None
mqttCfg = ""
en = False

# multiprocessing queue
q0 = mp.Queue()
q1 = mp.Queue()
q2 = mp.Queue()
q3 = mp.Queue()
p1 = None
p2 = None
p3 = None
p4 = None

FILE = Path(__file__).resolve()
ROOT = FILE.parents[0]  # get root directory
if str(ROOT) not in sys.path:
    sys.path.append(str(ROOT))  # add ROOT to PATH
ROOT = Path(os.path.relpath(ROOT, Path.cwd()))  # relative

if sys.version_info[0] == 3 and sys.version_info[1] >= 8 and sys.platform.startswith('win'):
    asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
    
def obj_detect_1(sys_config, q0):
    config = sys_config['yolo']
    source = sys_config['stream']['camera1_url']
    area = sys_config['detect_area']['camera1_area']
    yolo = YOLOv5(config=config)
    data = yolo.loadData(source, area)
    data.count = 0
    while True:
        result = yolo.detect(next(data), area)
        q0.put(result)


def obj_detect_2(sys_config, q1):
    config = sys_config['yolo']
    source = sys_config['stream']['camera2_url']
    area = sys_config['detect_area']['camera2_area']
    yolo = YOLOv5(config=config)
    data = yolo.loadData(source, area)
    data.count = 0
    while True:
        result = yolo.detect(next(data), area)
        q1.put(result)

def obj_detect_3(sys_config, q2):
    config = sys_config['yolo']
    source = sys_config['stream']['camera3_url']
    area = sys_config['detect_area']['camera3_area']
    yolo = YOLOv5(config=config)
    data = yolo.loadData(source, area)
    data.count = 0
    while True:
        result = yolo.detect(next(data), area)
        q2.put(result)

def obj_detect_4(sys_config, q3):
    config = sys_config['yolo']
    source = sys_config['stream']['camera4_url']
    area = sys_config['detect_area']['camera4_area']
    yolo = YOLOv5(config=config)
    data = yolo.loadData(source, area)
    data.count = 0
    while True:
        result = yolo.detect(next(data), area)
        q3.put(result)

async def watchdog():
    global p1, p2, p3, p4
    while True:
        """
        if not p1.is_alive():
            p1.start()
        if not p2.is_alive():
            p2.start()
        if not p3.is_alive():
            p3.start()
        if not p4.is_alive():
            p4.start()
        """
        await asyncio.sleep(1)

async def processResult():
    global sys_config, q0, q0, q2, q3, en
    timing = sys_config['light']['vehicle_timing']
    max_timing = sys_config['light']['max_timing']
    min_timing = sys_config['light']['min_timing']
    while True:
        await asyncio.sleep(5)

        #0
        while not q0.empty():   #clear queue before get new value
            q0.get()  # as docs say: Remove and return an item from the queue.
        id0_vehicle = json.loads(q0.get())

        #1
        while not q1.empty():   #clear queue before get new value
            q1.get()  # as docs say: Remove and return an item from the queue.
        id1_vehicle = json.loads(q1.get())

        #2
        while not q2.empty():   #clear queue before get new value
            q2.get()  # as docs say: Remove and return an item from the queue.
        id2_vehicle = json.loads(q2.get())

        #3
        while not q3.empty():   #clear queue before get new value
            q3.get()  # as docs say: Remove and return an item from the queue.
        id3_vehicle = json.loads(q3.get())

        id0_timing = 0
        id1_timing = 0
        id2_timing = 0
        id3_timing = 0

        # calculate only exist keys(id0) 
        if '01_CAR' in id0_vehicle[0]:
            id0_timing += id0_vehicle[0]['01_CAR'] * timing[0]
        if '02_VAN' in id0_vehicle[0]:
            id0_timing += id0_vehicle[0]['02_VAN'] * timing[1]
        if '03_BUS' in id0_vehicle[0]:
            id0_timing += id0_vehicle[0]['03_BUS'] * timing[2]
        if '04_TRUCK' in id0_vehicle[0]:
            id0_timing += id0_vehicle[0]['04_TRUCK'] * timing[3]

        # calculate only exist keys(id1) 
        if '01_CAR' in id1_vehicle[0]:
            id1_timing += id1_vehicle[0]['01_CAR'] * timing[0]
        if '02_VAN' in id1_vehicle[0]:
            id1_timing += id1_vehicle[0]['02_VAN'] * timing[1]
        if '03_BUS' in id1_vehicle[0]:
            id1_timing += id1_vehicle[0]['03_BUS'] * timing[2]
        if '04_TRUCK' in id1_vehicle[0]:
            id1_timing += id1_vehicle[0]['04_TRUCK'] * timing[3]

        # calculate only exist keys(id2) 
        if '01_CAR' in id2_vehicle[0]:
            id2_timing += id2_vehicle[0]['01_CAR'] * timing[0]
        if '02_VAN' in id2_vehicle[0]:
            id2_timing += id2_vehicle[0]['02_VAN'] * timing[1]
        if '03_BUS' in id2_vehicle[0]:
            id2_timing += id2_vehicle[0]['03_BUS'] * timing[2]
        if '04_TRUCK' in id2_vehicle[0]:
            id2_timing += id2_vehicle[0]['04_TRUCK'] * timing[3]

        # calculate only exist keys(id3) 
        if '01_CAR' in id3_vehicle[0]:
            id3_timing += id3_vehicle[0]['01_CAR'] * timing[0]
        if '02_VAN' in id3_vehicle[0]:
            id3_timing += id3_vehicle[0]['02_VAN'] * timing[1]
        if '03_BUS' in id3_vehicle[0]:
            id3_timing += id3_vehicle[0]['03_BUS'] * timing[2]
        if '04_TRUCK' in id3_vehicle[0]:
            id3_timing += id3_vehicle[0]['04_TRUCK'] * timing[3]

        
        # limit output by max_timing
        if id0_timing > max_timing[0]:
            id0_timing = max_timing[0]
        if id1_timing > max_timing[1]:
            id1_timing = max_timing[1]
        if id2_timing > max_timing[2]:
            id2_timing = max_timing[2]
        if id3_timing > max_timing[3]:
            id3_timing = max_timing[3]

        # limit output by min_timing
        if id0_timing < min_timing[0]:
            id0_timing = min_timing[0]
        if id1_timing < min_timing[1]:
            id1_timing = min_timing[1]
        if id2_timing < min_timing[2]:
            id2_timing = min_timing[2]
        if id3_timing < min_timing[3]:
            id3_timing = min_timing[3]

        res = [id0_timing, id1_timing, id2_timing, id3_timing]
        final_cmd = {"timing": [value for value in res]}
        final_cmd = json.dumps(final_cmd)
        print(final_cmd)
        if en:
            await publish(final_cmd)

async def mqtt_on_message(message):
    global en, mqttCfg
    topic = message.topic
    payload = message.payload.decode('utf-8')
    print('topic:', topic)
    print('payload:', payload)
    cmd = json.loads(payload)
    if 'mode' in cmd:
        mode = cmd['mode']
        if mode == 0:
            en = True
        else:
            en = False

async def publish(message, retain=False):
    global mqttCfg, client
    try: 
        await client.publish(mqttCfg.out_topic, message, qos=mqttCfg.qos, retain=retain)
        logger.info(f"Published message '{message}' to topic '{mqttCfg.out_topic}' with QoS {mqttCfg.qos}")
    except MqttError as error:
        logger.info(f"Error publishing message: {error}")

# mqtt subscribe
async def mqtt_loop():
    global mqttCfg, client
    await client.connect()
    logger.info("Connected to MQTT server")
    logger.info("MQTT Client ID: %s", mqttCfg.clientId)
    async with client.unfiltered_messages() as messages:
        # config set topic
        await client.subscribe(mqttCfg.out_topic, mqttCfg.qos)
        logger.info("Subscribed to: %s",mqttCfg.out_topic)
        async for message in messages:
            asyncio.ensure_future(mqtt_on_message(message))

async def mqtt():
    global mqttCfg
    while True:
        try:
            await mqtt_loop()
        except MqttError as error:
            logger.error(f'MQTT: Error "{error}". Reconnecting in {mqttCfg.reConTime} seconds.')
        finally:
            await asyncio.sleep(mqttCfg.reConTime)

# initialize function
async def initialize(
    # initialize args    
    config=ROOT / 'config.yaml',  # default config file
    debug=False # disable debug
):
    # define to use global variable
    global HwMac, sys_config, mqttCfg, client
    global db_path, db_update_interval, db_keep_day

    HwMac = getHwMac()

    # config logger
    if not debug:
        logging.basicConfig(
            level=logging.INFO,
            format="%(asctime)s | %(levelname)s | %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
            force=True
        )
    else:
        logging.basicConfig(
            level=logging.DEBUG,
            format="%(asctime)s | %(levelname)s | %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
            force=True
        )

    # load config from file
    with open(config, "r") as stream:
        try:
            logger.debug("Opening config file")
            sys_config = yaml.safe_load(stream)
            logger.debug("Config loaded= %s", sys_config)
            logger.info("Config loaded")
        except yaml.YAMLError as exc:
            logger.error("Config load failed")
            logger.debug("Config load error=", exc)
    
    # mqtt version
    mqtt_version = str(sys_config['mqtt']['version'])
    if mqtt_version == "3.1":
        mqtt_version = ProtocolVersion.V31
    elif mqtt_version == "3.1.1":
        mqtt_version = ProtocolVersion.V311
    elif mqtt_version == "5":
        mqtt_version = ProtocolVersion.V5
    else:
        logger.error("MQTT version in config file invalid.")
        sys.exit()
    
    # mqtt client id
    if sys_config['mqtt']['mac_id'] == True:
        mqtt_client_id = sys_config['system']['name'] + "-" + getClientID(HwMac)
    else:
        mqtt_client_id = sys_config['system']['name']

    # set mqtt config
    global mqttCfg 
    mqttCfg = mqttConfig(sys_config['mqtt']['host'], sys_config['mqtt']['port'], mqtt_version, sys_config['mqtt']['username'], sys_config['mqtt']['password'], sys_config['mqtt']['out_topic'], sys_config['mqtt']['in_topic'], sys_config['mqtt']['qos'], mqtt_client_id, sys_config['mqtt']['reconn_interval'])
    client = Client(
            hostname=mqttCfg.host,
            port=mqttCfg.port,
            protocol=mqttCfg.version,
            username=mqttCfg.username,
            password=mqttCfg.password,
            client_id=mqttCfg.clientId
        )

    # data logger config
    db_path = sys_config['statistic']['db_path']
    db_update_interval = 60 * sys_config['statistic']['db_update_interval']
    db_keep_day = sys_config['statistic']['db_keep_day']

    # print basic information
    logger.info("Name: %s", sys_config['system']['name'])
    logger.info("Platform: %s", getPlatform())
    logger.info("Number of cpu: %d", mp.cpu_count())
    logger.info("MAC Address: %s", HwMac)

    # start mqtt
    task1 = asyncio.create_task(mqtt())

    # start multiprocessing
    global q0, q1, q2, q3
    global p1, p2, p3, p4
    p1 = mp.Process(target=obj_detect_1, args=(sys_config, q0, ))
    p2 = mp.Process(target=obj_detect_2, args=(sys_config, q1, ))
    p3 = mp.Process(target=obj_detect_3, args=(sys_config, q2, ))
    p4 = mp.Process(target=obj_detect_4, args=(sys_config, q3, ))
    p1.start()
    p2.start()
    p3.start()
    p4.start()
    # start process result
    task2 = asyncio.create_task(processResult())
    # start watchdog
    task3 = asyncio.create_task(watchdog())
    await asyncio.gather(task1, task2, task3)

    #quit monitor
    try:
        while True:
            pass
    except KeyboardInterrupt:
        print("Main process interrupted by user, stopping process")
        p1.terminate()
        p1.join()
        p2.terminate()
        p2.join()
        p3.terminate()
        p3.join()
        p4.terminate()
        p4.join()

# print arguments function
def print_args(args: Optional[dict] = None, show_file=True, show_fcn=False):
    # Print function arguments (optional args dict)
    x = inspect.currentframe().f_back  # previous frame
    file, _, fcn, _, _ = inspect.getframeinfo(x)
    if args is None:  # get args automatically
        args, _, _, frm = inspect.getargvalues(x)
        args = {k: v for k, v in frm.items() if k in args}
    s = (f'{Path(file).stem}: ' if show_file else '') + (f'{fcn}: ' if show_fcn else '')
    print(s + ', '.join(f'{k}={v}' for k, v in args.items()))

# arguments parser function
def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', type=str, default=ROOT / 'config.yaml', help='path to config file')
    parser.add_argument('--debug', action='store_true', help='enable debug mode')
    args = parser.parse_args()
    print_args(vars(args))
    return args

# run the code if this is main
if __name__ == "__main__":
    args = parse_args()
    asyncio.run(initialize(**vars(args)))