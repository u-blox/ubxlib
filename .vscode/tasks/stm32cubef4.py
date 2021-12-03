import os, sys
import shutil
from invoke import task
from . import utils, arm_embedded, unity, make

STM32CUBE_F4_URL="https://github.com/STMicroelectronics/STM32CubeF4.git"

DEFAULT_MAKEFILE_DIR = f"{utils.UBXLIB_DIR}/port/platform/stm32cube/mcu/stm32f4/runner"
DEFAULT_OUTPUT_NAME = "runner_stm32f4"
DEFAULT_BUILD_DIR = os.path.join("_build","stm32cubef4")
DEFAULT_JOB_COUNT = 8

@task(
    pre=[
        arm_embedded.check_installation,
        unity.check_installation,
        make.check_installation
    ],
)
def check_installation(ctx):
    """Check STM32CubeF4 SDK installation"""
    cfg = ctx.config.stm32cubef4
    ctx.stm32cubef4_env = [
        f"ARM_GCC_TOOLCHAIN_PATH={ctx.config.arm_embedded.install_dir}/bin",
        f"UNITY_PATH={ctx.config.unity.install_dir}",
        f"STM32CUBE_FW_PATH={cfg.install_dir}",
    ]
    sys.stdout.write("Checking STM32CubeF4 SDK: ")
    if not os.path.exists(cfg.install_dir):
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download STM32CubeF4?"):
            ctx.run(f"git clone --branch {cfg.version} --depth 1 {STM32CUBE_F4_URL} {cfg.install_dir}")
        else:
            exit()
    else:
        print("Found ({})".format(cfg.install_dir))

    ctx.stm32cubef4_dir = cfg.install_dir

@task(
    pre=[check_installation],
    help={
        "makefile_dir": f"Makefile project directory to build (default: {DEFAULT_MAKEFILE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "jobs": f"The number of Makefile jobs (default: {DEFAULT_JOB_COUNT})"
    }
)
def build(ctx, makefile_dir=DEFAULT_MAKEFILE_DIR, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, jobs=DEFAULT_JOB_COUNT):
    """Build a STM32CubeF4 SDK based application"""
    # Read U_FLAGS from stm32cubef4.u_flags
    u_flags = utils.get_u_flags(ctx.config.cfg_dir, "stm32cubef4", output_name)
    # If the flags has been modified we trigger a rebuild
    if u_flags['modified']:
        clean(ctx, output_name, build_dir)

    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    os.makedirs(build_dir, exist_ok=True)
    with ctx.cd(makefile_dir):
        # OUTPUT_DIRECTORY is very picky in Windows.
        # Seems it must be a relative path and `\` directory separators must NOT be used.
        build_dir = os.path.relpath(build_dir, makefile_dir).replace("\\", "/")
        ctx.run(f'make -j{jobs} UBXLIB_PATH={ctx.config.root_dir} OUTPUT_DIRECTORY={build_dir} '\
                f'CFLAGS="{u_flags["u_flags"]}" {" ".join(ctx.stm32cubef4_env)}')

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
