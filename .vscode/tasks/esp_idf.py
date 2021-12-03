import os, sys
import shutil
from invoke import task
from . import utils, unity

ESP_IDF_URL="https://github.com/espressif/esp-idf"

DEFAULT_CMAKE_DIR = f"{utils.UBXLIB_DIR}/port/platform/esp-idf/mcu/esp32/runner"
DEFAULT_OUTPUT_NAME = "runner_esp32"
DEFAULT_BUILD_DIR = os.path.join("_build","esp_idf")

@task()
def check_installation(ctx):
    """Check ESP-IDF SDK installation"""
    cfg = ctx.config.esp_idf

    # Check ESP-IDF SDK
    sys.stdout.write("Checking ESP-IDF SDK: ")
    if not os.path.exists(cfg.install_dir):
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download ESP-IDF SDK?"):
            ctx.run(f"git clone --branch {cfg.version} --recursive --depth 1 {ESP_IDF_URL} {cfg.install_dir}")
        else:
            exit()
    else:
        print("Found ({})".format(cfg.install_dir))

    # Check ESP-IDF toolchain
    ctx.config.run.env["IDF_TOOLS_PATH"] = cfg.idf_tools_install_dir
    if not os.path.exists(cfg.idf_tools_install_dir):
        print("Not found ({})".format(cfg.idf_tools_install_dir))
        if utils.question("Do you want to install ESP toolchain?"):
            if ctx.config.is_linux:
                ctx.run(f"{cfg.install_dir}/install.sh")
            else:
                ctx.run(f"{cfg.install_dir}/install.bat")
        else:
            exit()
    else:
        print("Found ({})".format(cfg.idf_tools_install_dir))

    ctx.esp_idf_dir = cfg.install_dir
    ctx.esp_idf_tools_dir = cfg.idf_tools_install_dir
    if ctx.config.is_linux:
        ctx.esp_idf_pre_command = f"source {cfg.install_dir}/export.sh &&"
    else:
        ctx.esp_idf_pre_command = f"call {cfg.install_dir}/export.bat &"

@task(
    pre=[check_installation],
    help={
        "cmake_dir": f"CMake project directory to build (default: {DEFAULT_CMAKE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def build(ctx, cmake_dir=DEFAULT_CMAKE_DIR, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR):
    """Build a ESP-IDF SDK based application"""
    # Read U_FLAGS from esp_idf.u_flags
    u_flags = utils.get_u_flags(ctx.config.cfg_dir, "esp_idf", output_name)
    # If the flags has been modified we trigger a rebuild
    if u_flags['modified']:
        clean(ctx, output_name, build_dir)
    ctx.config.run.env["U_FLAGS"] = u_flags["u_flags"]

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
