import os
import shutil
import tempfile
from . import u_pkg_utils
from invoke import Context
from .u_base_package import UPackageException, UBasePackage

class USeggerJlinkPackage(UBasePackage):

    def install_segger_jlink_tools(self, ctx: Context):
        """Install the Segger JLink tools"""
        # Segger put an "accept our terms and conditions" page
        # in front of the download of their tools; for Linux
        # Nordic provide a zip file which contains the tools
        if u_pkg_utils.is_linux():
            with tempfile.TemporaryDirectory() as temp_dir:
                if "url" not in self.cfg:
                    print("No URL specified to download a Segger tools file from")
                    raise UPackageException
                if "package_name" not in self.cfg:
                    print("No package name specified to extract and install")
                    raise UPackageException
                u_pkg_utils.download_and_extract(self.cfg['url'], temp_dir, False)
                install_path = os.path.join(temp_dir, self.cfg['package_name'])
                if os.path.exists(install_path):
                    ctx.run(f"sudo apt install -y {install_path}")
                    # The dependencies check in the Segger installation file needs to be sorted
                    ctx.run("sudo apt --fix-broken install")
                    # Now we can try again
                    ctx.run(f"sudo apt install -y {install_path}")
                else:
                    print(f"Unable to find downloaded/extracted file {install_path}")
                    raise UPackageException
        else:
            print("Please install the Segger J-Link Software and Documentation Pack from https://www.segger.com/downloads/jlink/")
            raise UPackageException

    def check_installed(self, ctx: Context):
        """Check Segger JLink installation"""
        is_ok = False
        try:
            is_ok = ctx.run(f"{self.cfg['check_command']}", hide=True).ok
        except:
            pass
        return is_ok

    def install(self, ctx: Context):
        """Install Segger JLink"""
        self.install_segger_jlink_tools(ctx)

