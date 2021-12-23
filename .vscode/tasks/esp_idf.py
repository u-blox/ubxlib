import os
import shutil
from invoke import task
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package

ESP_IDF_URL="https://github.com/espressif/esp-idf"

DEFAULT_CMAKE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/esp-idf/mcu/esp32/runner"
DEFAULT_OUTPUT_NAME = "runner_esp32"
DEFAULT_BUILD_DIR = os.path.join("_build","esp_idf")

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
    """Build a ESP-IDF SDK based application"""
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
    """Remove all files for a ESP-IDF SDK build"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
