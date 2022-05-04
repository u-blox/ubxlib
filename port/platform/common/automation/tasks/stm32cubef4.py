import os
import shutil
import time
import sys
from telnetlib import Telnet
from invoke import task, Exit
from tasks import task_utils
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package, u_pkg_utils
from scripts.u_utils import SwoDecoder
from pathlib import Path

STM32CUBE_F4_URL="https://github.com/STMicroelectronics/STM32CubeF4.git"

DEFAULT_MAKEFILE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/stm32cube/mcu/stm32f4/runner"
DEFAULT_OUTPUT_NAME = "runner_stm32f4"
DEFAULT_BUILD_DIR = os.path.join("_build","stm32cubef4")
DEFAULT_JOB_COUNT = 8
DEFAULT_FLASH_FILE = f"runner.elf"

OPENOCD_DISABLE_PORTS_CMDS = [
    'gdb_port disabled',
    'tcl_port disabled',
    'telnet_port disabled'
]


def _to_openocd_args(openocd_cmds):
    argstr = ''
    for arg in openocd_cmds:
        argstr += f'-c "{arg}" '
    return argstr

@task()
def check_installation(ctx):
    """Check STM32CubeF4 SDK installation"""
    # Load required packages
    pkgs = u_package.load(ctx, [
        "make", "unity", "arm_embedded_gcc", "stm32cubef4", "openocd"
    ])
    stm32cubef4_pkg = pkgs["stm32cubef4"]
    ae_gcc_pkg = pkgs["arm_embedded_gcc"]
    unity_pkg = pkgs["unity"]

    ctx.stm32cubef4_env = [
        f"ARM_GCC_TOOLCHAIN_PATH={ae_gcc_pkg.get_install_path()}/bin",
        f"UNITY_PATH={unity_pkg.get_install_path()}",
        f"STM32CUBE_FW_PATH={stm32cubef4_pkg.get_install_path()}",
    ]
    ctx.stm32cubef4_dir = stm32cubef4_pkg.get_install_path()
    ctx.arm_toolchain_path = ae_gcc_pkg.get_install_path() + "/bin"

@task(
    pre=[check_installation],
    help={
        "makefile_dir": f"Makefile project directory to build (default: {DEFAULT_MAKEFILE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
        "jobs": f"The number of Makefile jobs (default: {DEFAULT_JOB_COUNT})"
    }
)
def build(ctx, makefile_dir=DEFAULT_MAKEFILE_DIR, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, jobs=DEFAULT_JOB_COUNT, u_flags=None):
    """Build a STM32CubeF4 SDK based application"""
    # When user calls the "analyze" task it will in turn call this function
    # with ctx.is_static_analyze=True
    is_static_analyze = False
    if hasattr(ctx, 'is_static_analyze'):
        is_static_analyze = ctx.is_static_analyze

    # Handle u_flags
    if u_flags:
        cflags = u_flags_to_cflags(u_flags)
    else:
        # Read U_FLAGS from stm32cubef4.u_flags
        u_flags = get_cflags_from_u_flags_yml(ctx.config.vscode_dir, "stm32cubef4", output_name)
        # If the flags has been modified we trigger a rebuild
        if u_flags['modified']:
            clean(ctx, output_name, build_dir)
        cflags = u_flags["cflags"]

    ctx.build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    os.makedirs(ctx.build_dir, exist_ok=True)
    with ctx.prefix(u_pkg_utils.change_dir_prefix(makefile_dir)):
        # OUTPUT_DIRECTORY is very picky in Windows.
        # Seems it must be a relative path and `\` directory separators must NOT be used.
        build_dir = os.path.relpath(ctx.build_dir, makefile_dir).replace("\\", "/")
        build_cmd = f'make -j{jobs} UBXLIB_PATH={ctx.config.root_dir} OUTPUT_DIRECTORY={build_dir} '\
                    f'CFLAGS=\'{cflags}\' {" ".join(ctx.stm32cubef4_env)}'
        if is_static_analyze:
            ctx.analyze_dir = f"{ctx.build_dir}/analyze"
            check_proc = ctx.run(f'CodeChecker check -b "{build_cmd}" -o {ctx.analyze_dir} ' \
                                 f'--config {u_utils.CODECHECKER_CFG_FILE} -i {u_utils.CODECHECKER_IGNORE_FILE}', warn=True)
            if check_proc.exited == 1:
                raise Exit("CodeChecker error")
            elif check_proc.exited >= 128:
                raise Exit("CodeChecker fatal error")
        else:
            ctx.run(build_cmd)

@task(
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files for a STM32CubeF4 build"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)


@task(
    pre=[check_installation],
    help={
        "file": f"The file to flash (default: {DEFAULT_FLASH_FILE}",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "debugger_serial": "The debugger serial number (optional)",
    }
)
def flash(ctx, file=DEFAULT_FLASH_FILE, debugger_serial="",
          output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Flash a nRF5 SDK based application"""
    build_dir = Path(build_dir, output_name).absolute().as_posix()
    cmds = OPENOCD_DISABLE_PORTS_CMDS
    if debugger_serial != "":
        cmds.append(f'hla_serial {debugger_serial}')
    cmds += [
        f'program {build_dir}/{file} reset',
        'exit'
    ]
    args = _to_openocd_args(cmds)
    ctx.run(f'openocd -f {u_utils.OPENOCD_CFG_DIR}/stm32f4.cfg {args}')

@task(
    pre=[check_installation],
    help={
        "debugger_serial": "The debugger serial number (optional)",
        "port": "The local TCP port to use for OpenOCD SWO output",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}). This is used for finding ELF file",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR}). This is used for finding ELF file",
        "elf_file": f"The .elf file used for decoding backtrace. When this is used --build-dir and --output-name has no function."
    }
)
def log(ctx, debugger_serial="", port=40404,
        output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR,
        elf_file=None):
    """Open a log terminal"""

    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if elf_file is None:
        elf_file = task_utils.get_elf(build_dir, "*.elf")

    cmds = OPENOCD_DISABLE_PORTS_CMDS
    if debugger_serial != "":
        cmds.append(f'hla_serial {debugger_serial}')
    cmds += [
        'init',
        f'tpiu config internal :{port} uart off \$_TARGET_SYSTEM_FREQUENCY \$_TARGET_SWO_FREQUENCY',
        'itm port 0 on',
        'reset init',
        'resume'
    ]
    args = _to_openocd_args(cmds)
    promise = ctx.run(f'openocd -f {u_utils.OPENOCD_CFG_DIR}/stm32f4.cfg {args}', asynchronous=True)
    # Let OpenOCD startup first
    time.sleep(5)
    try:
        line = ""
        decoder = SwoDecoder(0, True)
        with Telnet('127.0.0.1', port) as tn:
            while True:
                data = tn.read_some()
                if data == b'':
                    break
                decoded_data = decoder.decode(data)
                line += "".join(map(chr, decoded_data)).replace("\r", "")
                if "\n" in line:
                    lines = line.split("\n")
                    for l in lines[:-1]:
                        if "Backtrace: " in l:
                            parse_backtrace(ctx, elf_file, l)
                            sys.stdout.write(l + "\n")
                        else:
                            sys.stdout.write(l + "\n")
                    sys.stdout.flush()
                    line = lines[-1]

    finally:
        if u_utils.is_linux:
            promise.runner.kill()
        else:
            promise.runner.send_interrupt(KeyboardInterrupt())

@task(
    pre=[check_installation],
)
def parse_backtrace(ctx, elf_file, line):
    """Parse a backtrace line
    Example usage: inv stm32cubef4.parse-backtrace zephyr.elf "Backtrace:0x400ec4df:0x3ffbabb0 0x400df5a6:0x3ffbabd0"
    """
    task_utils.parse_backtrace(ctx, elf_file, line, toolchain_prefix=f"{ctx.arm_toolchain_path}/arm-none-eabi-")

@task(
    pre=[check_installation],
    help={
        "makefile_dir": f"Makefile project directory to build (default: {DEFAULT_MAKEFILE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
        "jobs": f"The number of Makefile jobs (default: {DEFAULT_JOB_COUNT})"
    }
)
def analyze(ctx, makefile_dir=DEFAULT_MAKEFILE_DIR, output_name=DEFAULT_OUTPUT_NAME,
            build_dir=DEFAULT_BUILD_DIR, jobs=DEFAULT_JOB_COUNT, u_flags=None):
    """Run CodeChecker static code analyzer (clang-analyze + clang-tidy)"""
    ctx.is_static_analyze = True
    build(ctx, makefile_dir, output_name, build_dir, jobs, u_flags)

    parse_proc = ctx.run(f'CodeChecker parse -e html {ctx.analyze_dir} -o {ctx.build_dir}/analyze_html', warn=True, hide=u_utils.is_automation())
    # Check the return codes
    if parse_proc.exited == 2:
        if u_utils.is_automation():
            # When running on Jenkins we print out an URL to the test report
            workspace_dir = os.environ['WORKSPACE']
            rel_analyze_path = os.path.relpath(f"{build_dir}/analyze_html/index.html", workspace_dir)
            url = f"{os.environ['BUILD_URL']}artifact/{rel_analyze_path}"
            print("\n"
                    "*************************************************************\n"
                    "* CodeChecker returned a report\n"
                    "*************************************************************\n",
                    file=sys.stderr)
            print(f"Please see the report here:\n{url}")
            raise Exit(f"CodeChecker found things to address")
    elif not parse_proc.ok:
        print(parse_proc.stdout)
        print(parse_proc.stderr, file=sys.stderr)
        raise Exit(f"CodeChecker parse failed")
