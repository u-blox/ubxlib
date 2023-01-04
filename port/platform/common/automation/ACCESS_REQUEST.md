# Introduction
u-blox employees may request access to the `ubxlib` test system as follows.

# Instructions
Access is secured through public/private keys, hence it will be per device/machine.  After following the instructions below, access will be seamless from that machine, just as if you were accessing a machine on the internal network.

## Arranging HTTPS Access For A Linux Machine
Please generate a key and a Certificate Signing Request using OpenSSL with:

```
openssl genrsa -des3 -out ubxlib_test_system.key 4096
openssl req -new -key ubxlib_test_system.key -out ubxlib_test_system_devicename.csr
```

...where the key file should be password protected and `devicename` is replaced with something that uniquely identifies that particular device (e.g. in my case it would be `gb-cmb-lt-rmea`), entering that same string in the `Organisation Name` field of the CSR and also populating the `E-mail Address` field of the CSR with your u-blox e-mail address; everything else in there can be left blank by just pressing `.` and then `<enter>`.

Keep the `.key` file safely somewhere (do not reveal it to anyone) and send the file `ubxlib_test_system_devicename.csr` to [ubxlib@u-blox.com](mailto:ubxlib@u-blox.com).

You will get back a signed certificate, valid for 365 days, plus the CA certificate for the `ubxlib` test system, along with instructions on how to load those into Firefox.

## Arranging HTTPS Access For A Windows Machine
If you can, [install OpenSSL](https://wiki.openssl.org/index.php/Binaries) and do the same as above.  If you do not want to install OpenSSL, just e-mail the `devicename` of the machine to [ubxlib@u-blox.com](mailto:ubxlib@u-blox.com).  You will get back a `.pfx` file that you can install in Windows, allowing you to use any Windows browser.

## Arranging SSH/SFTP Access
On rare occasions you may also need to SSH into the `ubxlib` test system or SFTP files from/to it.  For this, please generate a public/private SSH key pair with:

```
ssh-keygen -f path/to/ubxlib_test_system_devicename_key -t ecdsa -b 521
```

...where `path/to/` is `~/.ssh/` for Linux or `%homedrive%%homepath%\.ssh\` for Windows and `devicename` is replaced just like above; please password protect the key file.

E-mail `ubxlib_test_system_client_key.pub` (NOT the non-`.pub` file, that must never leave the client machine) to [ubxlib@u-blox.com](mailto:ubxlib@u-blox.com) and the client machine will be granted SSH/SFTP access.