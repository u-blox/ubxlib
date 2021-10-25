import os, sys
from invoke import task
from . import utils

ARM_TOOLCHAIN_PATH_FILE="arm-toolchain-path.txt"
GNUARMEMB_TOOLCHAIN_VERSION="10-2020-q4-major"
LINUX_ARM_EMBEDDED_URL=  f"https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-{GNUARMEMB_TOOLCHAIN_VERSION}-x86_64-linux.tar.bz2"
WINDOWS_ARM_EMBEDDED_URL=f"https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-{GNUARMEMB_TOOLCHAIN_VERSION}-win32.zip"

@task
def check_installation(ctx):
    """Check GNU ARM embedded GCC installation"""
    sys.stdout.write("Checking GNU ARM embedded toolchain: ")
    cfg = ctx.config.arm_embedded
    if not os.path.exists(cfg.install_dir):
        if ctx.config.is_linux:
            url = LINUX_ARM_EMBEDDED_URL
        else:
            url = WINDOWS_ARM_EMBEDDED_URL
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download GNU ARM embedded toolchain {}?".format(GNUARMEMB_TOOLCHAIN_VERSION)):
            utils.download_and_extract(url, cfg.install_dir, skip_first_sub_dir=True)
        else:
            exit()
    else:
        print("Found {} ({})".format(cfg.version, cfg.install_dir))

    # Put the installation path in a file that vscode launch.json can use for locating the toolchain
    with open(ARM_TOOLCHAIN_PATH_FILE, 'w') as f:
        f.write(os.path.join(cfg.install_dir, "bin"))
