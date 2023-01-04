import sys
import requests
import tempfile
import io
import os
import time
import shutil
from io import BytesIO
from zipfile import ZipFile
from tarfile import TarFile
from glob import glob
import platform

def is_automation():
    """Returns True if running automated (detected by checking if there is a TTY)"""
    return not sys.stdin.isatty()

def is_linux():
    '''Returns True when system is Linux'''
    return platform.system() == 'Linux'

def is_arm():
    '''Returns True when we're on a 64-bit ARM (e.g. a Raspberry Pi 2.1 and above)'''
    return platform.machine() == 'aarch64'

def question(text):
    """ Prompt user for yes/no question
    returns True if user enter yes otherwise False
    """
    yes = {'yes','y', 'ye', ''}
    no = {'no','n'}
    if not sys.stdin.isatty():
        # Auto yes if stdin is not available
        return True
    while True:
        sys.stdout.write(f"{text} [y/n]: ")
        choice = input().lower()
        if choice in yes:
            return True
        elif choice in no:
            return False

def download(url, file):
    """Download a file from URL"""
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
    return content_length


def extract_tar(file, tar_mode, dest_dir, skip_first_sub_dir):
    """Extract a .tar file
    file:               The File object to extract
    tar_mode:           The tar mode - see https://docs.python.org/3/library/tarfile.html
    dest_dir:           Destination directory
    skip_first_sub_dir: When this is set to true the first subdir in the tar file
                        will be skipped
    """
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
                sys.stdout.write("\rExtracting  [{}{}]".format("#" * progress, " " * (20-progress)))
                self.next_time += 0.5
            elif self.tell() + size >= self.size:
                sys.stdout.write("\rExtracting  [{}]".format("#" * 20))

            return io.BufferedReader.read(self, size)

    parent_dir = os.path.realpath(os.path.join(dest_dir, '..'))
    os.makedirs(parent_dir, exist_ok=True)

    archive = None
    temp_dir = None
    try:
        archive = TarFile.open(fileobj=ProgressWrapper(file), mode=tar_mode, encoding="utf8")
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
    """Extract a .zip file
    file:               The File object to extract
    dest_dir:           Destination directory
    skip_first_sub_dir: When this is set to true the first subdir in the zip file
                        will be skipped
    """
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
                sys.stdout.write("\rExtracting  [{}{}]".format("#" * progress, " " * (20-progress)))
                next_time += 0.5
        print("\nDone")


def download_and_extract(url, dest_dir, skip_first_sub_dir=False):
    """Download and extract an archive
    url:                URL to download
    dest_dir:           Destination directory
    skip_first_sub_dir: When this is set to true the first subdir in the zip file
                        will be skipped
    """
    tar_mode = None
    zip = False
    if url.lower().endswith(".tar.bz2"):
        tar_mode = "r:bz2"
    elif url.lower().endswith(".tar.xz"):
        tar_mode = "r:xz"
    elif url.lower().endswith(".tar.gz"):
        tar_mode = "r:gz"
    elif url.lower().endswith(".zip"):
        zip = True

    with tempfile.TemporaryFile() as file:
        length = download(url, file)
        file.seek(0)
        print(f"Downloaded {length} byte(s)")
        # Now when the file has been downloaded we start the extraction
        if tar_mode != None:
            extract_tar(file, tar_mode, dest_dir, skip_first_sub_dir)
        elif zip:
            extract_zip(file, dest_dir, skip_first_sub_dir)
        else:
            os.makedirs(dest_dir, exist_ok=True)
            dest_file_path = os.path.join(dest_dir, os.path.basename(url))
            with open(dest_file_path, 'wb') as dest_file:
                dest_file.write(file.read())


def add_dir_to_path(config, dir):
    """Add a directory to PATH in a PyInvoke config"""
    if "PATH" in config.run.env:
        path = dir + os.pathsep + config.run.env
    else:
        path = dir + os.pathsep + os.environ["PATH"]
    config.run.env["PATH"] = path


def change_dir_prefix(path):
    """PyInvoke ctx.cd() doesn't handle a windows case when changing directory
    to a different drive. For this reason this function can be used to generate
    a command prefix similar to what ctx.cd() does, but also handling change of
    drive.

    So instead of:
        with ctx.cd(some_path):
    use this instead:
        with ctx.prefix(u_utils.change_dir_prefix(some_path)):
    """
    prefix = f"cd {path}"
    if not is_linux():
        drive = os.path.splitdrive(path)
        prefix += f" && {drive[0]}"
    return prefix
