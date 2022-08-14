import socket
import platform
from getmac import get_mac_address as getmac
import string
import sys
import asyncio

sys.path.append('./yolov5')
from SmartTraffy import ROOT
from pathlib import Path
import torch
import torch.backends.cudnn as cudnn
from yolov5.models.common import DetectMultiBackend
from yolov5.utils.dataloaders import IMG_FORMATS, VID_FORMATS, LoadImages, LoadStreams
from yolov5.utils.general import (LOGGER,check_file, check_img_size, check_imshow, check_requirements, colorstr, cv2,
                           increment_path, non_max_suppression, print_args, scale_coords, strip_optimizer, xyxy2xywh)
from yolov5.utils.plots import Annotator, colors, save_one_box
from yolov5.utils.torch_utils import select_device, time_sync
@torch.no_grad()

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
    mac = getHwMac()
    mac = mac.translate(str.maketrans('', '', string.punctuation))
    return mac[6:12]

def str2bool(str):
    if str.lower() == 'true':
        return True
    else:
        return False

def array2int(array):
    result = None
    length = len(array)
    if length == 0:
        return None
    else:
        for n in array:
            result += array[n]
        return result


# config file validation
def configValidation(path):
    pass

# object detection
async def objDetectRun():
    pass

# model loader
class objDetection:
    def __init__(
            self,
            source: str,
            config: str
        ):
        # setup config
        weights = ROOT + "/" + str(config['weights'])
        data = ROOT + "/" + str(config['data'])
        imgsz = (config['img_size'], config['img_size'])
        conf_thres = float(config['conf_thres'])
        iou_thres = float(config[''])
        max_det = int(config['max_det'])
        device = str(config['device'])
        view_img = str2bool(str(config['view_img']))
        save_txt = str2bool(str(config['save_txt']))
        save_conf = str2bool(str(config['save_conf']))
        save_crop = str2bool(str(config['save_crop']))
        nosave = str2bool(str(config['nosave']))
        classes = array2int(config['classes'])
        agnostic_nms = str2bool(str(config['agnostic_nms']))
        augment = str2bool(str(config['augment']))
        visualize = str2bool(str(config['visualize']))
        update = str2bool(str(config['update']))
        project = ROOT + "/" + str(config['project'])
        name = str(config['name'])
        exist_ok = str2bool(str(config['exist_ok']))
        line_thickness = int(config['line_thickness'])
        hide_labels = str2bool(str(config['hide_labels']))
        hide_conf = str2bool(str(config['hide_conf']))
        half = str2bool(str(config['half']))
        dnn = str2bool(str(config['dnn']))

        loop = asyncio.get_event_loop()
        running: "asyncio.Future[int]" = asyncio.Future()

        save_img = not config['nosave'] and not source.endswith('.txt')  # save inference images
        is_file = Path(source).suffix[1:] in (IMG_FORMATS + VID_FORMATS)
        is_url = source.lower().startswith(('rtsp://', 'rtmp://', 'http://', 'https://'))
        webcam = source.isnumeric() or source.endswith('.txt') or (is_url and not is_file)
        if is_url and is_file:
            source = check_file(source)  # download
        
        # Directories
        save_dir = increment_path(Path(project) / name, exist_ok=exist_ok)  # increment run
        (save_dir / 'labels' if save_txt else save_dir).mkdir(parents=True, exist_ok=True)  # make dir

        # Load model
        device = select_device(config['device'])
        model = DetectMultiBackend(weights, device=device, dnn=dnn, data=data, fp16=half)
        stride, names, pt = model.stride, model.names, model.pt
        imgsz = check_img_size(imgsz, s=stride)  # check image size

        # Dataloader
        if webcam:
            view_img = check_imshow()
            cudnn.benchmark = True  # set True to speed up constant image size inference
            dataset = LoadStreams(source, img_size=imgsz, stride=stride, auto=pt)
            bs = len(dataset)  # batch_size
        else:
            dataset = LoadImages(source, img_size=imgsz, stride=stride, auto=pt)
            bs = 1  # batch_size
        vid_path, vid_writer = [None] * bs, [None] * bs

        # Run inference
        model.warmup(imgsz=(1 if pt else bs, 3, *imgsz))  # warmup
        dt, seen = [0.0, 0.0, 0.0], 0
        for path, im, im0s, vid_cap, s in dataset:
            t1 = time_sync()
            im = torch.from_numpy(im).to(device)
            im = im.half() if model.fp16 else im.float()  # uint8 to fp16/32
            im /= 255  # 0 - 255 to 0.0 - 1.0
            if len(im.shape) == 3:
                im = im[None]  # expand for batch dim
            t2 = time_sync()
            dt[0] += t2 - t1

            # Inference
            visualize = increment_path(save_dir / Path(path).stem, mkdir=True) if visualize else False
            pred = model(im, augment=config['augment'], visualize=config['visualize'])
            t3 = time_sync()
            dt[1] += t3 - t2

            # NMS
            pred = non_max_suppression(pred, config['conf_thres'], config['iou_thres'], config['classes'], config['agnostic_nms'], max_det=config['max_det'])
            dt[2] += time_sync() - t3

            # Second-stage classifier (optional)
            # pred = utils.general.apply_classifier(pred, classifier_model, im, im0s)

            # Process predictions
            for i, det in enumerate(pred):  # per image
                seen += 1
                if webcam:  # batch_size >= 1
                    p, im0, frame = path[i], im0s[i].copy(), dataset.count
                    s += f'{i}: '
                else:
                    p, im0, frame = path, im0s.copy(), getattr(dataset, 'frame', 0)

                p = Path(p)  # to Path
                save_path = str(save_dir / p.name)  # im.jpg
                txt_path = str(save_dir / 'labels' / p.stem) + ('' if dataset.mode == 'image' else f'_{frame}')  # im.txt
                s += '%gx%g ' % im.shape[2:]  # print string
                gn = torch.tensor(im0.shape)[[1, 0, 1, 0]]  # normalization gain whwh
                imc = im0.copy() if config['save_crop'] else im0  # for save_crop
                annotator = Annotator(im0, line_width=line_thickness, example=str(names))
                if len(det):
                    # Rescale boxes from img_size to im0 size
                    det[:, :4] = scale_coords(im.shape[2:], det[:, :4], im0.shape).round()

                    # Print results
                    for c in det[:, -1].unique():
                        n = (det[:, -1] == c).sum()  # detections per class
                        s += f"{n} {names[int(c)]}{'s' * (n > 1)}, "  # add to string

                    # Write results
                    for *xyxy, conf, cls in reversed(det):
                        if save_txt:  # Write to file
                            xywh = (xyxy2xywh(torch.tensor(xyxy).view(1, 4)) / gn).view(-1).tolist()  # normalized xywh
                            line = (cls, *xywh, conf) if save_conf else (cls, *xywh)  # label format
                            with open(f'{txt_path}.txt', 'a') as f:
                                f.write(('%g ' * len(line)).rstrip() % line + '\n')

                        if save_img or save_crop or view_img:  # Add bbox to image
                            c = int(cls)  # integer class
                            label = None if hide_labels else (names[c] if hide_conf else f'{names[c]} {conf:.2f}')
                            annotator.box_label(xyxy, label, color=colors(c, True))
                        if save_crop:
                            save_one_box(xyxy, imc, file=save_dir / 'crops' / names[c] / f'{p.stem}.jpg', BGR=True)

                # Stream results
                im0 = annotator.result()
                if view_img:
                    cv2.imshow(str(p), im0)
                    cv2.waitKey(1)  # 1 millisecond

                # Save results (image with detections)
                if save_img:
                    if dataset.mode == 'image':
                        cv2.imwrite(save_path, im0)
                    else:  # 'video' or 'stream'
                        if vid_path[i] != save_path:  # new video
                            vid_path[i] = save_path
                            if isinstance(vid_writer[i], cv2.VideoWriter):
                                vid_writer[i].release()  # release previous video writer
                            if vid_cap:  # video
                                fps = vid_cap.get(cv2.CAP_PROP_FPS)
                                w = int(vid_cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                                h = int(vid_cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                            else:  # stream
                                fps, w, h = 30, im0.shape[1], im0.shape[0]
                            save_path = str(Path(save_path).with_suffix('.mp4'))  # force *.mp4 suffix on results videos
                            vid_writer[i] = cv2.VideoWriter(save_path, cv2.VideoWriter_fourcc(*'mp4v'), fps, (w, h))
                        vid_writer[i].write(im0)

            # Print time (inference-only)
            LOGGER.info(f'{s}Done. ({t3 - t2:.3f}s)')

        # Print results
        t = tuple(x / seen * 1E3 for x in dt)  # speeds per image
        LOGGER.info(f'Speed: %.1fms pre-process, %.1fms inference, %.1fms NMS per image at shape {(1, 3, *imgsz)}' % t)
        if save_txt or save_img:
            s = f"\n{len(list(save_dir.glob('labels/*.txt')))} labels saved to {save_dir / 'labels'}" if save_txt else ''
            LOGGER.info(f"Results saved to {colorstr('bold', save_dir)}{s}")
        if update:
            strip_optimizer(weights)  # update model (to fix SourceChangeWarning)