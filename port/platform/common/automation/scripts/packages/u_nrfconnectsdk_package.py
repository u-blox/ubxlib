import os
import json
import shutil
from urllib.request import urlopen
from invoke import Context
from .u_base_package import UPackageException, UAbortedException, UBasePackage
from .u_pkg_utils import is_linux
from . import u_pkg_utils

SDK_NRF_URL="https://github.com/nrfconnect/sdk-nrf"

class UNrfConnectSdkPackage(UBasePackage):

    def check_installed(self, ctx: Context):
        """Check that the toolchain for nRF connect SDK is installed"""
        # Check nRF connect SDK
        return os.path.exists(os.path.join(self.package_dir, ".west"))


    def install(self, ctx: Context):
        """Install nRF connect SDK"""
        if self.check_installed(ctx):
            print(f"{self.package_dir} already exists")
            if not u_pkg_utils.question("Do you want to remove the directory and re-install?"):
                raise UAbortedException
            shutil.rmtree(self.package_dir)

        os.makedirs(self.package_dir, exist_ok=True)
        with ctx.prefix(u_pkg_utils.change_dir_prefix(self.package_dir)):
            ctx.run(f"west init -m {SDK_NRF_URL} --mr {self.version}")
            ctx.run(f"west update")
