import os, sys
from invoke import task
from . import utils

WINDOWS_MAKE_URL="http://repo.msys2.org/mingw/x86_64/mingw-w64-x86_64-make-4.3-1-any.pkg.tar.xz"

@task
def check_installation(ctx):
    """Check make installation"""
    if not ctx.config.is_linux:
        cfg = ctx.config.make
        mingw_make_bin_dir = f"{cfg.install_dir}/mingw64/bin"
        sys.stdout.write("Checking make: ")
        if not os.path.exists(cfg.install_dir):
            print("Not found ({})".format(cfg.install_dir))
            if utils.question("Do you want to download mingw make?"):
                utils.download_and_extract(WINDOWS_MAKE_URL, cfg.install_dir)
                os.rename(f'{mingw_make_bin_dir}/mingw32-make.exe', f'{mingw_make_bin_dir}/make.exe')
            else:
                exit(-1)
        else:
            print("Found ({})".format(cfg.install_dir))
        utils.add_dir_to_path(ctx.config, mingw_make_bin_dir)
