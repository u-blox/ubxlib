import shutil
import os
import tempfile
from invoke import Context
from . import u_pkg_utils

class UPackageException(Exception):
    """u_package exception"""
    def __init__(self, message=None):
        super().__init__(message)

class UAbortedException(UPackageException):
    """User aborted exception"""
    def __init__(self, message=None):
        super().__init__(message)

class UBasePackage:
    """Base class for an u_package"""
    def get_version(self) -> str:
        """Return the package version as a string"""
        return self.version
    def get_install_path(self) -> str:
        """Return the full package installation dir"""
        return self.package_dir

class UGitPackage(UBasePackage):
    """Git repo package"""
    def check_installed(self, ctx: Context):
        """Check if the repo is installed"""
        version = self.version
        if not os.path.exists(self.package_dir):
            return False
        with ctx.prefix(u_pkg_utils.change_dir_prefix(self.package_dir)):
            result = ctx.run("git tag --points-at HEAD", hide=True, warn=True)
            if not result.ok:
                print("Failed to get git tags - git repo probably not initialized")
                return False
            tags = result.stdout.splitlines()
            if version in tags:
                # Found a matching tag so we are good
                return True
            branch = ctx.run(f"git rev-parse --abbrev-ref HEAD", hide=True).stdout.strip()
            if version == branch:
                # Branch name maches version so we are good
                return True

            current_version = branch if len(tags) == 0 else tags[0]
            print(f"Found version: {current_version}, but need version: {version}")
            if not u_pkg_utils.question("Do you want to switch version?"):
                raise UAbortedException

            # Check if repo is dirty
            if not ctx.run(f"git diff-files --quiet", hide=True).ok:
                raise UPackageException("Can't switch version since repo is dirty")

            self.switched_rev = True # Hack to notify class children when we are switching revision
            ctx.run(f"git fetch \"+refs/tags/{version}:refs/tags/{version}\"", hide=True, warn=True)
            ctx.run(f"git fetch origin {version}:{version}", warn=True)
            ctx.run(f"git -c advice.detachedHead=False checkout {version}")
            ctx.run(f"git submodule sync --recursive")
            ctx.run(f"git submodule update --recursive")
            return True


    def install(self, ctx: Context):
        """Installed (clone) the repo
        Note: Will also clone git submodules"""
        url = self.cfg['url']
        if os.path.exists(self.package_dir):
            print(f"{self.package_dir} already exists")
            if not u_pkg_utils.question("Do you want to remove the directory and re-install?"):
                raise UAbortedException
            with ctx.prefix(u_pkg_utils.change_dir_prefix(self.package_dir)):
                # Check if repo is dirty
                if not ctx.run(f"git diff-files --quiet", hide=True).ok:
                    raise UPackageException("Can't remove directory since the repo is dirty")
            shutil.rmtree(self.package_dir)
        return ctx.run(f"git -c advice.detachedHead=False " \
                       f"clone --branch {self.version} --recursive --depth 1 {url} {self.package_dir}").ok


class UAptPackage(UBasePackage):
    """Linux APT package"""
    def check_installed(self, ctx: Context):
        """Check if the apt package is installed"""
        is_ok = False
        try:
            is_ok = ctx.run(f"{self.cfg['check_command']}", hide=True).ok
        except:
            pass
        return is_ok

    def install(self, ctx):
        """Install the apt package"""
        is_ok = False
        if "url" in self.cfg:
            with tempfile.TemporaryDirectory() as temp_dir:
                skip_first_subdir = self.cfg['skip_first_subdir'] if 'skip_first_subdir' in self.cfg else False
                u_pkg_utils.download_and_extract(self.cfg['url'], temp_dir, skip_first_subdir)
                pkg_path = os.path.join(temp_dir, self.cfg['package_name'])
                is_ok = ctx.run(f"sudo apt update && sudo apt install -y {pkg_path}").ok
        else:
            is_ok = ctx.run(f"sudo apt update && sudo apt install -y {self.cfg['package_name']}").ok
        return is_ok


class UArchivePackage(UBasePackage):
    """Archive package
    Will download and extract archive from an URL.
    Supports both .tar and .zip-files"""
    def check_installed(self, ctx: Context):
        """Check if the archive is installed
        Note: Each installed archive will have a .ubxversion file containing the version number"""
        version = self.cfg['version']
        version_file = f"{self.package_dir}/.ubxversion"
        if not os.path.exists(version_file):
            return False
        current_version = ""
        with open(version_file, 'r', encoding='utf8') as file:
            current_version = file.read().rstrip()
        return current_version == version

    def install(self, ctx: Context):
        """Install the archive
        Note: A .ubxversion file will be placed in the package dir containing the version number"""
        url = self.cfg['url']
        version_file = f"{self.package_dir}/.ubxversion"
        skip_first_subdir = self.cfg['skip_first_subdir'] if 'skip_first_subdir' in self.cfg else False
        if os.path.exists(self.package_dir):
            if not u_pkg_utils.question("Do you want to remove the directory and re-install?"):
                raise UAbortedException
            shutil.rmtree(self.package_dir)

        u_pkg_utils.download_and_extract(url, self.package_dir, skip_first_subdir)
        with open(version_file, 'w', encoding='utf8') as f:
            f.write(self.version)

class UExecutablePackage(UBasePackage):
    """Executable package
    Will download and run an executable from a URL.  The
    executable may be in a zip or tar file, or just plain"""
    def check_installed(self, ctx: Context):
        """Check if the executable is installed
        Note: Each installed archive will have a .ubxversion file containing the version number"""
        version = self.cfg['version']
        version_file = f"{self.package_dir}/.ubxversion"
        if not os.path.exists(version_file):
            return False
        current_version = ""
        with open(version_file, 'r', encoding='utf8') as file:
            current_version = file.read().rstrip()
        return current_version == version

    def install(self, ctx: Context):
        """Install the executable
        Note: A .ubxversion file will be placed in the package dir containing the version number"""
        url = self.cfg['url']
        version_file = f"{self.package_dir}/.ubxversion"
        skip_first_subdir = self.cfg['skip_first_subdir'] if 'skip_first_subdir' in self.cfg else False
        if os.path.exists(self.package_dir):
            if not u_pkg_utils.question("Do you want to remove the directory and re-install?"):
                raise UAbortedException
            shutil.rmtree(self.package_dir)

        u_pkg_utils.download_and_extract(url, self.package_dir, skip_first_subdir)
        if 'run_with_switches' in self.cfg:
            run_path = os.path.join(self.package_dir, self.cfg['package_name'])
            if (ctx.run(f"{run_path} {self.cfg['run_with_switches']}").ok):
                with open(version_file, 'w', encoding='utf8') as f:
                    f.write(self.version)
            else:
                print(f"Unable to run {run_path} with switches {self.cfg['run_with_switches']}")
                shutil.rmtree(self.package_dir)


