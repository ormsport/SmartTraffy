import socket
import platform
from getmac import get_mac_address as getmac
import string
import sys
import json
from SmartTraffy import ROOT
sys.path.append('yolov5')

from pathlib import Path
import torch
import torch.backends.cudnn as cudnn

from yolov5.models.common import DetectMultiBackend
from yolov5.utils.custom_dataloaders import IMG_FORMATS, VID_FORMATS, LoadImages, LoadStreams
from yolov5.utils.general import (LOGGER, Profile, check_file, check_img_size, check_imshow, check_requirements, colorstr, cv2,
                           increment_path, non_max_suppression, print_args, scale_boxes, strip_optimizer, xyxy2xywh)
from yolov5.utils.plots import Annotator, colors, save_one_box
from yolov5.utils.torch_utils import select_device, smart_inference_mode

@smart_inference_mode()

# variable class
class mqttConfig:
    def __init__(self, host, port, version, username, password, topicOut, topicIn, qos, clientId, reConTime):
        self.host = host
        self.port = port
        self.version = version
        self.username = username
        self.password = password
        self.out_topic = topicOut
        self.in_topic = topicIn
        self.qos = qos
        self.clientId = clientId
        self.reConTime = reConTime

# get host by name
def getHost(name):
    host = socket.gethostbyname(name)
    return host

# get system platform
def getPlatform():
    return platform.platform()

# get system os
def getOs():
    return platform.system()

# get hw mac address
def getHwMac():
    if getOs() == "Windows":
        return getmac(interface="Ethernet")
    else:
        route = "/proc/net/route"
        with open(route) as f:
            for line in f.readlines():
                try:
                    iface, dest, _, flags, _, _, _, _, _, _, _, =  line.strip().split()
                    if dest != '00000000' or not int(flags, 16) & 2:
                        continue
                    return iface
                except:
                    continue

# generate mqtt client id
def getClientID(mac):
    mac = mac.translate(str.maketrans('', '', string.punctuation))
    return mac[6:12]

def str2bool(str):
    if str.lower() == 'true':
        return True
    else:
        return False

# yolo v5 - object detection
class YOLOv5:
    def __init__(self, config):
        # setup param
        config = config
        self.weights = ROOT / str(config['model_path']) / str(config['weights'])
        self.data = ROOT / str(config['model_path']) / str(config['data'])
        self.imgsz = config['img_size']
        self.conf_thres = float(config['conf_thres'])
        self.iou_thres = float(config['iou_thres'])
        self.max_det = int(config['max_det'])
        self.device = str(config['device'])
        self.view_img = str2bool(str(config['view_img']))
        self.save_txt = str2bool(str(config['save_txt']))
        self.save_conf = str2bool(str(config['save_conf']))
        self.save_crop = str2bool(str(config['save_crop']))
        self.nosave = str2bool(str(config['nosave']))
        self.classes = config['classes']
        self.agnostic_nms = str2bool(str(config['agnostic_nms']))
        self.augment = str2bool(str(config['augment']))
        self.visualize = str2bool(str(config['visualize']))
        self.update = str2bool(str(config['update']))
        self.project = ROOT / str(config['model_path']) / str(config['project'])
        self.name = str(config['name'])
        self.exist_ok = str2bool(str(config['exist_ok']))
        self.line_thickness = int(config['line_thickness'])
        self.hide_labels = str2bool(str(config['hide_labels']))
        self.hide_conf = str2bool(str(config['hide_conf']))
        self.half = str2bool(str(config['half']))
        self.dnn = str2bool(str(config['dnn']))
        self.vid_stride = int(config['vid_stride'])

        self.save_img = bool
        self.is_file = bool
        self.is_url = bool
        self.webcam = bool
        self.bs = int
        self.transforms = None

        # Directories
        self.save_dir = increment_path(Path(self.project) / self.name, exist_ok=self.exist_ok)  # increment run
        (self.save_dir / 'labels' if self.save_txt else self.save_dir).mkdir(parents=True, exist_ok=True)  # make dir
        
        # Load model
        self.device = select_device(self.device)
        self.model = DetectMultiBackend(self.weights, device=self.device, dnn=self.dnn, data=self.data, fp16=self.half)
        self.stride, self.names, self.pt = self.model.stride, self.model.names, self.model.pt
        self.imgsz = check_img_size(self.imgsz, s=self.stride)  # check image size

    def loadData(self, source, area):
        self.save_img = not self.nosave and not source.endswith('.txt')  # save inference images
        self.is_file = Path(source).suffix[1:] in (IMG_FORMATS + VID_FORMATS)
        self.is_url = source.lower().startswith(('rtsp://', 'rtmp://', 'http://', 'https://'))
        self.webcam = source.isnumeric() or source.endswith('.txt') or (self.is_url and not self.is_file)
        if self.is_url and self.is_file:
            source = check_file(source)  # download
        # Dataloader
        if self.webcam:
            self.view_img = check_imshow()
            dataset = LoadStreams(source, area, img_size=self.imgsz, stride=self.stride, auto=self.pt, vid_stride=self.vid_stride)
            self.bs = len(dataset)  # batch_size
        else:
            self.bs = 1  # batch_size
            dataset = LoadImages(source, area, img_size=self.imgsz, stride=self.stride, auto=self.pt, vid_stride=self.vid_stride)
        self.transforms = dataset.transforms
        return dataset

    def detect(self, data, area):
        path, im, im0s, s, im1s = data[0], data[1], data[2], data[4], data[5]
        seen, windows, dt = 0, [], (Profile(), Profile(), Profile())
        x, y, w, h = area[0], area[1], area[2], area[3]

        # Run inference
        self.model.warmup(imgsz=(1 if self.pt else self.bs, 3, *self.imgsz))  # warmup

        with dt[0]:
            im = torch.from_numpy(im).to(self.device)
            im = im.half() if self.model.fp16 else im.float()  # uint8 to fp16/32
            im /= 255  # 0 - 255 to 0.0 - 1.0
            if len(im.shape) == 3:
                im = im[None]  # expand for batch dim

        # Inference
        with dt[1]:
            pred = self.model(im)

        # NMS
        with dt[2]:
            pred = non_max_suppression(pred, self.conf_thres, self.iou_thres, self.classes, self.agnostic_nms, max_det=self.max_det)
        
        # Process predictions
        for i, det in enumerate(pred):  # per image
            result = []
            seen += 1
            if self.webcam:  # batch_size >= 1
                p, im0, im1 = path[i], im0s[i].copy(), im1s[i].copy()
                s += f'{i}: '
            else:
                p, im0, im1 = path, im0s.copy(), im1s.copy()
            
            s += '%gx%g ' % im.shape[2:]  # print string
            annotator = Annotator(im1, line_width=self.line_thickness, example=str(self.names))
            if len(det):
                # Rescale boxes from img_size to im0 size
                det[:, :4] = scale_boxes(im.shape[2:], det[:, :4], im0.shape).round()

                # Print results
                for c in det[:, -1].unique():
                    n = (det[:, -1] == c).sum()  # detections per class
                    s += f"{n} {self.names[int(c)]}{'s' * (n > 1)}, "  # add to string
                    tmp = {self.names[int(c)]:int(n)}
                    result.append(tmp)
                
                # Write results
                for *xyxy, conf, cls in reversed(det):
                    if self.view_img:
                        c = int(cls)  # integer class
                        label = None if self.hide_labels else (self.names[c] if self.hide_conf else f'{self.names[c]} {conf:.2f}')
                        xyxy[0] += x
                        xyxy[1] += y
                        xyxy[2] += x
                        xyxy[3] += y
                        annotator.box_label(xyxy, label, color=colors(c, True))

            # Stream results
            im1 = annotator.result()
            if self.view_img:
                if platform.system() == 'Linux' and p not in windows:
                    windows.append(p)
                    cv2.namedWindow(str(p), cv2.WINDOW_NORMAL | cv2.WINDOW_KEEPRATIO)  # allow window resize (Linux)
                    cv2.resizeWindow(str(p), im1.shape[1], im1.shape[0])
                cv2.imshow(str(p), im1)
                cv2.waitKey(1)  # 1 millisecond

        # Print time (inference-only)
        #print(f"{s}{'' if len(det) else '(no detections), '}{dt[1].dt * 1E3:.1f}ms")
        result = [{i:j for x in result for i,j in x.items()}]
        return json.dumps(result)
"""
class async_mqtt:
    def __init__(self, config):
        self.host = config.host
        self.port = config.port
        self.username = config.username
        self.password = config.password
        self.topic = config.topic
        self.qos = config.qos
        self.version = config.version 
        self.clientId = config.clientid
        self.reconnect_interval = config.reConTime
"""