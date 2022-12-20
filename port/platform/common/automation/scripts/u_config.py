#!/usr/bin/env python

'''Functions to help with packages in automation.'''

import os
import yaml

def parse_platform_specific(cfg, wanted_platform):
    """Recursive function that will parse platform specific config
       This will move all children of matching platform keys to its parent
       I.e. if current platform is "linux" this config:
          nrf5:
             version: 15.3.0_59ac345
             windows:
                 install_dir: C:/ubxlib_sdks/nRF5_SDK_15.3.0_59ac345
             linux:
                 install_dir: ${HOME}/sdks/nRF5_SDK_15.3.0_59ac345
             linux_arm:
                 install_dir: ${HOME}/sdks/nRF5_SDK_15.3.0_59ac345
       will become:
          nrf5:
             version: 15.3.0_59ac345
             install_dir: ${HOME}/sdks/nRF5_SDK_15.3.0_59ac345
    """
    newcfg = cfg.copy()
    for key, value in cfg.items():
        if key in ("linux", "linux_arm", "windows"):
            if key == wanted_platform:
                newcfg.update(value)
            del newcfg[key]
        elif isinstance(value, dict):
            newcfg[key] = parse_platform_specific(cfg[key], wanted_platform)

    return newcfg

def load_config_yaml(file_path, is_linux, is_arm):
    """Load a yaml config file
    Notes:
    * If the yaml file contains environmental variables these will first be expanded.
    * The yaml file can also contain platform specific config.
      Please see parse_platform_specific() how these are handled."""
    with open(file_path, 'r', encoding='utf8') as file:
        cfg = yaml.safe_load(os.path.expandvars(file.read()))
    wanted_platform = "windows"
    if is_linux:
        wanted_platform = "linux"
        if is_arm:
            wanted_platform = "linux_arm"
    return parse_platform_specific(cfg, wanted_platform)
