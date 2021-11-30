import os, sys
import shutil
from invoke import task
from . import utils, unity

ESP_IDF_URL="https://github.com/espressif/esp-idf"

@task(
    pre=[
        unity.check_installation
    ],
)
def check_installation(ctx):
    """Check ESP-IDF SDK installation"""
    cfg = ctx.config.esp_idf
    ctx.esp_idf_env = [
        f"ARM_GCC_TOOLCHAIN_PATH={ctx.config.arm_embedded.install_dir}/bin",
        f"UNITY_PATH={ctx.config.unity.install_dir}",
        f"STM32CUBE_FW_PATH={cfg.install_dir}",
    ]
    sys.stdout.write("Checking ESP-IDF SDK: ")
    if not os.path.exists(cfg.install_dir):
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download ESP-IDF SDK?"):
            ctx.run(f"git clone --branch {cfg.version} --recursive --depth 1 {ESP_IDF_URL} {cfg.install_dir}")
        else:
            exit()
    else:
        print("Found ({})".format(cfg.install_dir))

    if not os.path.exists(cfg.idf_tools_install_dir):
        print("Not found ({})".format(cfg.idf_tools_install_dir))
        if utils.question("Do you want to install ESP toolchain?"):
            ctx.run(f"export IDF_TOOLS_PATH={cfg.idf_tools_install_dir} && {cfg.install_dir}/install.sh")
        else:
            exit()
    else:
        print("Found ({})".format(cfg.idf_tools_install_dir))

    ctx.esp_idf_dir = cfg.install_dir
    ctx.esp_idf_tools_dir = cfg.idf_tools_install_dir
    ctx.esp_idf_pre_command = f"export IDF_TOOLS_PATH={cfg.idf_tools_install_dir} && source {cfg.install_dir}/export.sh &&"

@task(
    pre=[check_installation],
    help={
        "cmake_dir": "Makefile project directory to build",
        "target_name": "A target name (build sub folder, default: ../port/platform/esp-idf/mcu/esp32/runner)",
        "build_dir": "Output build directory (default: {})".format(os.path.join("_build","esp_idf")),
    }
)
def build(ctx, cmake_dir="../port/platform/esp-idf/mcu/esp32/runner", target_name="esp32",
          build_dir=os.path.join("_build","esp_idf")):
    """Build a ESP-IDF SDK based application"""
    # Read U_FLAGS from esp_idf.u_flags
    u_flags = utils.get_u_flags(ctx.config.cfg_dir, "esp_idf", target_name)
    # If the flags has been modified we trigger a rebuild
    if u_flags['modified']:
        clean(ctx, target_name, build_dir)

    cmake_dir = os.path.abspath(cmake_dir)
    build_dir = os.path.abspath(os.path.join(build_dir, target_name))
    os.makedirs(build_dir, exist_ok=True)
    ctx.run(f'{ctx.esp_idf_pre_command} U_FLAGS="{u_flags["u_flags"]}" idf.py -C {cmake_dir} -B {build_dir} '\
            f'-DSDKCONFIG:STRING={cmake_dir}/sdkconfig -DTEST_COMPONENTS=ubxlib_runner ' \
            f'build')

@task(
    help={
        "target_name": "A target name (build sub folder)",
        "build_dir": "Output bild directory (default: {})".format(os.path.join("_build","esp_idf")),
    }
)
def clean(ctx, target_name, build_dir=os.path.join("_build","esp_idf")):
    """Remove all files for a ESP-IDF SDK build"""
    build_dir = os.path.abspath(os.path.join(build_dir, target_name))
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
