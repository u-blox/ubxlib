import os
import shutil
from invoke import task
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package

STM32CUBE_F4_URL="https://github.com/STMicroelectronics/STM32CubeF4.git"

DEFAULT_MAKEFILE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/stm32cube/mcu/stm32f4/runner"
DEFAULT_OUTPUT_NAME = "runner_stm32f4"
DEFAULT_BUILD_DIR = os.path.join("_build","stm32cubef4")
DEFAULT_JOB_COUNT = 8
DEFAULT_FLASH_FILE = f"runner.elf"

@task()
def check_installation(ctx):
    """Check STM32CubeF4 SDK installation"""
    # Load required packages
    pkgs = u_package.load(ctx, [
        "make", "unity", "arm_embedded_gcc", "stm32cubef4", "openocd"
    ])
    stm32cubef4_pkg = pkgs["stm32cubef4"]
    ae_gcc_pkg = pkgs["arm_embedded_gcc"]
    unity_pkg = pkgs["unity"]

    ctx.stm32cubef4_env = [
        f"ARM_GCC_TOOLCHAIN_PATH={ae_gcc_pkg.get_install_path()}/bin",
        f"UNITY_PATH={unity_pkg.get_install_path()}",
        f"STM32CUBE_FW_PATH={stm32cubef4_pkg.get_install_path()}",
    ]
    ctx.stm32cubef4_dir = stm32cubef4_pkg.get_install_path()

@task(
    pre=[check_installation],
    help={
        "makefile_dir": f"Makefile project directory to build (default: {DEFAULT_MAKEFILE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
        "jobs": f"The number of Makefile jobs (default: {DEFAULT_JOB_COUNT})"
    }
)
def build(ctx, makefile_dir=DEFAULT_MAKEFILE_DIR, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, jobs=DEFAULT_JOB_COUNT, u_flags=None):
    """Build a STM32CubeF4 SDK based application"""

    # Handle u_flags
    if u_flags:
        cflags = u_flags_to_cflags(u_flags)
    else:
        # Read U_FLAGS from stm32cubef4.u_flags
        u_flags = get_cflags_from_u_flags_yml(ctx.config.cfg_dir, "stm32cubef4", output_name)
        # If the flags has been modified we trigger a rebuild
        if u_flags['modified']:
            clean(ctx, output_name, build_dir)
        cflags = u_flags["cflags"]

    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    os.makedirs(build_dir, exist_ok=True)
    with ctx.prefix(u_utils.change_dir_prefix(makefile_dir)):
        # OUTPUT_DIRECTORY is very picky in Windows.
        # Seems it must be a relative path and `\` directory separators must NOT be used.
        build_dir = os.path.relpath(build_dir, makefile_dir).replace("\\", "/")
        ctx.run(f'make -j{jobs} UBXLIB_PATH={ctx.config.root_dir} OUTPUT_DIRECTORY={build_dir} '\
                f'CFLAGS="{cflags}" {" ".join(ctx.stm32cubef4_env)}')

@task(
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files for a STM32CubeF4 build"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)


@task(
    pre=[check_installation],
    help={
        "file": f"The file to flash (default: {DEFAULT_FLASH_FILE}",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "debugger_serial": "The debugger serial number (optional)",
    }
)
def flash(ctx, file=DEFAULT_FLASH_FILE, debugger_serial="",
          output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Flash a nRF5 SDK based application"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    cmd = f'openocd -f {u_utils.OPENOCD_CFG_DIR}/stm32f4.cfg'
    if debugger_serial != "":
        cmd += f' -c hla_serial {debugger_serial}'
    cmd += f' -c "program {build_dir}/{file} reset" -c exit'
    ctx.run(cmd)

