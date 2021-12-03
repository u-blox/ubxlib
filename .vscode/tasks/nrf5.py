import os, sys
import shutil
from invoke import task
from . import utils, arm_embedded, unity, make

DEFAULT_MAKEFILE_DIR = f"{utils.UBXLIB_DIR}/port/platform/nrf5sdk/mcu/nrf52/gcc/runner"
DEFAULT_OUTPUT_NAME = "runner_ubx_evkninab3_nrf52840"
DEFAULT_BUILD_DIR = os.path.join("_build","nrf5")
DEFAULT_JOB_COUNT = 8

@task(
    pre=[
        arm_embedded.check_installation,
        unity.check_installation,
        make.check_installation
    ],
)
def check_installation(ctx):
    """Check nRF5 SDK installation"""
    cfg = ctx.config.nrf5
    ctx.nrf5_env = [
        f"GNU_INSTALL_ROOT={ctx.config.arm_embedded.install_dir}/bin/",
        f"GNU_PREFIX=arm-none-eabi",
        f"GNU_VERSION={ctx.config.arm_embedded.version}",
        f"UNITY_PATH={ctx.config.unity.install_dir}",
    ]
    sys.stdout.write("Checking nRF5 SDK: ")
    if not os.path.exists(cfg.install_dir):
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download nRF5 SDK v{}?".format(cfg.version)):
            utils.download_and_extract(cfg.url, cfg.install_dir, skip_first_sub_dir=True)
        else:
            exit()
    else:
        print("Found {} ({})".format(cfg.version, cfg.install_dir))
    ctx.nrf5_dir = cfg.install_dir

@task(
    pre=[check_installation],
    help={
        "makefile_dir": f"Makefile project directory to build (default: {DEFAULT_MAKEFILE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "jobs": f"The number of Makefile jobs (default: {DEFAULT_JOB_COUNT})",
    }
)
def build(ctx, makefile_dir=DEFAULT_MAKEFILE_DIR, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, jobs=DEFAULT_JOB_COUNT):
    """Build a nRF5 SDK based application"""
    # Read U_FLAGS from nrfconnect.u_flags
    u_flags = utils.get_u_flags(ctx.config.cfg_dir, "nrf5", output_name)
    # If the flags has been modified we trigger a rebuild
    if u_flags['modified']:
        clean(ctx, output_name, build_dir)

    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    os.makedirs(build_dir, exist_ok=True)
    with ctx.cd(makefile_dir):
        # OUTPUT_DIRECTORY is very picky in Windows.
        # Seems it must be a relative path and `\` directory separators must NOT be used.
        build_dir = os.path.relpath(build_dir, makefile_dir).replace("\\", "/")
        ctx.run(f'make -j{DEFAULT_JOB_COUNT} UBXLIB_PATH={ctx.config.root_dir} OUTPUT_DIRECTORY={build_dir} '\
                f'NRF5_PATH={ctx.nrf5_dir} CFLAGS="{u_flags["u_flags"]}" {" ".join(ctx.nrf5_env)}')

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
