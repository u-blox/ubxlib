import os
import shutil
from invoke import task
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package

DEFAULT_MAKEFILE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/nrf5sdk/mcu/nrf52/gcc/runner"
DEFAULT_OUTPUT_NAME = "runner_ubx_evkninab3_nrf52840"
DEFAULT_BUILD_DIR = os.path.join("_build","nrf5")
DEFAULT_JOB_COUNT = 8

@task()
def check_installation(ctx):
    """Check nRF5 SDK installation"""

    # Load required packages
    pkgs = u_package.load(ctx, [
        "make", "unity", "arm_embedded_gcc", "nrf5sdk"
    ])
    nrf5sdk_pkg = pkgs["nrf5sdk"]
    ae_gcc_pkg = pkgs["arm_embedded_gcc"]
    unity_pkg = pkgs["unity"]

    ctx.nrf5_env = [
        f"GNU_INSTALL_ROOT={ae_gcc_pkg.get_install_path()}/bin/",
        f"GNU_PREFIX=arm-none-eabi",
        f"GNU_VERSION={ae_gcc_pkg.get_version()}",
        f"UNITY_PATH={unity_pkg.get_install_path()}",
    ]
    ctx.nrf5_dir = nrf5sdk_pkg.get_install_path()

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
          build_dir=DEFAULT_BUILD_DIR, u_flags=None, jobs=DEFAULT_JOB_COUNT):
    """Build a nRF5 SDK based application"""

    # Handle u_flags
    if u_flags:
        cflags = u_flags_to_cflags(u_flags)
    else:
        # Read U_FLAGS from nrf5.u_flags
        u_flags = get_cflags_from_u_flags_yml(ctx.config.cfg_dir, "nrf5", output_name)
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
                f'NRF5_PATH={ctx.nrf5_dir} CFLAGS="{cflags}" {" ".join(ctx.nrf5_env)}')

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
