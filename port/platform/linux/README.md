# Introduction
These directories provide the implementation of the porting layer on native Linux.

# Limitations
Some limitations apply on this platform:

- Linux does not provide an implementation of critical sections, something which this code relies upon for cellular power saving (specifically, the process of waking up from cellular power-saving) hence cellular power saving cannot be used from a Linux build.
- On a Raspberry Pi (any flavour), the I2C HW implementation of the Broadcom chip does not correctly support clock stretching, which is required for u-blox GNSS devices, hence it is recommended that, if you are using I2C to talk to the GNSS device, you use the bit-bashing I2C drive to avoid data loss, e.g. by adding the line `dtoverlay=i2c-gpio,i2c_gpio_sda=2,i2c_gpio_scl=3,i2c_gpio_delay_us=2,bus=8` to `/boot/config.txt` and NOT uncommenting the `i2c_arm` line in the same file.
- Use of GPIO chips above 0 are supported but NOT when the pin in question is to be an output that must be set high at initialisation; this is because the `uPortGpioConfig()` call is what tells this code to use an index other than 0 and, to set an output pin high at initialisation, `uPortGpioSet()`, has to be called _before_ `uPortGpioConfig()`.
- All testing has been carried out on a 64-bit Raspberry Pi 4.