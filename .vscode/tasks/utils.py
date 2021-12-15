import sys
import requests
import tempfile
import io
import os
import time
import yaml
import json
import shutil
from hashlib import md5
from io import BytesIO
from zipfile import ZipFile
from tarfile import TarFile
from glob import glob

U_FLAG_YML = "u_flags.yml"

UBXLIB_DIR = os.path.abspath(os.path.dirname(__file__) + "/../..")

def question(text):
    yes = {'yes','y', 'ye', ''}
    no = {'no','n'}
    if not sys.stdin.isatty():
        # Auto yes if stdin is not available
        return True
    while True:
        sys.stdout.write("{} [y/n]: ".format(text))
        choice = input().lower()
        if choice in yes:
            return True
        elif choice in no:
            return False


def download(url, file):
    response = requests.get(url, stream=True)
    content_length = response.headers.get("content-length")

    if content_length:
        bytes_read = 0
        total_length = int(content_length)
        next_time = time.time()
        for data in response.iter_content(chunk_size=4096):
            bytes_read += len(data)
            file.write(data)
            if time.time() >= next_time or bytes_read == total_length:
                progress = int((bytes_read / total_length) * 20)
                sys.stdout.write("\rDownloading [{}{}]".format("#" * progress, " " * (20-progress)))
                next_time += 0.5
    else:
        file.write(response.content)
    print()



def extract_tar(file, tar_mode, dest_dir, skip_first_sub_dir):
    class ProgressWrapper(io.BufferedReader):
        def __init__(self, file, *args, **kwargs):
            io.BufferedReader.__init__(self, raw=file, *args, **kwargs)
            self.next_time = time.time()
            # Get the file size
            self.seek(0, os.SEEK_END)
            self.size = self.tell()
            self.seek(0)

        def read(self, size):
            if time.time() >= self.next_time:
                progress = int((self.tell() / self.size) * 20)
                sys.stdout.write("\rExtracting [{}{}]".format("#" * progress, " " * (20-progress)))
                self.next_time += 0.5
            elif self.tell() + size >= self.size:
                sys.stdout.write("\rExtracting [{}]".format("#" * 20))

            return io.BufferedReader.read(self, size)

    parent_dir = os.path.realpath(os.path.join(dest_dir, '..'))
    os.makedirs(parent_dir, exist_ok=True)

    archive = None
    temp_dir = None
    try:
        archive = TarFile.open(fileobj=ProgressWrapper(file), mode=tar_mode)
        if skip_first_sub_dir:
            # We can't use the same trick as for the zip file since then symbolic links
            # in the tar file will break. Instead we extract to a temporary directory
            # and then move the subdirectory to dest_dir
            temp_dir = tempfile.TemporaryDirectory()
            archive.extractall(temp_dir.name)
            subdirs = glob(f"{temp_dir.name}/*/")
            if len(subdirs) != 1:
                raise Exception("Unexpected subdirectory count - can't skip first sub directory")
            shutil.move(subdirs[0],dest_dir)
        else:
            archive.extractall(dest_dir)
    finally:
        if archive != None:
            archive.close()
        if temp_dir != None:
            temp_dir.cleanup()

    print("\nDone")


def extract_zip(file, dest_dir, skip_first_sub_dir):
    next_time = time.time()
    with ZipFile(BytesIO(file.read())) as archive:
        info_entries = archive.infolist()
        entry_count = len(info_entries)
        entry_index = 0
        parent_dir = os.path.realpath(os.path.join(dest_dir, '..'))
        os.makedirs(parent_dir, exist_ok=True)
        for info_entry in info_entries:
            entry_index += 1
            # Nothing to do for directories
            if info_entry.filename[-1] == '/':
                continue

            # If the first sub directory in the zip file should be skipped
            # we handle it here
            if skip_first_sub_dir:
                sep = info_entry.filename.split('/')
                if len(sep) > 1:
                    filename = '/'.join(sep[1:])
                info_entry.filename = filename

            # Extract the file
            archive.extract(info_entry, dest_dir)
            # Print progress bar
            if time.time() >= next_time or entry_index == entry_count:
                progress = int((entry_index / entry_count) * 20)
                sys.stdout.write("\rExtracting [{}{}]".format("#" * progress, " " * (20-progress)))
                next_time += 0.5
        print("\nDone")


def download_and_extract(url, dest_dir, skip_first_sub_dir=False):
    tar_mode = None
    if url.lower().endswith(".tar.bz2"):
        tar_mode = "r:bz2"
    if url.lower().endswith(".tar.xz"):
        tar_mode = "r:xz"
    with tempfile.TemporaryFile() as file:
        download(url, file)
        file.seek(0)

        # Now when the file has been downloaded we start the extraction
        if tar_mode != None:
            extract_tar(file, tar_mode, dest_dir, skip_first_sub_dir)
        else:
            extract_zip(file, dest_dir, skip_first_sub_dir)


def get_u_flags(cfg_dir, builder_name, target, store_new_hash=True):
    u_flags = ""
    file_path = os.path.join(cfg_dir, U_FLAG_YML)
    u_flag_data = None

    # Load u_flags.yml
    if os.path.exists(file_path):
        with open(file_path, 'r') as f:
            u_flag_data = yaml.safe_load(os.path.expandvars(f.read()))

    # If things are missing in the structure - add them
    if u_flag_data == None:
        u_flag_data = {}
    if not builder_name in u_flag_data or u_flag_data[builder_name] == None:
        u_flag_data[builder_name] = {}
    if not target in u_flag_data[builder_name]:
        u_flag_data[builder_name][target] = { "u_flags": [ None ], "md5": "" }
    if not "md5" in u_flag_data[builder_name][target]:
        u_flag_data[builder_name][target]["md5"] = ""
    if not "u_flags" in u_flag_data[builder_name][target]:
        u_flag_data[builder_name][target]["u_flags"] = [ None ]
    cur_hash = u_flag_data[builder_name][target]["md5"]
    u_flag_list = u_flag_data[builder_name][target]["u_flags"]

    # Calculate new hash based on the u_flags list to see if anything has changed
    data = json.dumps(u_flag_list, sort_keys=True, ensure_ascii=True)
    new_hash = md5(data.encode('ascii')).hexdigest()
    if store_new_hash:
        u_flag_data[builder_name][target]["md5"] = new_hash

    # Write back the .yml file
    with open(file_path, 'w') as file:
        yaml.dump(u_flag_data, file, default_flow_style=False)

    # Add "-D" to each entry
    if u_flag_list != None and u_flag_list[0] != None:
        entries = [f"-D{line.strip()}" for line in u_flag_list]
        u_flags = " ".join(entries)
    return {
        'modified' : cur_hash != new_hash,
        'u_flags' : u_flags
    }

def add_dir_to_path(config, dir):
    if "PATH" in config.run.env:
        path = dir + os.pathsep + config.run.env
    else:
        path = dir + os.pathsep + os.environ["PATH"]
    config.run.env["PATH"] = path