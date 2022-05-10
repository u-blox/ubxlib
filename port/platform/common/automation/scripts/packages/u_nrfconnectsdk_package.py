import os
import json
import shutil
from urllib.request import urlopen
from invoke import Context
from .u_base_package import UPackageException, UAbortedException, UBasePackage
from .u_pkg_utils import is_linux
from . import u_pkg_utils

ZEPHYR_URL="https://github.com/zephyrproject-rtos/zephyr.git"
SDK_NRF_URL="https://github.com/nrfconnect/sdk-nrf"
NRF_TOOLCHAIN_BASE_URL="https://developer.nordicsemi.com/.pc-tools/toolchain"
TOOLCHAIN_DIR="toolchain"

def download_windows_ncs_toolchain(version, install_dir):
    """Download and install nRF connect SDK"""
    with urlopen(f"{NRF_TOOLCHAIN_BASE_URL}/index.json") as url:
        toolchain_list = json.loads(url.read().decode())
        for entry in toolchain_list:
            if entry["version"] == version:
                toolchain_url = NRF_TOOLCHAIN_BASE_URL + "/" + entry['toolchains'][0]['name']
                break

    if not toolchain_url:
        raise UPackageException(f"Could not find NCS toolchain {version}")

    u_pkg_utils.download_and_extract(toolchain_url, install_dir)

class UNrfConnectSdkPackage(UBasePackage):

    def get_windows_toolchain_path(self):
        return f"{self.package_dir}/{TOOLCHAIN_DIR}"

    def check_windows_toolchain(self, ctx: Context):
        """Check nRF connect toolchain"""
        return os.path.exists(self.get_windows_toolchain_path())

    def install_windows_toolchain(self, ctx: Context):
        """Install nRF connect toolchain"""
        download_windows_ncs_toolchain(self.version, self.get_windows_toolchain_path())


    def check_installed(self, ctx: Context):
        """Check that the toolchain for nRF connect SDK is installed"""
        if not is_linux():
            # Windows has a toolchain archive - make sure it has been downloaded
            if not self.check_windows_toolchain(ctx):
                return False

        # Check nRF connect SDK
        return os.path.exists(os.path.join(self.package_dir, ".west"))


    def install(self, ctx: Context):
        """Install nRF connect SDK"""
        if self.check_installed(ctx):
            print(f"{self.package_dir} already exists")
            if not u_pkg_utils.question("Do you want to remove the directory and re-install?"):
                raise UAbortedException
            shutil.rmtree(self.package_dir)

        if not is_linux():
            # Windows has a toolchain archive - make sure it has been downloaded
            if not self.check_windows_toolchain(ctx):
                self.install_windows_toolchain(ctx)

        os.makedirs(self.package_dir, exist_ok=True)
        with ctx.prefix(u_pkg_utils.change_dir_prefix(self.package_dir)):
            ctx.run(f"west init -m {SDK_NRF_URL} --mr {self.version}")
            ctx.run(f"west update")
