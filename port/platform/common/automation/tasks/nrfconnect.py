import os
import shutil
import sys
import json
import re
from pathlib import Path

from invoke import task, Exit
from tasks import task_utils
from scripts import u_utils
from scripts.u_log_readers import URttReader
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package, u_pkg_utils

DEFAULT_CMAKE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/zephyr/runner"
DEFAULT_BOARD_NAME = "nrf5340dk_nrf5340_cpuapp"
DEFAULT_OUTPUT_NAME = f"runner_{DEFAULT_BOARD_NAME}"
DEFAULT_BUILD_DIR = os.path.join("_build","nrfconnect")
DEFAULT_MCU = "NRF5340_XXAA_APP"

def posix_path(path):
    path = Path(path).expanduser().as_posix()
    if not u_pkg_utils.is_linux():
        path = path.lower()
    return path

def filter_compile_commands(input_json_file='compile_commands.json',
                            output_json_file='filtered_commands.json',
                            include_dirs=[],
                            exclude_patterns=[]):
    # Load input json file
    with open(input_json_file, encoding='utf8') as f:
        commands = json.load(f)

    # Convert and expand to posix path so the path format is the same
    # as we will use for the compile commands
    include_dirs = [ posix_path(dir) for dir in include_dirs ]
    # Windows needs some special treatments
    if not u_pkg_utils.is_linux():
        # FS in Windows is case insensitive
        exclude_patterns = [ pattern.lower().replace("\\", "/") for pattern in exclude_patterns ]
    filtered_commands = []
    for cmd in commands:
        # Convert all paths to posix
        cmd["file"] = posix_path(cmd["file"])
        cmd["directory"] = posix_path(cmd["directory"])
        # Windows needs some more special treatments
        if not u_pkg_utils.is_linux():
            for match in re.finditer(r"([a-zA-Z]:\\[^ ]*)", cmd["command"]):
                cmd["command"] = cmd["command"].replace(match[0], match[0].replace("\\", "/"))
        file_path = cmd["file"]
        included = any(file_path.startswith(dir) for dir in include_dirs)
        excluded = any(pattern in file_path for pattern in exclude_patterns)
        if included and not excluded:
            # When -nostdinc is specified clang doesn't function correctly
            # but removing seems to succeed...
            cmd["command"] = cmd["command"].replace("-nostdinc", "")
            filtered_commands.append(cmd)

    # Write output json file
    with open(output_json_file, 'w', encoding='utf8') as outfile:
        json.dump(filtered_commands, outfile)

@task()
def check_installation(ctx):
    """Check that the toolchain for nRF connect SDK is installed"""
    ctx.zephyr_pre_command = ""

    # Load required packages
    pkgs = u_package.load(ctx, ["arm_embedded_gcc", "nrfconnectsdk", "make", "nrf_cli", "segger_jlink"])
    ncs_pkg = pkgs["nrfconnectsdk"]
    ae_gcc_pkg = pkgs["arm_embedded_gcc"]

    if not u_utils.is_linux():
        # The Zephyr related env variables will be setup by <toolchain>/cmd/env.cmd
        ctx.zephyr_pre_command = f"{ncs_pkg.get_windows_toolchain_path()}/cmd/env.cmd & "
    else:
        ctx.config.run.env["ZEPHYR_BASE"] = f'{ncs_pkg.get_install_path()}/zephyr'
        ctx.config.run.env["ZEPHYR_TOOLCHAIN_VARIANT"] = 'gnuarmemb'
        ctx.config.run.env["GNUARMEMB_TOOLCHAIN_PATH"] = ae_gcc_pkg.get_install_path()
    ctx.arm_toolchain_path = ae_gcc_pkg.get_install_path() + "/bin"

@task(
    pre=[check_installation],
    help={
        "cmake_dir": f"CMake project directory to build (default: {DEFAULT_CMAKE_DIR})",
        "board_name": f"Zephyr board name (default: {DEFAULT_BOARD_NAME})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)"
    }
)
def build(ctx, cmake_dir=DEFAULT_CMAKE_DIR, board_name=DEFAULT_BOARD_NAME,
          output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR,
          u_flags=None):
    """Build a nRF connect SDK based application"""
    pristine = "auto"

    # Handle u_flags
    if u_flags:
        ctx.config.run.env["U_FLAGS"] = u_flags_to_cflags(u_flags)
    else:
        # Read U_FLAGS from nrfconnect.u_flags
        u_flags = get_cflags_from_u_flags_yml(ctx.config.vscode_dir, "nrfconnect", output_name)
        ctx.config.run.env["U_FLAGS"] = u_flags["cflags"]
        # If the flags has been modified we trigger a rebuild
        if u_flags['modified']:
            pristine = "always"

    build_dir = os.path.join(build_dir, output_name)
    ctx.run(f'{ctx.zephyr_pre_command}west build -p {pristine} -b {board_name} {cmake_dir} --build-dir {build_dir}')

@task(
    pre=[check_installation],
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files for a nRF connect SDK build"""
    build_dir = os.path.join(build_dir, output_name)
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

@task(
    pre=[check_installation],
    help={
        "debugger_serial": "The debugger serial number (optional)",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "hex_file": "Optional: Specify the hex file to flash manually"
    }
)
def flash(ctx, debugger_serial="", output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, hex_file=None):
    """Flash a nRF connect SDK based application"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if debugger_serial != "":
        debugger_serial = f"--snr {debugger_serial}"
    hex_arg = "" if hex_file == None else f"--hex-file {hex_file}"
    ctx.run(f'{ctx.zephyr_pre_command}west flash --skip-rebuild {hex_arg} -d {build_dir} {debugger_serial} --erase --recover')

@task(
    pre=[check_installation],
    help={
        "mcu": f"The MCU name (The JLink target name) (default: {DEFAULT_MCU})",
        "debugger_serial": "The debugger serial number (optional)",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}). This is used for finding ELF file",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR}). This is used for finding ELF file",
        "elf_file": f"The .elf file used for decoding backtrace. When this is used --build-dir and --output-name has no function.",
        "reset": "When set the target device will reset on connection."
    }
)
def log(ctx, mcu=DEFAULT_MCU, debugger_serial=None,
        output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR,
        elf_file=None, reset=True):
    """Open a log terminal"""
    if debugger_serial == "":
        debugger_serial = None

    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if elf_file is None:
        elf_file = task_utils.get_elf(build_dir, "zephyr/zephyr.elf")
    block_address = task_utils.get_rtt_block_address(ctx, elf_file,
                                                     toolchain_prefix=f"{ctx.arm_toolchain_path}/arm-none-eabi-")

    with URttReader(mcu, jlink_serial=debugger_serial, reset_on_connect=reset, rtt_block_address=block_address) as rtt_reader:
        line = ""
        while True:
            data = rtt_reader.read()
            if data:
                line += bytearray(data).decode(sys.stdout.encoding, "backslashreplace").replace("\r", "")
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

@task(
    pre=[check_installation],
)
def terminal(ctx):
    """Open a nRFconnect SDK terminal"""
    ctx.run(f'{ctx.zephyr_pre_command}{ctx.config.run.shell}', pty=True)

@task(
    pre=[check_installation],
)
def parse_backtrace(ctx, elf_file, line):
    """Parse an nRFconnect backtrace line
    Example usage: inv nrfconnect.parse-backtrace zephyr.elf "Backtrace:0x400ec4df:0x3ffbabb0 0x400df5a6:0x3ffbabd0"
    """
    task_utils.parse_backtrace(ctx, elf_file, line, toolchain_prefix=f"{ctx.arm_toolchain_path}/arm-none-eabi-")

@task(
    pre=[check_installation],
    help={
        "cmake_dir": f"CMake project directory to build (default: {DEFAULT_CMAKE_DIR})",
        "board_name": f"Zephyr board name (default: {DEFAULT_BOARD_NAME})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)"
    }
)
def analyze(ctx, cmake_dir=DEFAULT_CMAKE_DIR, board_name=DEFAULT_BOARD_NAME,
            output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR,
            u_flags=None):
    """Run CodeChecker static code analyzer (clang-analyze + clang-tidy)"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    os.makedirs(build_dir, exist_ok=True)

    if u_flags:
        ctx.config.run.env["U_FLAGS"] = u_flags_to_cflags(u_flags)

    # Start by getting the compile commands by using the CMAKE_EXPORT_COMPILE_COMMANDS setting
    # This will generate compile_commands.json
    with ctx.prefix(u_pkg_utils.change_dir_prefix(build_dir)):
        ctx.run(f'{ctx.zephyr_pre_command}cmake -DBOARD={board_name} -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja {cmake_dir}')

    # Now when we got all compile commands we filter out only the relevant (i.e. commands for compiling ubxlib source)
    filter_compile_commands(input_json_file=f"{build_dir}/compile_commands.json",
                            output_json_file=f"{build_dir}/filtered_commands.json",
                            include_dirs=[u_utils.UBXLIB_DIR],
                            exclude_patterns=[build_dir])

    with ctx.prefix(u_pkg_utils.change_dir_prefix(build_dir)):
        # Need to first build zephyr lib so that we get the generated headers
        ctx.run(f'{ctx.zephyr_pre_command}ninja zephyr')
        # Do the analyze
        analyze_proc = ctx.run(f'CodeChecker analyze filtered_commands.json -o ./analyze ' \
                               f'--config {u_utils.CODECHECKER_CFG_FILE} -i {u_utils.CODECHECKER_IGNORE_FILE}', warn=True)
        # Regardless if there is an error in previous step we try to convert the analyze result to HTML
        parse_proc = ctx.run(f'CodeChecker parse -e html ./analyze -o ./analyze_html', warn=True, hide=u_utils.is_automation())
        # Now check the return codes
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

        if not analyze_proc.ok:
            raise Exit(f"CodeChecker analyze returned an error")
