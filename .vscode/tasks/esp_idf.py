import os
import sys
import shutil
import json
from invoke import task
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package
from scripts.u_log_readers import UUartReader

ESP_IDF_URL="https://github.com/espressif/esp-idf"

DEFAULT_CMAKE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/esp-idf/mcu/esp32/runner"
DEFAULT_OUTPUT_NAME = "runner_esp32"
DEFAULT_BUILD_DIR = os.path.join("_build","esp_idf")

# In the test automation we build on one computer and flash on
# different one. Since "idf.py flash" will automatically first
# check that everything is built this becomes a problem.
# To solve this we parse the flasher_args.json like idf.py
# and call esptool.py manually.
def _get_idf_flash_command(idf_path, build_dir, port, baudrate):
    with open(os.path.join(build_dir, 'flasher_args.json')) as f:
        flasher_args = json.load(f)

    def flasher_path(f):
        return os.path.join(build_dir, f)

    cmd = 'python %s -p %s -b %s --before %s --after %s --chip %s %s write_flash ' % (
            '%s/components/esptool_py/esptool/esptool.py' % idf_path,
            port,
            baudrate,
            flasher_args['extra_esptool_args']['before'],
            flasher_args['extra_esptool_args']['after'],
            flasher_args['extra_esptool_args']['chip'],
            '--no-stub' if not flasher_args['extra_esptool_args']['stub'] else ''
        )

    cmd += ' '.join(flasher_args['write_flash_args']) + ' '
    flash_items = sorted(
        ((o, f) for (o, f) in flasher_args['flash_files'].items() if len(o) > 0),
        key=lambda x: int(x[0], 0),
    )
    for o, f in flash_items:
        cmd += o + ' ' + flasher_path(f) + ' '

    return cmd

@task()
def check_installation(ctx):
    """Check ESP-IDF SDK installation"""
    # Load required packages
    pkgs = u_package.load(ctx, ["esp_idf"])
    esp_idf_pkg = pkgs["esp_idf"]

    ctx.esp_idf_dir = esp_idf_pkg.get_install_path()
    ctx.esp_idf_tools_dir = esp_idf_pkg.get_idf_tools_path()
    if u_utils.is_linux():
        ctx.esp_idf_pre_command = f"source {ctx.esp_idf_dir}/export.sh &&"
    else:
        ctx.esp_idf_pre_command = f"call {ctx.esp_idf_dir}/export.bat &"

@task(
    pre=[check_installation],
    help={
        "cmake_dir": f"CMake project directory to build (default: {DEFAULT_CMAKE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)"
    }
)
def build(ctx, cmake_dir=DEFAULT_CMAKE_DIR, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, u_flags=None):
    """Build an ESP-IDF SDK based application"""
    # Handle u_flags
    if u_flags:
        ctx.config.run.env["U_FLAGS"] = u_flags_to_cflags(u_flags)
    else:
        # Read U_FLAGS from esp_idf.u_flags
        u_flags = get_cflags_from_u_flags_yml(ctx.config.cfg_dir, "esp_idf", output_name)
        # If the flags has been modified we trigger a rebuild
        if u_flags['modified']:
            clean(ctx, output_name, build_dir)
        ctx.config.run.env["U_FLAGS"] = u_flags["cflags"]

    cmake_dir = os.path.abspath(cmake_dir)
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    os.makedirs(build_dir, exist_ok=True)

    # TODO: Move -DTEST_COMPONENTS=ubxlib_runner out from this file
    ctx.run(f'{ctx.esp_idf_pre_command} idf.py -C {cmake_dir} -B {build_dir} '\
            f'-DSDKCONFIG:STRING={cmake_dir}/sdkconfig -DTEST_COMPONENTS=ubxlib_runner ' \
            f'build')

@task(
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files for an ESP-IDF SDK build"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

@task(
    pre=[check_installation],
    help={
        "serial_port": "The serial port connected to ESP32 device",
        "cmake_dir": f"CMake project directory to build (default: {DEFAULT_CMAKE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "use_flasher_json": f"When set to true esptool.py will be manually called based on" \
                             "generated flasher_args.json. This allows flash without rebuild."
    }
)
def flash(ctx, serial_port, cmake_dir=DEFAULT_CMAKE_DIR, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, use_flasher_json=False):
    """Flash an ESP-IDF SDK based application"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))

    if use_flasher_json:
        cmd = _get_idf_flash_command(ctx.esp_idf_dir, build_dir, serial_port, 460800)
        ctx.run(f'{ctx.esp_idf_pre_command} {cmd}')
    else:
        ctx.run(f'{ctx.esp_idf_pre_command} idf.py -C {cmake_dir} -B {build_dir} '\
                f'-p {serial_port} flash')

@task(
    pre=[check_installation],
)
def terminal(ctx):
    """Open an ESP-IDF SDK terminal"""
    ctx.run(f'{ctx.esp_idf_pre_command} {ctx.config.run.shell}', pty=True)


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

