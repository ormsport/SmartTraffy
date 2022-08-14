from asyncio.log import logger
import os
import sys
import inspect
import argparse
from pathlib import Path
from typing import Optional
from functions import *
import yaml
import asyncio
from asyncio_mqtt import Client, ProtocolVersion, MqttError
import logging

# system global variable
sys_config = ""
HwMac = getHwMac()

# mqtt global vriable
mqtt_server = ""
mqtt_port = ""
mqtt_username = ""
mqtt_password = ""
mqtt_qos = ""
mqtt_version = ""
mqtt_macid = None
mqtt_client_id = ""
mqtt_config_set_topic = ""
mqtt_config_get_topic = ""
mqtt_control_set_topic = ""
mqtt_control_get_topic = ""
mqtt_status_topic = ""
mqtt_debug_topic = ""

# recorder global vriable
db_path = ""
db_update_interval = ""
db_keep_day = ""

FILE = Path(__file__).resolve()
ROOT = FILE.parents[0]  # get root directory
if str(ROOT) not in sys.path:
    sys.path.append(str(ROOT))  # add ROOT to PATH
ROOT = Path(os.path.relpath(ROOT, Path.cwd()))  # relative

if sys.version_info[0] == 3 and sys.version_info[1] >= 8 and sys.platform.startswith('win'):
    asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

# initialize function
def initialize(
    # initialize args    
    config=ROOT / 'config.yaml',  # default config file
    debug=False # disable debug
):
    # define to use global variable
    global sys_config, mqtt_server, mqtt_port, mqtt_version, mqtt_username, mqtt_password, mqtt_qos, mqtt_client_id, mqtt_config_set_topic, mqtt_config_get_topic, mqtt_control_set_topic, mqtt_control_get_topic, mqtt_status_topic, mqtt_debug_topic, reconnect_interval
    global db_path, db_update_interval, db_keep_day

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
    
    # mqtt client config
    mqtt_server = sys_config['mqtt']['server']
    mqtt_port = sys_config['mqtt']['port']
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
    mqtt_username = sys_config['mqtt']['username']
    mqtt_password = sys_config['mqtt']['password']
    mqtt_qos = sys_config['mqtt']['qos']
    #  topic
    if sys_config['mqtt']['mac_id'] == True:
        mqtt_client_id = sys_config['system']['name'] + "-" + getClientID(HwMac)
    else:
        mqtt_client_id = sys_config['system']['name']
    mqtt_config_set_topic = sys_config['system']['name'] + "/config/set"
    mqtt_config_get_topic = sys_config['system']['name'] + "/config/get"
    mqtt_control_set_topic = sys_config['system']['name'] + "/control/set"
    mqtt_control_get_topic = sys_config['system']['name'] + "/control/get"
    mqtt_status_topic = sys_config['system']['name'] + "/status"
    mqtt_debug_topic = sys_config['system']['name'] + "/debug"
    reconnect_interval = sys_config['mqtt']['reconn_interval']

    # recorder config
    db_path = sys_config['statistic']['db_path']
    db_update_interval = 60 * sys_config['statistic']['db_update_interval']
    db_keep_day = sys_config['statistic']['db_keep_day']

    # print basic information
    logger.info("Name: %s", sys_config['system']['name'])
    logger.info("Platform: %s", getPlatform())
    logger.info("MAC Address: %s", HwMac)

    # setup async loop
    asyncio.run(mqtt())
    #asyncio.run(update_db(sys_config))
    
async def obj_detect_1():
    config = sys_config['yolo']
    source = sys_config['stream']['camera_1_url']
    while True:
        async with objDetection(source=source,config=config) as ObjDetection1:
            pass

async def obj_detect_2():
    while True:
        print('this from obj_detect_2')

async def obj_detect_3():
    while True:
        print('this from obj_detect_3')

async def obj_detect_4():
    while True:
        print('this from obj_detect_1')

async def update_db():
    while True:
        print('this from update_db')
        await asyncio.sleep(db_update_interval)

async def mqtt_on_message(message):
    topic = message.topic
    payload = message.payload.decode('utf-8')
    print('topic:', topic)
    print('payload:', payload)

async def mqtt_loop():
    async with Client(
        hostname=mqtt_server,
        port=mqtt_port,
        protocol=mqtt_version,
        username=mqtt_username,
        password=mqtt_password,
        client_id=mqtt_client_id
    ) as mqtt_client:
        logger.info("Connected to MQTT server")
        logger.info("MQTT Client ID: %s", mqtt_client_id)
        async with mqtt_client.unfiltered_messages() as messages:
            # config set topic
            logger.info("Subscribing to: %s",mqtt_config_set_topic)
            await mqtt_client.subscribe(mqtt_config_set_topic, mqtt_qos)
            # control set topic
            logger.info("Subscribing to: %s",mqtt_control_set_topic)
            await mqtt_client.subscribe(mqtt_control_set_topic, mqtt_qos)
            async for message in messages:
                asyncio.ensure_future(mqtt_on_message(message))

async def mqtt():
    while True:
        try:
            await mqtt_loop()
        except MqttError as error:
            logger.error(f'MQTT: Error "{error}". Reconnecting in {reconnect_interval} seconds.')
        finally:
            await asyncio.sleep(reconnect_interval)

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

# pass arguments to initialize function
def init(args):
    initialize(**vars(args))

# run the code if this is main
if __name__ == "__main__":
    args = parse_args()
    init(args)