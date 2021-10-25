import os, sys
from invoke import task
from . import utils

UNITY_URL="https://github.com/ThrowTheSwitch/Unity"

@task
def check_installation(ctx):
    """Check Unity installation"""
    sys.stdout.write("Checking Unity: ")
    cfg = ctx.config.unity
    if not os.path.exists(cfg.install_dir):
        print("Not found ({})".format(cfg.install_dir))
        if utils.question("Do you want to download Unity?"):
            ctx.run(f"git clone {UNITY_URL} {cfg.install_dir}")
        else:
            exit()
    else:
        print("Found ({})".format(cfg.install_dir))
