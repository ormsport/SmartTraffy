import os
import sys
import inspect
import argparse
from pathlib import Path
from typing import Optional
import yaml

FILE = Path(__file__).resolve()
ROOT = FILE.parents[0]  # get root directory
if str(ROOT) not in sys.path:
    sys.path.append(str(ROOT))  # add ROOT to PATH
ROOT = Path(os.path.relpath(ROOT, Path.cwd()))  # relative

def run(
        config=ROOT / 'config.yaml',  # config file
        live=False,  # do not show live streaming
):
    # load config from yaml
    with open(config, "r") as stream:
        try:
            config = yaml.safe_load(stream)
            print('config loaded=', config)
        except yaml.YAMLError as exc:
            print('config load error=', exc)

    # run the program

    



def print_args(args: Optional[dict] = None, show_file=True, show_fcn=False):
    # Print function arguments (optional args dict)
    x = inspect.currentframe().f_back  # previous frame
    file, _, fcn, _, _ = inspect.getframeinfo(x)
    if args is None:  # get args automatically
        args, _, _, frm = inspect.getargvalues(x)
        args = {k: v for k, v in frm.items() if k in args}
    s = (f'{Path(file).stem}: ' if show_file else '') + (f'{fcn}: ' if show_fcn else '')
    print(s + ', '.join(f'{k}={v}' for k, v in args.items()))

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', type=str, default=ROOT / 'config.yaml', help='path to config file')
    parser.add_argument('--live', action='store_true', help='show live streaming from cctv')
    args = parser.parse_args()
    print_args(vars(args))
    return args

def main(args):
    run(**vars(args))

if __name__ == "__main__":
    args = parse_args()
    main(args)