import os
import shutil
import sys
import json
import re
from pathlib import Path
from invoke import task, Exit
from tasks import task_utils
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags, get_cflags_from_u_flags_yml
from scripts.packages import u_package, u_pkg_utils
from scripts.u_log_readers import UUartReader

DEFAULT_CMAKE_BASE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/zephyr/runner"
DEFAULT_BOARD_NAME = "nrf5340dk_nrf5340_cpuapp"
DEFAULT_OUTPUT_NAME = f"runner"
DEFAULT_BUILD_DIR = os.path.join("_build", "zephyr_native")

BOARD_NAME_TO_MCU = {
    "nucleo_f767zi": "stm32",
    "nucleo_u575zi_q": "stm32"
}

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

def get_mcu(board_name):
    """Get the MCU name for the given board_name"""
    mcu = BOARD_NAME_TO_MCU.get(board_name)
    if not mcu:
        print(f"Board {board_name} not found in list of known board names in automation script zephyr_native.py")

    return mcu

@task()
def check_installation(ctx):
    """Check that the toolchain for Zephyr is installed"""
    ctx.zephyr_pre_command = ""

    # Load required packages
    pkgs = u_package.load(ctx, ["arm_embedded_gcc", "zephyr_native", "ninja", "cmake", "gperf", "unity", "openocd"])
    zephyr_native_pkg = pkgs["zephyr_native"]
    ae_gcc_pkg = pkgs["arm_embedded_gcc"]
    unity_pkg = pkgs["unity"]

    ctx.config.run.env["ZEPHYR_BASE"] = f'{zephyr_native_pkg.get_install_path()}/zephyr'
    ctx.config.run.env["ZEPHYR_TOOLCHAIN_VARIANT"] = 'gnuarmemb'
    ctx.config.run.env["GNUARMEMB_TOOLCHAIN_PATH"] = ae_gcc_pkg.get_install_path()
    ctx.arm_toolchain_path = ae_gcc_pkg.get_install_path() + "/bin"
    ctx.config.run.env["UNITY_PATH"] = f'{unity_pkg.get_install_path()}'

@task(
    pre=[check_installation],
    help={
        "board_name": f"Zephyr board name (must be provided)",
        "cmake_dir": f"CMake project directory to build; if not specified cmake_base_dir will apply",
        "cmake_base_dir": f"The base CMake project directory, used if cmake_dir is not provided (default: {DEFAULT_CMAKE_BASE_DIR}); to this will be appended an underscore and then the MCU name for the board",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
        "features": "Feature list, e.g. \"cell short_range\" to leave out gnss; overrides the environment variable UBXLIB_FEATURES and u_flags.yml"
    }
)
def build(ctx, board_name, cmake_dir=None, cmake_base_dir=DEFAULT_CMAKE_BASE_DIR,
          output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, u_flags=None, features=None):
    """Build a Zephyr based application"""
    pristine = "auto"
    mcu = get_mcu(board_name)
    if not cmake_dir:
        cmake_dir = cmake_base_dir + "_" + mcu

    # Read U_FLAGS and features from zephyr_native.u_flags, if it is there  TODO
    u_flags_yml = get_cflags_from_u_flags_yml(ctx.config.vscode_dir, "zephyr_native", output_name)
    if u_flags_yml:
        ctx.config.run.env["U_FLAGS"] = u_flags_yml["cflags"]
        if not features and "features" in u_flags_yml:
            features = u_flags_yml["features"]
        # If the flags have been modified we trigger a rebuild
        if u_flags_yml['modified']:
            pristine = "always"

    # Let any passed-in u_flags override the .yml file
    if u_flags:
        ctx.config.run.env["U_FLAGS"] = u_flags_to_cflags(u_flags)

    build_dir = os.path.join(build_dir, output_name)
    west_cmd = f'{ctx.zephyr_pre_command}west build -p {pristine} -b {board_name} {cmake_dir} --build-dir {build_dir}'
    # Add UBXLIB_FEATURES from the parameter passed-in or the environment
    if features:
        os.environ["UBXLIB_FEATURES"] = features
    if "UBXLIB_FEATURES" in os.environ:
        west_cmd += f' -- -DUBXLIB_FEATURES={os.environ["UBXLIB_FEATURES"].replace(" ", ";")}'
        if u_utils.is_linux():
            # A semicolon is a special character on Linux
            west_cmd = west_cmd.replace(";", "\\;")
    ctx.run(west_cmd)
    # This specific case gives a return code so that automation.py can call it directly for instance 8
    return 0 # TODO

@task(
    pre=[check_installation],
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files for a Zephyr build"""
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
def flash(ctx, board_name, debugger_serial="", output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR):
    """Flash a Zephyr-based application"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if debugger_serial != "":
        debugger_serial = f"--serial {debugger_serial}"
    ctx.run(f'{ctx.zephyr_pre_command}west flash --skip-rebuild -d {build_dir} --runner openocd {debugger_serial}', hide=False)

@task(
    pre=[check_installation],
    help={
        "board_name": f"The board name (must be provided)",
        "debugger_serial": "The debugger serial number (optional)",
        "port": "The local TCP port to use for OpenOCD SWO output",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}). This is used for finding ELF file",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR}). This is used for finding ELF file",
        "elf_file": f"The .elf file used for decoding backtrace. When this is used --build-dir and --output-name has no function.",
        "reset": "When set the target device will reset on connection."
    }
)
def log(ctx, board_name, serial_port, baudrate=115200,
        output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR,
        elf_file=None):
    """Open a log terminal"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    if elf_file is None:
        elf_file = task_utils.get_elf(build_dir, "zephyr/zephyr.elf")

    with UUartReader(port=serial_port, baudrate=baudrate) as serial:
        line = ""
        while True:
            data = serial.read()
            if data:
                line += data.decode(sys.stdout.encoding, "backslashreplace").replace("\r", "")
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
    """Open a Zephyr terminal"""
    ctx.run(f'{ctx.zephyr_pre_command}{ctx.config.run.shell}', pty=True)

@task(
    pre=[check_installation],
)
def parse_backtrace(ctx, elf_file, line):
    """Parse a Zephyr backtrace line
    Example usage: inv zephyr_native.parse-backtrace zephyr.elf "Backtrace:0x400ec4df:0x3ffbabb0 0x400df5a6:0x3ffbabd0"
    """
    task_utils.parse_backtrace(ctx, elf_file, line, toolchain_prefix=f"{ctx.arm_toolchain_path}/arm-none-eabi-")

@task(
    pre=[check_installation],
    help={
        "toolchain": "Included only for compatibility with other analyzers, ignored in this case",
        "board_name": f"Zephyr board name (default: {DEFAULT_BOARD_NAME})",
        "cmake_dir": f"CMake project directory to build; if not specified cmake_base_dir will apply",
        "cmake_base_dir": f"The base CMake project directory, used if cmake_dir is not provided (default: {DEFAULT_CMAKE_BASE_DIR}); to this will be appended an underscore and then the MCU name for the board",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "Extra u_flags (when this is specified u_flags.yml will not be used)",
        "features": "Feature list, e.g. \"cell short_range\" to leave out gnss; overrides the environment variable UBXLIB_FEATURES"
    }
)
def analyze(ctx, toolchain="", board_name=DEFAULT_BOARD_NAME, cmake_dir=None,
            cmake_base_dir=DEFAULT_CMAKE_BASE_DIR,
            output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR,
            u_flags=None, features=None):
    """Run CodeChecker static code analyzer (clang-analyze + clang-tidy)"""
    mcu = get_mcu(board_name)
    if not cmake_dir:
        cmake_dir = cmake_base_dir + "_" + mcu

    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    features_string = ""
    os.makedirs(build_dir, exist_ok=True)

    if u_flags:
        ctx.config.run.env["U_FLAGS"] = u_flags_to_cflags(u_flags)

    if features:
        os.environ["UBXLIB_FEATURES"] = features
    if "UBXLIB_FEATURES" in os.environ:
        features_string = f'-DUBXLIB_FEATURES={os.environ["UBXLIB_FEATURES"].replace(" ", ";")}'

    # Start by getting the compile commands by using the CMAKE_EXPORT_COMPILE_COMMANDS setting
    # This will generate compile_commands.json
    with ctx.prefix(u_pkg_utils.change_dir_prefix(build_dir)):
        ctx.run(f'{ctx.zephyr_pre_command}cmake -DBOARD={board_name} {features_string} -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -G Ninja {cmake_dir}')

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
