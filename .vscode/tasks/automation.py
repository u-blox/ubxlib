from invoke import task
from scripts.packages import u_package


@task()
def export(ctx):
    """Output the u_package environment"""
    pkg_cfg = u_package.get_u_packages_config(ctx)
    for pkg_name in pkg_cfg:
        print(f'export U_PKG_{pkg_name.upper()}={pkg_cfg[pkg_name]["package_dir"]}')


@task()
def install_all(ctx):
    """Makes sure all packages are installed"""
    pkg_names = []
    pkg_cfg = u_package.get_u_packages_config(ctx)
    for pkg_name in pkg_cfg:
        pkg_names.append(pkg_name)
    u_package.load(ctx, pkg_names)
