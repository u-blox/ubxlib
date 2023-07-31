import os
import shutil
import sys
import json
import re
from pathlib import Path
from time import time
from datetime import timedelta


from invoke import task, Exit
from tasks import task_utils
from scripts import u_utils, u_get_build_dir
from scripts.u_log_readers import URttReader
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package, u_pkg_utils

DEFAULT_CMAKE_DIR = f"{u_utils.PLATFORM_DIR}/linux/mcu/posix/runner"
DEFAULT_OUTPUT_NAME = "runner_linux"
DEFAULT_BUILD_DIR = u_get_build_dir.DEFAULT_BUILD_DIR

@task()
def check_installation(ctx):
    """Check Linux installation"""
    # Load required packages
    pkgs = u_package.load(ctx, ["cmake", "libssl-dev", "libgpiod-dev", "unity"])
    unity_pkg = pkgs["unity"]

    ctx.config.run.env["UNITY_PATH"] = unity_pkg.get_install_path()

@task(
    pre=[check_installation],
    help={
        "cmake_dir": f"CMake project directory to build (default: {DEFAULT_CMAKE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)"
    }
)
def build(ctx, cmake_dir=DEFAULT_CMAKE_DIR,
          output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR,
          u_flags=None):
    """Build a Linux based application"""
    rebuild = False

    # Handle u_flags
    if u_flags:
        ctx.config.run.env["U_FLAGS"] = u_flags_to_cflags(u_flags)
    else:
        # Read U_FLAGS from Linux u_flags
        u_flags = get_cflags_from_u_flags_yml(ctx.config.vscode_dir, "linux", output_name)
        ctx.config.run.env["U_FLAGS"] = u_flags["cflags"]
        # If the flags has been modified we trigger a rebuild
        if u_flags['modified']:
            rebuild = True

    build_dir = os.path.join(build_dir, output_name)

    start = time()
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)
    with u_utils.ChangeDir(build_dir):
        ctx.run(f'cmake {cmake_dir}')
        if rebuild:
            ctx.run(f'make clean')
        ctx.run(f'make')
    print("= Elapsed time:", timedelta(seconds=round(time()-start)))

@task(
    pre=[check_installation],
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files for a Linux build"""
    build_dir = os.path.join(build_dir, output_name)
    if os.path.exists(build_dir):
        print(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)

