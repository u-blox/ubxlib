import os
import shutil
import sys
from glob import glob
from invoke import task
from scripts import u_utils
from scripts.u_flags import u_flags_to_cflags
from scripts.packages import u_package
from scripts.u_log_readers import UUartReader

# Supported versions of some things: you can find them at
# https://registry.platformio.org/search
# Note: PlatformIO works best if you pick a version at
# the platform level and let it determine what version of
# framework etc. goes-with.  If you specify a version of
# a framework it does NOT do the reverse and figure out
# the right version of platform/tools/etc. to go with it
VERSION_LIST = {
  "espressif32": "~4.4.0",
  "nordicnrf52": "~9.2.0"
}

PLATFORMIO_ENV_NAME = "ubxlib_test"
CMAKE_MIN_VERSION_STRING = "3.13.1"
ARDUINO_DIR = f"{u_utils.PLATFORM_DIR}/arduino"
ESPRESSIF_PLATFORM_SUBDIR = "port/platform/esp-idf/mcu/esp32"
ESPRESSIF_RUNNER_SDKCONFIG_DEFAULTS_PATH = f"{u_utils.UBXLIB_DIR}/{ESPRESSIF_PLATFORM_SUBDIR}/runner/sdkconfig.defaults"

# The path where the PlatformIO stuff is installed by
# PlatformIO itself
PLATFORMIO_PATH = os.path.join(os.path.expanduser("~"), ".platformio")

# The path to the GCC ARM tools as installed by PlatformIO itself
PLATFORMIO_ARM_NONE_EABI_PATH  = os.path.join(PLATFORMIO_PATH, "packages", "toolchain-gccarmnoneeabi")

# Lord knows why but the path to PIO beneath the home
# directory is different on Linux and Windows
PIO_PATH = os.path.join(PLATFORMIO_PATH, "penv")
if u_utils.is_linux():
    PIO_PATH = os.path.join(PIO_PATH, "bin", "pio")
else:
    PIO_PATH = os.path.join(PIO_PATH, "Scripts", "pio")

DEFAULT_OUTPUT_NAME = "runner_platformio"
DEFAULT_BUILD_DIR = "_build/platformio"
DEFAULT_PLATFORM = None
DEFAULT_FRAMEWORK = "zephyr"
DEFAULT_BOARD_NAME = "nrf52840_dk"

def get_platform(board):
    '''Determine the platform from the name of the board'''
    platform = None
    print_string = f"Board {board} implies"

    if board.lower().startswith("nrf52"):
        platform = "nordicnrf52"
    elif board.lower().startswith("esp") or board.lower() == "nina_w10":
        platform = "espressif32"

    if platform:
        print_string += f" platform \"{platform}\"."
    else:
        print_string += f" no known platform."

    print(print_string)

    return platform

def create_zephyr_cmakelists(build_dir, main_path):
    '''Create the CMakeLists.txt file for a Zephyr build that brings in the ubxlib module'''
    print("Creating CMakeLists.txt for Zephyr build.")
    lib_path = build_dir.replace('\\','/') + f"/.pio/libdeps/{PLATFORMIO_ENV_NAME}/ubxlib"
    with open(os.path.join(build_dir, "zephyr", "CMakeLists.txt"), "w") as cmakelists:
        cmakelists.write(f"set(ZEPHYR_EXTRA_MODULES \"{lib_path}\")\n")
        cmakelists.write(f"cmake_minimum_required(VERSION {CMAKE_MIN_VERSION_STRING})\n")
        cmakelists.write("include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)\n")
        cmakelists.write(f"project({PLATFORMIO_ENV_NAME})\n")
        cmakelists.write(f"target_sources(app PRIVATE ../src/{os.path.basename(main_path)})\n")

def remove_line_containing_from_file(file_path, text):
    '''Remove any line containing text from file_path'''
    lines_out = []
    with (open(file_path, "r")) as file:
        lines_in = file.readlines()
        for line in lines_in:
            if not text in line:
                lines_out.append(line)
    if lines_out:
        with (open(file_path, "w")) as file:
            for line in lines_out:
                file.write(line)

def do_platform(ctx, pkgs, build_dir, platform, framework, main_path):
    '''Do platform/framework specific stuff'''
    platform_package_list = []
    framework_version = ""
    platform_version = ""

    if platform in VERSION_LIST:
        platform_version = VERSION_LIST[platform]
        print(f"Platform {platform} will be version {platform_version}.")

    if platform == "espressif32":
        # For Espressif we need the sdkconfig.defaults from the normal
        # ESP-IDF build
        os.makedirs(build_dir, exist_ok=True)
        shutil.copy(ESPRESSIF_RUNNER_SDKCONFIG_DEFAULTS_PATH, build_dir)
        if framework == "espidf":
            # For ESP-IDF  we _could_ find the framework version from
            # u_packages.yml and apply that here.  However, there is
            # a slight risk of incompatibility as Platform IO can't
            # work backwards to the Espressif platform version from the
            # espidf framework version, so it is better to pick a
            # satisfactory version for espressif32 and leave it at that
            # Find a satisfactory version by going to
            # https://registry.platformio.org/search, finding the
            # platform you want, clicking on the versions link and
            # then clicking through the release notes to see when things
            # got updated.
            pass
        elif framework == "arduino":
            # For Arduino u_packages.yml is of no help since it only specifies
            # the version of arduino_cli, so we go with the flow again
            # When testing with Arduino we need an [empty] header file
            # named ubxlib_test.h
            tmp = os.path.join(build_dir, "include");
            os.makedirs(tmp, exist_ok=True )
            with open(os.path.join(tmp, "ubxlib_test.h"), "w") as header_file:
                header_file.write("// Deliberately empty header file")
    elif framework == "zephyr":
        # The Zephyr version is implied by the nRF Connect SDK but picking
        # a specific version without specifying a version of the nordicnrf52
        # platform leads to compilation errors, so we just let the version
        # be implied: e.g. nordicnrf52 version 9.2.0 will bring Zephyr
        # version 2.7.1
        # We also need the prj.conf file that we use for testing; this should go into
        # the zephyr sub-directory of the build
        src_path = os.path.join(u_utils.PLATFORM_DIR, "zephyr", "runner", "prj.conf")
        dst_path = os.path.join(build_dir, "zephyr")
        os.makedirs(dst_path, exist_ok=True)
        shutil.copy(src_path, dst_path)
        # For reasons we do not understand, when building under Platform IO,
        # if CONFIG_USERSPACE is on then the Zephyr boilerpate CMakeLists.txt
        # file does not generate a required file, linker-kobject-prebuilt-rodata.h,
        # and compilation fails, hence we remove `CONFIG_USERSPACE=y` here
        # to leave it at the default of "off"
        remove_line_containing_from_file(os.path.join(dst_path, "prj.conf"), "CONFIG_USERSPACE")
        lines_out = []
        dst_path = os.path.join(dst_path, "prj.conf")
        with (open(dst_path, "r")) as prj_file:
            lines_in = prj_file.readlines()
            for line in lines_in:
                if not "CONFIG_USERSPACE" in line:
                    lines_out.append(line)
        if lines_out:
            with (open(dst_path, "w")) as prj_file:
                for line in lines_out:
                    prj_file.write(line)

        # And finally we need a CMakeLists.txt that brings in the ubxlib
        # library as a Zephyr module
        create_zephyr_cmakelists(build_dir, main_path)

    return platform_version, platform_package_list

def get_main_path(framework):
    '''Return the full path to the file containing main()'''
    main_path = None

    if framework == "espidf":
        main_path = os.path.join(u_utils.PLATFORM_DIR, "esp-idf", "app", "u_main.c")
    elif framework == "zephyr":
        main_path = os.path.join(u_utils.PLATFORM_DIR, "zephyr", "app", "u_main.c")
    elif framework == "arduino":
        main_path = os.path.join(u_utils.PLATFORM_DIR, "arduino", "app", "app.ino")

    return main_path

@task()
def check_installation(ctx):
    """Check PIO installation"""
    pkgs = u_package.load(ctx, ["platformio"])

    # This is needed by the Zephyr/NRFConnect backtracing stuff
    ctx.arm_toolchain_path = os.path.join(PLATFORMIO_ARM_NONE_EABI_PATH, "bin")

    # Check it's there
    ctx.run(f"{PIO_PATH} system info")

@task(
    pre=[check_installation],
    help={
        "platform": f"The platform, leave blank to imply from board (default: {DEFAULT_PLATFORM})",
        "board": f"The Arduino board (default: {DEFAULT_BOARD_NAME})",
        "framework": f"The framework (default: {DEFAULT_FRAMEWORK})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME})",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})",
        "u_flags": "build flags, separated with spaces",
    }
)
def build(ctx, platform=DEFAULT_PLATFORM,
          board=DEFAULT_BOARD_NAME,
          framework=DEFAULT_FRAMEWORK,
          output_name=DEFAULT_OUTPUT_NAME,
          build_dir=DEFAULT_BUILD_DIR, u_flags=None):
    """Build with Platform IO"""
    pkgs = u_package.get_u_packages_config(ctx)
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    main_path = get_main_path(framework)
    add_unity = True
    build_flags = None
    platformio_ini_lines = []

    # If no platform is passed in, find it based on the board
    if not platform:
        platform = get_platform(board)

    # Do any platform/framework-specific stuff and, while doing so, get
    # the platform version, if required, and any PIO platform package
    # manager strings that need to be fed to platform.ini
    platform_version, platform_package_list = do_platform(ctx, pkgs, build_dir,
                                                          platform, framework,
                                                          main_path)

    # Turn u_flags into an array of build flags
    if u_flags:
        build_flags = u_flags_to_cflags(u_flags).split()

    # If we're on Espressif then unity is already included and we'll
    # get a conflict if we try to include it again
    if platform == "espressif32":
        add_unity = False

    # Now create the platform.ini file
    platformio_ini_lines.append(f"[env:{PLATFORMIO_ENV_NAME}]")
    if platform_version:
        platform_version = "@" + platform_version
    platformio_ini_lines.append(f"platform = {platform}{platform_version}")
    platformio_ini_lines.append(f"board = {board}")
    platformio_ini_lines.append(f"framework = {framework}")
    tmp = f"lib_deps = {u_utils.UBXLIB_DIR}"
    if add_unity:
        platformio_ini_lines.append("test_framework = unity")
        tmp = tmp + ", unity"
    platformio_ini_lines.append(tmp)
    # Set this so that the test functions will not be put into
    # a separate archive and hence the constructors which form
    # their entry points will be called
    platformio_ini_lines.append("lib_archive = no")
    if platform_package_list:
        platformio_ini_lines.append("platform_packages = ")
        for platform_package in platform_package_list:
            platformio_ini_lines.append(f"    {platform_package}")
    if build_flags:
        tmp = "build_flags = "
        need_separator = False
        for build_flag in build_flags:
            if need_separator:
                tmp += " "
            tmp += f"'{build_flag}'"
            need_separator = True
        platformio_ini_lines.append(f"{tmp}")
    os.makedirs(build_dir, exist_ok=True )
    with open(os.path.join(build_dir, "platformio.ini"), "w") as platformio_ini_file:
        print("platformio.ini will be:")
        for line in platformio_ini_lines:
            print(f"  {line}")
            platformio_ini_file.write(line + "\n")

    # Copy the file containing main() into the src directory of the build
    tmp = os.path.join(build_dir, "src")
    os.makedirs(tmp, exist_ok=True)
    shutil.copy(main_path, tmp)

    # Now call PIO run from the build directory
    with u_utils.ChangeDir(build_dir):
        # Uncomment the following line to do a system prune, removing
        # unnecessary PIO stuff to retrieve disk space
        # ctx.run(f"{PIO_PATH} system prune --force")
        print(f"Calling Platform IO in directory {build_dir}; this might take"  \
              " a while if there are packages to download.")
        pio = f"{PIO_PATH} run"
        ctx.run(f"{pio} --target cleanall")
        ctx.run(f"{pio}")

@task(
    help={
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def clean(ctx, output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Remove all files"""
    build_dir = os.path.join(build_dir, output_name)
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

@task(
    pre=[check_installation],
    help={
        "serial_port": "The serial port connected to the device",
        "board": f"The board (default: {DEFAULT_BOARD_NAME})",
        "output_name": f"An output name (build sub folder, default: {DEFAULT_OUTPUT_NAME}",
        "build_dir": f"Output build directory (default: {DEFAULT_BUILD_DIR})"
    }
)
def flash(ctx, serial_port, board=DEFAULT_BOARD_NAME,
          output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Flash an application"""
    # Uncomment the following line to do a system prune, removing unnecessary
    # stuff to get back disk space
    # ctx.run(f"{PIO_PATH} system prune --force")
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    with u_utils.ChangeDir(build_dir):
        # When running under automation we may now be on a different machine to
        # where the build was carried out and hence the lib_deps line in platformio.ini,
        # pointing to ubxlib, may specify a path that doesn't exist.
        # Since we don't need lib_deps for flashing, we can remove it
        remove_line_containing_from_file("platformio.ini", "lib_deps")
        ctx.run(f"{PIO_PATH} run --disable-auto-clean --upload-port {serial_port} -v -t nobuild -t upload")

@task(
    pre=[check_installation],
)
def log(ctx, serial_port, baudrate=115200,
        output_name=DEFAULT_OUTPUT_NAME, build_dir=DEFAULT_BUILD_DIR):
    """Open a log terminal"""
    build_dir = os.path.abspath(os.path.join(build_dir, output_name))
    with u_utils.ChangeDir(build_dir):
        ctx.run(f"{PIO_PATH} device monitor --port {serial_port} --baud {baudrate}")

