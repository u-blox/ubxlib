# Shared functions
import re
import os
from glob import glob

def addr2line(ctx, elf_file, addresses, toolchain_prefix=""):
    """addr2line wrapper"""
    proc = ctx.run(f'{toolchain_prefix}addr2line -pfiaC -e {elf_file} {addresses}', hide=True, warn=True)
    if proc.ok:
        return proc.stdout
    else:
        return '?? ??:0'

def parse_backtrace(ctx, elf_file, line, indent=4, toolchain_prefix=""):
    """Parse a backtrace line
    Example of line: "Backtrace: 0x400ec4df:0x3ffbabb0 0x400df5a6:0x3ffbabd0"
    """
    line = line.lower().replace("backtrace:", "").strip()
    back_trace = addr2line(ctx, elf_file, line, toolchain_prefix).rstrip()
    # If this is a jenkins run we remove the path up until */ubxlib/
    # This is useful if you have copied the .elf file to another
    # machine and want vscode to understand the paths.
    back_trace = re.sub(r'at .*?jenkins.*?/ubxlib/', 'at ', back_trace)
    # Make some space between function name and file path so it's easier to read
    bt_lines = back_trace.split("\n")
    for l in bt_lines:
        s = l.rstrip().split(" at ")
        l = s[0]
        if len(s) > 1:
            l = f'{s[0]: <40} {s[1]}'
        print((" " * indent) + l)


def get_elf(build_dir, file_pattern="*.elf"):
    """Helper function for finding an ELF file in a build directory"""
    elf_file = None
    matches = glob(f'{build_dir}/**/{file_pattern}', recursive=True)
    if len(matches) > 0:
        elf_file = matches[0]
    else:
        print("Warning: Didn't find any ELF file")

    if elf_file is not None:
        print(f'Using ELF file: {elf_file}')

    return elf_file

def get_rtt_block_address(ctx, elf_file, toolchain_prefix, symbol_name="_SEGGER_RTT"):
    address = None
    if elf_file is not None:
        proc = ctx.run(f'{toolchain_prefix}readelf -s {elf_file}', hide=True, warn=True)
        if proc.ok:
            for item in proc.stdout.split("\n"):
                if item.endswith(" " + symbol_name):
                    address_str = re.findall(r": ([0-9a-fA-F]+) ", item)[0]
                    address = int(address_str, 16)
                    print(f"Found RTT segment address {hex(address)}")

    return address
