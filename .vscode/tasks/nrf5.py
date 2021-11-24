import os, sys
import shutil
from invoke import task
from . import utils, arm_embedded, unity, make

@task(
    pre=[
        arm_embedded.check_installation,
        unity.check_installation,
        make.check_installation
    ],
)
def check_installation(ctx):
    """Check nRF5 SDK installation"""
    cfg = ctx.config.nrf5
    ctx.nrf5_env = [
        f"GNU_INSTALL_ROOT={ctx.config.arm_embedded.install_dir}/bin/",
        f"GNU_PREFIX=arm-none-eabi",
        f"GNU_VERSION={ctx.config.arm_embedded.version}",
        f"UNITY_PATH={ctx.config.unity.install_dir}",
    ]
    sys.stdout.write("Checking nRF5 SDK: ")
    if not os.path.exists(cfg.install_dir):
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download nRF5 SDK v{}?".format(cfg.version)):
            utils.download_and_extract(cfg.url, cfg.install_dir, skip_first_sub_dir=True)
        else:
            exit()
    else:
        print("Found {} ({})".format(cfg.version, cfg.install_dir))
    ctx.nrf5_dir = cfg.install_dir

@task(
    pre=[check_installation],
    help={
        "makefile_dir": "Makefile project directory to build",
        "target_name": "A target name (build sub folder)",
        "builddir": "Output bild directory (default: {})".format(os.path.join("_build","nrf5")),
    }
)
def build(ctx, makefile_dir, target_name, builddir=os.path.join("_build","nrf5")):
    """Build a nRF5 SDK based application"""
    # Read U_FLAGS from nrfconnect.u_flags
    u_flags = utils.get_u_flags(ctx.config.cfg_dir, "nrf5", target_name)
    # If the flags has been modified we trigger a rebuild
    if u_flags['modified']:
        clean(ctx, target_name, builddir)

    builddir = os.path.abspath(os.path.join(builddir, target_name))
    os.makedirs(builddir, exist_ok=True)
    with ctx.cd(makefile_dir):
        # OUTPUT_DIRECTORY is very picky in Windows.
        # Seems it must be a relative path and `\` directory separators must NOT be used.
        builddir = os.path.relpath(builddir, makefile_dir).replace("\\", "/")
        ctx.run(f'make -j8 UBXLIB_PATH={ctx.config.root_dir} OUTPUT_DIRECTORY={builddir} '\
                f'NRF5_PATH={ctx.nrf5_dir} CFLAGS="{u_flags["u_flags"]}" {" ".join(ctx.nrf5_env)}')

@task(
    help={
        "target_name": "A target name (build sub folder)",
        "builddir": "Output bild directory (default: {})".format(os.path.join("_build","nrf5")),
    }
)
def clean(ctx, target_name='nrf52840_xxaa', builddir=os.path.join("_build","nrf5")):
    """Remove all files for a nRF5 SDK build"""
    build_dir = os.path.abspath(os.path.join(builddir, target_name))
    if os.path.exists(builddir):
        shutil.rmtree(builddir)
