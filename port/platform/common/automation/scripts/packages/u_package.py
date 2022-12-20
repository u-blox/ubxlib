import os
from invoke import Context
from portalocker import Lock
from portalocker.exceptions import AlreadyLocked
from .. import u_config
from .u_pkg_utils import is_automation, is_linux, is_arm
from .u_base_package import *
from .u_nrfconnectsdk_package import UNrfConnectSdkPackage
from .u_esp_idf_package import UEspIdfPackage
from .u_segger_jlink_package import USeggerJlinkPackage
from typing import List, Dict

AUTOMATION_LOCK_TIMEOUT = 60 * 15

def get_u_packages_config(ctx: Context):
    """Get the parsed u_packages.yml config"""
    # First check if user have specified a specific folder to put
    # the u_packages in
    if not "UBXLIB_PKG_DIR" in os.environ:
        if is_linux():
            pkg_dir = "${HOME}/.ubxlibpkg"
        else:
            pkg_dir = "${UserProfile}\\.ubxlibpkg"
        os.environ["UBXLIB_PKG_DIR"] = os.path.expandvars(pkg_dir)
        ctx.config.run.env["UBXLIB_PKG_DIR"] = os.environ["UBXLIB_PKG_DIR"]

    # Load u_packages.yml
    if not 'cfg_dir' in ctx.config:
        ctx.config['cfg_dir'] = os.getcwd()
    cfg_file = os.path.join(ctx.config['cfg_dir'], "u_packages.yml")
    pkg_cfg = u_config.load_config_yaml(cfg_file, is_linux(), is_arm())
    # For all packages that doesn't specify package_dir we add a default value
    for pkg_name in pkg_cfg:
        if not "package_dir" in pkg_cfg[pkg_name]:
            dir = os.path.join(os.path.expandvars("${UBXLIB_PKG_DIR}"), os.path.expandvars(f"{pkg_name}"))
            if "version" in pkg_cfg[pkg_name]:
                dir = dir + f"-{pkg_cfg[pkg_name]['version']}"
            pkg_cfg[pkg_name]["package_dir"] = dir

    return pkg_cfg


def load(ctx: Context, packages: List[str]) -> Dict[str,UBasePackage]:
    """Load a set of u_packages
    This function will use u_packages.yml to look for packages.
    If the packages aren't installed the user will get a question if
    they like to install them. If wrong version is installed an attempt
    to switch version will be made.

    The function will return a dict of u_package classes (inheriting
    UBasePackage). These can be used to query installation dir etc like:
      pkgs = u_package.load["my_package"]
      pkgs["my_package"].get_install_path()
    """
    pkg_cfg = get_u_packages_config(ctx)
    UBXLIB_PKG_DIR = os.environ["UBXLIB_PKG_DIR"]

    if not os.path.exists(UBXLIB_PKG_DIR):
        os.makedirs(UBXLIB_PKG_DIR)

    try:
        with Lock(f"{UBXLIB_PKG_DIR}/lock",
                  fail_when_locked = not is_automation(),
                  timeout=AUTOMATION_LOCK_TIMEOUT):
            u_packages = {}
            needs_install = []

            print("=== Loading u_packages ===")
            for pkg_name in packages:
                if not pkg_name in pkg_cfg:
                    raise UPackageException(f"Unknown package name: {pkg_name}")
                type = pkg_cfg[pkg_name]['type']
                if type == "nrfconnectsdk":
                    pkg = UNrfConnectSdkPackage()
                elif type == "archive":
                    pkg = UArchivePackage()
                elif type == "apt":
                    pkg = UAptPackage()
                elif type == "git":
                    pkg = UGitPackage()
                elif type == "executable":
                    pkg = UExecutablePackage()
                elif type == "esp_idf":
                    pkg = UEspIdfPackage()
                elif type == "segger_jlink":
                    pkg = USeggerJlinkPackage()
                else:
                    raise UPackageException(f"Unknown package type: {type}")
                pkg.name = pkg_name
                pkg.cfg = pkg_cfg[pkg_name]
                pkg.version = pkg.cfg["version"] if "version" in pkg.cfg else ""
                pkg.package_dir = pkg.cfg["package_dir"]
                u_packages[pkg.name] = pkg

                # Check if the package is installed
                # if not we will try to install it in next stage
                if pkg.check_installed(ctx):
                    print(f"Found {pkg.name} {pkg.version}")
                else:
                    needs_install.append(pkg)

            # Check if there are packages to install
            if len(needs_install):
                print("The following packages could not be found:")
                for pkg in needs_install:
                    print(f" {pkg.name} {pkg.version}")
                if not u_pkg_utils.question("Do you want to install them?"):
                    raise UAbortedException()
                for pkg in needs_install:
                    print(f"=== Installing {pkg.name} ===")
                    pkg.install(ctx)
                    # If there is a post installation shell command let's execute that now
                    if "post_install_command" in pkg.cfg:
                        cmd = pkg.cfg["post_install_command"]
                        with ctx.prefix(u_pkg_utils.change_dir_prefix(pkg.get_install_path())):
                            ctx.run(cmd)

            # Load the packages
            for pkg_name in u_packages:
                # Save the package installation path to an environment variable
                pkg = u_packages[pkg_name]
                ctx.config.run.env[f"U_PKG_{pkg_name.upper()}"] = pkg.package_dir

                # Check if the package needs to add a directory to PATH
                if "add_to_path" in pkg.cfg:
                    dir = f"{pkg.get_install_path()}/{pkg.cfg['add_to_path']}"
                    os.environ["PATH"] = os.pathsep.join([dir, os.environ["PATH"]])

    except AlreadyLocked:
        raise UPackageException("ERROR: u_package is busy")

    return u_packages
