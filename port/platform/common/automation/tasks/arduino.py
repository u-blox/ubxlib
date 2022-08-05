import os
import shutil
import sys
from glob import glob
from invoke import task
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package
from scripts.u_log_readers import UUartReader

ARDUINO_DIR = f"{u_utils.PLATFORM_DIR}/arduino"

# The name to use for the libraries sub-directory
LIBRARIES_SUB_DIR = "libraries"

# The name to use for the main ubxlib library
LIBRARY_NAME = "ubxlib"

# The thing to stick on the end of things when we mean the main library
LIBRARY_NAME_LIB_POSTFIX = ""

# The thing to stick on the end of things for the test library
LIBRARY_NAME_TEST_POSTFIX = "_test"

DEFAULT_SKETCH_PATH = f"{u_utils.PLATFORM_DIR}/arduino/app/app.ino"
DEFAULT_OUTPUT_NAME = "runner_nina_w10/app"
DEFAULT_BUILD_DIR = "_build/arduino"
DEFAULT_LIBRARIES_DIR = f"{DEFAULT_BUILD_DIR}/{LIBRARIES_SUB_DIR}"
DEFAULT_FLASH_FILE = f"app.ino.bin"
DEFAULT_TOOLCHAIN = "esp-idf"
DEFAULT_BOARD_NAME = "esp32:esp32:nina_w10"

BOARD_URLS = [
    "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
]

def check_board(ctx, board):
    '''Install board tools'''
    print("Check installation...")
    ctx.run(f"{ctx.arduino_cli} config init --overwrite --dest-dir .")

    # Add board URLs
    cmd = f"{ctx.arduino_cli} config add board_manager.additional_urls {' '.join(BOARD_URLS)}"
    ctx.run(cmd)
    ctx.run(f"{ctx.arduino_cli} core update-index")

    # Install boards
    cmd = f"{ctx.arduino_cli} core install "
    # I'm kind of guessing the syntax here, I think the
    # install command needs the first two parts of the
    # FQBN, with the actual board name on the end
    # missed off
    core = board.split(":")
    cmd += f"{core[0]}:{core[1]}"
    ctx.run(cmd)

# Create the ubxlib library for Arduino
def create_library(ctx, toolchain, library_path, postfix):
    '''Create the ubxlib library'''
    print("creating library...")
    ctx.run(f"python {ARDUINO_DIR}/u_arduino{postfix}.py -p {toolchain} " +
            f"-u {u_utils.UBXLIB_DIR} -o {library_path}{postfix} " +
            f"{ARDUINO_DIR}/source{postfix}.txt {ARDUINO_DIR}/include{postfix}.txt")


@task()
def check_installation(ctx):
    """Check Arduino installation"""
    # Load required packages
    pkgs = u_package.load(ctx, ["arduino-cli"])
    ctx.arduino_cli = pkgs["arduino-cli"].get_install_path() + '/arduino-cli'
    if not u_utils.is_linux():
        ctx.arduino_cli += ".exe"

    # First check that the Arduino CLI is there
    ctx.run(f"{ctx.arduino_cli} version")


@task(
    pre=[check_installation],
    help={
        "sketch_path": f"The complete Arduino sketch path (default: {DEFAULT_SKETCH_PATH})",
        "libraries_dir": f"The Arduino libraries dir (default: {DEFAULT_LIBRARIES_DIR})",
        "board": f"The Arduino board (default: {DEFAULT_BOARD_NAME})",
        "toolchain": f"The toolchain (default: {DEFAULT_TOOLCHAIN})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
    }
)
def build(ctx, sketch_path=DEFAULT_SKETCH_PATH,
          libraries_dir=DEFAULT_LIBRARIES_DIR,
          board=DEFAULT_BOARD_NAME,
          toolchain=DEFAULT_TOOLCHAIN,
          output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, u_flags=None):
    """Build an Arduino based application"""
    check_board(ctx, board)
    mcu = board.split(":")[0]
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    library_dir = f"{libraries_dir}/{LIBRARY_NAME}"
    # Create the ubxlib Arduino library
    create_library(ctx, toolchain, library_dir, LIBRARY_NAME_LIB_POSTFIX)
    # Create the ubxlib Arduino test library
    create_library(ctx, toolchain, library_dir, LIBRARY_NAME_TEST_POSTFIX)

    # Make a list of the sketches to build
    sketch_paths = [sketch_path]
    for fn in glob(f"{library_dir}/examples/**/*.ino", recursive=True):
        sketch_paths.append(os.path.abspath(fn))
    print(f"{len(sketch_paths)} thing(s) to build.")

    # Handle u_flags
    if u_flags:
        cflags = u_flags_to_cflags(u_flags)
    else:
        # Read U_FLAGS from arduino.u_flags
        u_flags = get_cflags_from_u_flags_yml(ctx.config.vscode_dir, "arduino", output_name)
        # If the flags has been modified we trigger a rebuild
        if u_flags['modified']:
            clean(ctx, output_name, build_dir)
        cflags = u_flags["cflags"]

    # Note: we set build partitions to "minimal" for compatibility with the
    # smaller [2 Mbyte] flash size on NINA-W102
    for sketch in sketch_paths:
        sketch_build_dir = os.path.join(build_dir, os.path.basename(os.path.split(sketch)[0]))
        print(f"Building {sketch} in {sketch_build_dir}...")
        cmd = f"{ctx.arduino_cli} compile --libraries {libraries_dir} --fqbn {board} " \
            f"--build-path {sketch_build_dir} --build-cache-path {sketch_build_dir} " \
            f"--build-property \"compiler.c.extra_flags={cflags}\" " \
            f"--build-property \"compiler.cpp.extra_flags={cflags}\" " \
            f"--build-property \"compiler.warning_flags=-Wall -Werror -Wno-missing-field-initializers -Wno-format\" " \
            f"--build-property build.partitions=minimal"
        ctx.run(f"{cmd} {sketch}")

    # If that was succesful, copy the ".a" files to the correct
    # locations under each library/mcu
    for fn in glob(f"{build_dir}/{LIBRARIES_SUB_DIR}/**/*.a", recursive=True):
        library_name = os.path.basename(os.path.split(fn)[0])
        destination_dir = f"{libraries_dir}/{library_name}/src/{mcu.lower()}"
        if not os.path.isdir(destination_dir):
            os.makedirs(destination_dir)
        shutil.copy2(fn, destination_dir)

@task(
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files for a nRF5 SDK build"""
    build_dir = os.path.join(build_dir, output_name)
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

@task(
    pre=[check_installation],
    help={
        "serial_port": "The serial port connected to the device",
        "board": f"The Arduino board (default: {DEFAULT_BOARD_NAME})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def flash(ctx, serial_port, board=DEFAULT_BOARD_NAME,
          output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Flash an Arduino based application"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    bin_file = f"{build_dir}/{os.path.basename(build_dir)}.ino.bin"
    check_board(ctx, board)
    ctx.run(f"{ctx.arduino_cli} upload -p {serial_port} --fqbn {board} -v --input-file {bin_file} {build_dir}")

@task(
    pre=[check_installation],
)
def log(ctx, serial_port, baudrate=115200, rts_state=None, dtr_state=None):
    """Open a log terminal"""
    with UUartReader(port=serial_port, baudrate=baudrate, rts_state=rts_state, dtr_state=dtr_state) as serial:
        while True:
            data = serial.read()
            if data:
                sys.stdout.write(data.decode(sys.stdout.encoding, "backslashreplace"))
                sys.stdout.flush()

