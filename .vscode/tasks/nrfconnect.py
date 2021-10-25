import os, sys
import json
import shutil
from urllib.request import urlopen
from invoke import task
from . import utils, arm_embedded, make

ZEPHYR_URL="https://github.com/zephyrproject-rtos/zephyr.git"
SDK_NRF_URL="https://github.com/nrfconnect/sdk-nrf"
NRF_TOOLCHAIN_BASE_URL="https://developer.nordicsemi.com/.pc-tools/toolchain"

def download_windows_toolchain(version, install_dir):
    with urlopen(f"{NRF_TOOLCHAIN_BASE_URL}/index.json") as url:
        toolchain_list = json.loads(url.read().decode())
        for entry in toolchain_list:
            if entry["version"] == version:
                toolchain_url = NRF_TOOLCHAIN_BASE_URL + "/" + entry['toolchains'][0]['name']
                break

    if not toolchain_url:
        print(f"Could not find NCS toolchain {version}")
        exit(1)

    utils.download_and_extract(toolchain_url, install_dir)


def check_windows_toolchain(ctx):
    # Check nRF connect toolchain
    cfg = ctx.config.nrfconnect
    toolchain_install_dir = f"{cfg.install_dir}/toolchain"

    sys.stdout.write("Checking nRF connect toolchain: ")
    if not os.path.exists(toolchain_install_dir):
        print("Not found ({})".format(toolchain_install_dir))
        if utils.question("Do you want to download nRF connect toolchain?"):
            download_windows_toolchain(cfg.version, toolchain_install_dir)
        else:
            exit(-1)
    else:
        print("Found ({})".format(toolchain_install_dir))
    ctx.ncs_toolchain_install_dir = toolchain_install_dir

@task(
    pre=[
        arm_embedded.check_installation,
        make.check_installation
    ]
)
def check_installation(ctx):
    """Check that the toolchain for nRF connect SDK is installed"""
    cfg = ctx.config.nrfconnect
    ctx.zephyr_env = {}
    ctx.zephyr_pre_command = ""

    if not ctx.config.is_linux:
        check_windows_toolchain(ctx)
        # The Zephyr related env variables will be setup by <toolchain>/cmd/env.cmd
        ctx.zephyr_pre_command = f"{ctx.ncs_toolchain_install_dir}/cmd/env.cmd & "
    else:
        ctx.config.run.env["ZEPHYR_BASE"] = f'{cfg.install_dir}/zephyr'
        ctx.config.run.env["ZEPHYR_TOOLCHAIN_VARIANT"] = 'gnuarmemb'
        ctx.config.run.env["GNUARMEMB_TOOLCHAIN_PATH"] = ctx.config.arm_embedded.install_dir

    # Check nRF connect SDK
    sys.stdout.write("Checking nRF connect SDK: ")
    if not os.path.exists(os.path.join(cfg.install_dir, ".west")):
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download nRF connect SDK?"):
            os.makedirs(cfg.install_dir, exist_ok=True)
            with ctx.cd(cfg.install_dir):
                ctx.run(f"{ctx.zephyr_pre_command}west init -m {SDK_NRF_URL} --mr {cfg.version}")
                ctx.run(f"{ctx.zephyr_pre_command}west update")
        else:
            exit(-1)
    else:
        print("Found {} ({})".format(cfg.version, cfg.install_dir))


@task(
    pre=[check_installation],
    help={
        "cmake_dir": "CMake project directory to build",
        "board_name": "Zephyr board name",
        "target_name": "A target name (build sub folder)",
        "build_dir": "Output bild directory (default: {})".format(os.path.join("_build","nrfconnect")),
    }
)
def build(ctx, cmake_dir, board_name, target_name, build_dir=os.path.join("_build","nrfconnect")):
    """Build a nRF connect SDK based application"""
    # Read U_FLAGS from nrfconnect.u_flags
    u_flags = utils.get_u_flags(ctx.config.cfg_dir, "nrfconnect", target_name)
    ctx.config.run.env['U_FLAGS'] = u_flags['u_flags']

    # If the flags has been modified we trigger a rebuild
    pristine = "always" if u_flags['modified'] else "auto"

    build_dir = os.path.join(build_dir, target_name)
    ctx.run(f'{ctx.zephyr_pre_command}west build -p {pristine} -b {board_name} {cmake_dir} --build-dir {build_dir}')

@task(
    pre=[check_installation],
    help={
        "target_name": "A target name (build sub folder)",
        "build_dir": "Output bild directory (default: {})".format(os.path.join("_build","nrfconnect")),
    }
)
def clean(ctx, target_name, build_dir=os.path.join("_build","nrfconnect")):
    """Remove all files for a nRF connect SDK build"""
    build_dir = os.path.join(build_dir, target_name)
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
