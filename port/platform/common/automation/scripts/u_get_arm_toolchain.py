'''This script is used by vscode to get the ARM toolchain path from u_packages.'''
import sys
import os
from invoke import context

from scripts.packages import u_package

def main():
    org_stdout = sys.stdout
    # Discard all output during u_package.load()
    with open(os.devnull, 'w', encoding='utf-8') as null:
        sys.stdout = null
        ctx = context.Context()
        pkgs = u_package.load(ctx, ["arm_embedded_gcc"])
        # Turn on stdout again
        sys.stdout = org_stdout
    print(pkgs["arm_embedded_gcc"].get_install_path() + "/bin")

if __name__ == '__main__':
    main()
