from invoke import Collection, Config
from sys import platform
import os
import yaml
from . import nrfconnect, nrf5

def parse_platform_specific(cfg, is_linux):
    """Recursive function that will parse platform specific config
       This will move all children of matching platform keys to its parent
       I.e. if current platform is "linux" this config:
          nrf5:
             version: 15.3.0_59ac345
             windows:
                 install_dir: C:/ubxlib_sdks/nRF5_SDK_15.3.0_59ac345
             linux:
                 install_dir: ${HOME}/sdks/nRF5_SDK_15.3.0_59ac345
       will become:
          nrf5:
             version: 15.3.0_59ac345
             install_dir: ${HOME}/sdks/nRF5_SDK_15.3.0_59ac345
    """
    newcfg = cfg.copy()
    for key, value in cfg.items():
        if key == "linux" or key == "windows":
            is_linux_setting = key == "linux"
            if is_linux == is_linux_setting:
                newcfg.update(value)
            del newcfg[key]
        elif isinstance(value, dict):
            newcfg[key] = parse_platform_specific(cfg[key], is_linux)

    return newcfg


tasks_dir = os.path.dirname(os.path.abspath(__file__))
vscode_dir = os.path.abspath(os.path.join(tasks_dir, ".."))
root_dir = os.path.abspath(os.path.join(vscode_dir, ".."))

cfg_dir = vscode_dir
if platform == "win32":
    is_linux = False
else:
    is_linux = True

# Load the config file
with open(os.path.join(cfg_dir, "config.yml"), 'r') as f:
    cfg = yaml.safe_load(os.path.expandvars(f.read()))
ns = Collection()

cfg = parse_platform_specific(cfg, is_linux)

cfg['is_linux'] = is_linux
cfg['root_dir'] = root_dir
cfg['cfg_dir'] = cfg_dir

ns.configure(cfg)
ns.add_collection(nrfconnect)
ns.add_collection(nrf5)