import os
import shutil
from pathlib import Path
from invoke import Context
from .u_base_package import UAbortedException, UBasePackage
from . import u_pkg_utils

# The west manifest file
MANIFEST_FILE_NAME = "west.yml"

# The name of the directory that west.yml will be copied to
MANIFEST_DIR = ".manifest"

class UZephyrPackage(UBasePackage):

    def check_installed(self, ctx: Context):
        """Check that the Zephyr is installed"""
        return os.path.exists(os.path.join(self.package_dir, ".west"))

    def install(self, ctx: Context):
        """Install Zephyr"""
        if self.check_installed(ctx):
            print(f"{self.package_dir} already exists")
            if not u_pkg_utils.question("Do you want to remove the directory and re-install?"):
                raise UAbortedException
            shutil.rmtree(self.package_dir)

        # Initialise west over in the package directory
        os.makedirs(os.path.join(self.package_dir, MANIFEST_DIR), exist_ok=True)
        # Copy west.yml into the manifest directory
        destination_file_path = os.path.join(self.package_dir, MANIFEST_DIR, MANIFEST_FILE_NAME)
        manifest_source_path=os.path.join(ctx.config['cfg_dir'], MANIFEST_FILE_NAME)
        shutil.copy2(manifest_source_path, os.path.join(self.package_dir, destination_file_path))
        # Update the revision number
        file = Path(destination_file_path)
        file.write_text(file.read_text().replace('REVISION_REPLACE_ME', self.version))
        with ctx.prefix(u_pkg_utils.change_dir_prefix(self.package_dir)):
            ctx.run(f"west init -l {MANIFEST_DIR}")
            ctx.run(f"west update -o=--depth=1 -n")
