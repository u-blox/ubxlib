This is the API for GNSS.

It also contains two Python scripts:
- [u_gnss_ucenter_ubx.py](u_gnss_ucenter_ubx.py): you can give this script the log output from `ubxlib` and it will find the UBX messages in it and write them to a file (or on Linux a PTY) which you can open in the u-blox [uCenter tool](https://www.u-blox.com/en/product/u-center).
- [u_gnss_cfg_val_key.py](u_gnss_cfg_val_key.py): this script should be executed if the enums in [u_gnss_cfg_val_key.h](u_gnss_cfg_val_key.h) have been updated; it will re-write the header file to include a set of key ID macros that can be used by the application.