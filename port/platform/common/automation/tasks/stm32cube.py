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

DEFAULT_TOOLCHAIN = "ThreadX"
DEFAULT_MAKEFILE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/stm32cube/mcu/stm32f4/runner"
DEFAULT_MAKEFILE_DIR_STM32U5 = f"{u_utils.UBXLIB_DIR}/port/platform/stm32cube/mcu/stm32u5/runner"
DEFAULT_OUTPUT_NAME = "runner_stm32"
DEFAULT_BUILD_DIR = os.path.join("_build","stm32cube")
DEFAULT_JOB_COUNT = 8
DEFAULT_FLASH_FILE = f"runner.elf"

OPENOCD_DISABLE_PORTS_CMDS = [
    'gdb_port disabled',
    'tcl_port disabled',
    'telnet_port disabled'
]

def stm32_get_makefile_dir(ctx, makefile_dir, toolchain):
    """Get the correct makefile directory"""
    if not makefile_dir:
        if ctx.mcu == "stm32u5":
            makefile_dir=DEFAULT_MAKEFILE_DIR_STM32U5
            if toolchain:
                makefile_dir += "_" + toolchain.lower()
        else:
            makefile_dir=DEFAULT_MAKEFILE_DIR
    return makefile_dir

def _to_openocd_args(openocd_cmds):
    argstr = ''
    for arg in openocd_cmds:
        argstr += f'-c "{arg}" '
    return argstr

@task()
def check_installation(ctx):
    """Check STM32Cube SDK installation"""
    # Load required packages

    # The MCU type will be stm32xx, where xx is f4 or u5;
    # append those last two characters to "stm32cube" here
    # to get the name of the FW package
    stm32cube_pkg_name = "stm32cube" + ctx.mcu[5:7]

    # stm32_cmsis_freertos is only required for STM32U5 but it is
    # small enough that we can get it for either case.
    pkgs = u_package.load(ctx, [
        "make", "unity", "arm_embedded_gcc", stm32cube_pkg_name, "stm32_cmsis_freertos", "openocd"
    ])

    stm32cube_pkg = pkgs[stm32cube_pkg_name]
    ae_gcc_pkg = pkgs["arm_embedded_gcc"]
    unity_pkg = pkgs["unity"]
    stm32_cmsis_freertos_pkg=pkgs["stm32_cmsis_freertos"]

    ctx.stm32cube_env = [
        f"ARM_GCC_TOOLCHAIN_PATH={ae_gcc_pkg.get_install_path()}/bin",
        f"UNITY_PATH={unity_pkg.get_install_path()}",
        f"STM32CUBE_FW_PATH={stm32cube_pkg.get_install_path()}",
        f"STM32_CMSIS_FREERTOS_PATH={stm32_cmsis_freertos_pkg.get_install_path()}",
    ]
    ctx.arm_toolchain_path = ae_gcc_pkg.get_install_path() + "/bin"

@task(
    pre=[check_installation],
    help={
        "toolchain": f"ThreadX or FreeRTOS (default: {DEFAULT_TOOLCHAIN})",
        "makefile_dir": f"Makefile project directory to build (default: {DEFAULT_MAKEFILE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
        "jobs": f"The number of Makefile jobs (default: {DEFAULT_JOB_COUNT})",
        "features": "Feature list, e.g. \"cell short_range\" to leave out gnss; overrides the environment variable UBXLIB_FEATURES and u_flags.yml"
    }
)
def build(ctx, toolchain=DEFAULT_TOOLCHAIN, makefile_dir=None, output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, jobs=DEFAULT_JOB_COUNT, u_flags=None,
          features=None):
    """Build a STM32Cube SDK based application"""
    cflags = ""

    makefile_dir = stm32_get_makefile_dir(ctx, makefile_dir, toolchain)

    # When user calls the "analyze" task it will in turn call this function
    # with ctx.is_static_analyze=True
    is_static_analyze = False
    if hasattr(ctx, 'is_static_analyze'):
        is_static_analyze = ctx.is_static_analyze

    # Read U_FLAGS and features from stm32cube.u_flags or
    # stm32cubef4.u_flags (for backwards-compatibility),
    # if they are there
    u_flags_yml = get_cflags_from_u_flags_yml(ctx.config.vscode_dir, "stm32cube", output_name)
    if not u_flags_yml:
        u_flags_yml = get_cflags_from_u_flags_yml(ctx.config.vscode_dir, "stm32cubef4", output_name)
    if u_flags_yml:
        cflags = u_flags_yml["cflags"]
        if not features and "features" in u_flags_yml:
            features = u_flags_yml["features"]
        # If the flags have been modified we trigger a rebuild,
        # but not if we're doing static analysis as it will have
        # already put things in the build
        if u_flags_yml['modified'] and not is_static_analyze:
            clean(ctx, output_name, build_dir)

    # Let any passed-in u_flags override the .yml file
    if u_flags:
        cflags = u_flags_to_cflags(u_flags)

    # Add any UBXLIB_FEATURES from the features parameter
    if features:
        os.environ["UBXLIB_FEATURES"] = features

    ctx.build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    os.makedirs(ctx.build_dir, exist_ok=True)
    with ctx.prefix(u_pkg_utils.change_dir_prefix(makefile_dir)):
        # OUTPUT_DIRECTORY is very picky in Windows.
        # Seems it must be a relative path and `\` directory separators must NOT be used.
        build_dir = os.path.relpath(ctx.build_dir, makefile_dir).replace("\\", "/")
        if is_static_analyze:
            # We do build_cmd differently for static analysis (single quotes
            # around the cflags rather than double); the double quotes are
            # the only ones that work with make on Windows, the single ones
            # are the only ones that work with the static analyzer on Linux.
            build_cmd = f'make -j{jobs} UBXLIB_PATH={ctx.config.root_dir} OUTPUT_DIRECTORY={build_dir} ' \
                        f'CFLAGS=\'{cflags}\' {" ".join(ctx.stm32cube_env)}'
            ctx.analyze_dir = f"{ctx.build_dir}/analyze"
            check_proc = ctx.run(f'CodeChecker check -b "{build_cmd}" -o {ctx.analyze_dir} ' \
                                 f'--config {u_utils.CODECHECKER_CFG_FILE} '
                                 '-d cppcheck-nullPointerRedundantCheck '
                                 '-d cppcheck-ignoredReturnValue '
                                 f'-i {u_utils.CODECHECKER_IGNORE_FILE}', warn=True)
            if check_proc.exited == 1:
                raise Exit("CodeChecker error")
            elif check_proc.exited >= 128:
                raise Exit("CodeChecker fatal error")
        else:
            build_cmd = f'make -j{jobs} UBXLIB_PATH={ctx.config.root_dir} OUTPUT_DIRECTORY={build_dir} ' \
                        f'CFLAGS=\"{cflags}\" {" ".join(ctx.stm32cube_env)}'
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
    """Flash a STM32 application"""
    build_dir = Path(build_dir, output_name).absolute().as_posix()
    cmds = OPENOCD_DISABLE_PORTS_CMDS
    if debugger_serial != "":
        cmds.append(f'adapter serial {debugger_serial}')
    cmds += [
        f'program {build_dir}/{file} reset',
        'exit'
    ]
    args = _to_openocd_args(cmds)
    ctx.run(f'openocd -f {u_utils.OPENOCD_CFG_DIR}/stm32{ctx.mcu[5:7]}.cfg {args}')

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
        cmds.append(f'adapter serial {debugger_serial}')
    cmds += [
        'init',
        f'stm32{ctx.mcu[5:7]}x.tpiu configure -output :{port}',
        f'stm32{ctx.mcu[5:7]}x.tpiu configure -protocol uart',
        f'stm32{ctx.mcu[5:7]}x.tpiu configure -formatter 0'
    ]
    # On Linux, the dollar that indicates to OpenOCD
    # "get this from the .cfg file" must be escaped,
    # while on Windows it must not
    if u_utils.is_linux():
        cmds += [f'stm32{ctx.mcu[5:7]}x.tpiu configure -traceclk \$_TARGET_SYSTEM_FREQUENCY']
    else:
        cmds += [f'stm32{ctx.mcu[5:7]}x.tpiu configure -traceclk $_TARGET_SYSTEM_FREQUENCY']
    if u_utils.is_linux():
        cmds += [f'stm32{ctx.mcu[5:7]}x.tpiu configure -pin-freq \$_TARGET_SWO_FREQUENCY']
    else:
        cmds += [f'stm32{ctx.mcu[5:7]}x.tpiu configure -pin-freq $_TARGET_SWO_FREQUENCY']
    cmds += [
        f'stm32{ctx.mcu[5:7]}x.tpiu enable',
        'itm port 0 on',
        'reset init',
        'resume'
    ]
    args = _to_openocd_args(cmds)
    promise = ctx.run(f'openocd -f {u_utils.OPENOCD_CFG_DIR}/stm32{ctx.mcu[5:7]}.cfg {args}', asynchronous=True)
    # Let OpenOCD start up first
    time.sleep(1)
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
        if u_utils.is_linux():
            promise.runner.kill()
        else:
            promise.runner.send_interrupt(KeyboardInterrupt())

@task(
    pre=[check_installation],
)
def parse_backtrace(ctx, elf_file, line):
    """Parse a backtrace line
    Example usage: inv stm32cube.parse-backtrace zephyr.elf "Backtrace:0x400ec4df:0x3ffbabb0 0x400df5a6:0x3ffbabd0"
    """
    task_utils.parse_backtrace(ctx, elf_file, line, toolchain_prefix=f"{ctx.arm_toolchain_path}/arm-none-eabi-")

@task(
    pre=[check_installation],
    help={
        "toolchain": f"ThreadX or FreeRTOS (default: {DEFAULT_TOOLCHAIN})",
        "makefile_dir": f"Makefile project directory to build (default: {DEFAULT_MAKEFILE_DIR})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
        "jobs": f"The number of Makefile jobs (default: {DEFAULT_JOB_COUNT})",
        "features": "Feature list, e.g. \"cell short_range\" to leave out gnss; overrides the environment variable UBXLIB_FEATURES and u_flags.yml"
    }
)
def analyze(ctx, toolchain=DEFAULT_TOOLCHAIN, makefile_dir=None, output_name=DEFAULT_OUTPUT_NAME,
            build_dir=DEFAULT_BUILD_DIR, jobs=DEFAULT_JOB_COUNT, u_flags=None, features=None):
    """Run CodeChecker static code analyzer (clang-analyze + clang-tidy)"""
    ctx.is_static_analyze = True

    makefile_dir = stm32_get_makefile_dir(ctx, makefile_dir, toolchain)
    build(ctx, toolchain, makefile_dir, output_name, build_dir, jobs, u_flags, features)

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
