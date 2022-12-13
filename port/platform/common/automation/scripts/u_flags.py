"""Utility module for handling u_flags"""

import os
import json
from hashlib import md5

import yaml

U_FLAG_YML = "u_flags.yml"

def get_cflags_from_u_flags_yml(cfg_dir, platform, target, store_new_hash=True):
    """Read out the CFLAGS from u_flags.yml for a specific platform & target"""
    cflags = ""
    file_path = os.path.join(cfg_dir, U_FLAG_YML)
    u_flag_data = None

    # Load u_flags.yml
    if os.path.exists(file_path):
        with open(file_path, 'r', encoding='utf8') as file:
            u_flag_data = yaml.safe_load(os.path.expandvars(file.read()))

    # If things are missing in the structure - add them
    if u_flag_data is None:
        u_flag_data = {}
    if not platform in u_flag_data or u_flag_data[platform] is None:
        u_flag_data[platform] = {}
    if not target in u_flag_data[platform]:
        u_flag_data[platform][target] = { "u_flags": [ None ], "md5": "" }
    if not "md5" in u_flag_data[platform][target]:
        u_flag_data[platform][target]["md5"] = ""
    if not "u_flags" in u_flag_data[platform][target]:
        u_flag_data[platform][target]["u_flags"] = [ None ]
    cur_hash = u_flag_data[platform][target]["md5"]
    u_flag_list = u_flag_data[platform][target]["u_flags"]

    # Calculate new hash based on the u_flags list to see if anything has changed
    data = json.dumps(u_flag_list, sort_keys=True, ensure_ascii=True)
    new_hash = md5(data.encode('ascii')).hexdigest()
    if store_new_hash:
        u_flag_data[platform][target]["md5"] = new_hash

    # Write back the .yml file
    with open(file_path, 'w', encoding='utf8') as file:
        yaml.dump(u_flag_data, file, default_flow_style=False)

    # Add "-D" to each entry
    if u_flag_list is not None and u_flag_list[0] is not None:
        entries = [f"-D{line.strip()}" for line in u_flag_list]
        cflags = " ".join(entries)
    return {
        'modified' : cur_hash != new_hash,
        'cflags' : cflags
    }

def u_flags_to_cflags(u_flags):
    """Helper function to convert the following string:
         "U_VARIABLE1=1 U_VARIABLE2=foo"
       to:
         "-DU_VARIABLE1=1 -DU_VARIABLE2=foo"
    """
    if isinstance(u_flags, str):
        u_flag_list = u_flags.split()
    else:
        u_flag_list = u_flags
    cflags = [f"-D{u_flag}" for u_flag in u_flag_list]

    return " ".join(cflags)
