import os
import shutil
from . import u_pkg_utils
from invoke import Context
from .u_pkg_utils import is_linux
from .u_base_package import UAbortedException, UGitPackage

ESP_IDF_URL="https://github.com/espressif/esp-idf"

class UEspIdfPackage(UGitPackage):

    def install_esp_idf_tools(self, ctx: Context):
        """Install the ESP-IDF toolchain"""
        idf_tools_dir = self.get_idf_tools_path()
        if os.path.exists(idf_tools_dir):
            print(f"{idf_tools_dir} already exists")
            if not u_pkg_utils.question("Do you want to remove the directory and re-install?"):
                raise UAbortedException
            shutil.rmtree(idf_tools_dir)
        if is_linux():
            ctx.run(f"{self.package_dir}/install.sh")
        else:
            ctx.run(f"{self.package_dir}/install.bat")


    def check_installed(self, ctx: Context):
        """Check ESP-IDF SDK installation"""
        ctx.config.run.env["IDF_TOOLS_PATH"] = self.get_idf_tools_path()
        self.switched_rev = False
        if not super().check_installed(ctx):
            return False
        if self.switched_rev:
            self.install_esp_idf_tools(ctx)

        if not os.path.exists(self.get_idf_tools_path()):
            return False
        return True

    def install(self, ctx: Context):
        """Install ESP-IDF SDK"""
        super().install(ctx)
        self.install_esp_idf_tools(ctx)

    def get_idf_tools_path(self):
        """Get the ESP-IDF toolchain install dir"""
        return f"{os.environ['UBXLIB_PKG_DIR']}/esp_idf_tools-{self.version}"

