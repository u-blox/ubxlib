# Introduction
This folder contains the source code for the UDP portion of a `go` based echo server and the certificates for use against a secure TCP echo server.  The UDP echo server is based upon the AWS FreeRTOS TCP/secure echo server implementation which can be found here:

https://github.com/aws/amazon-freertos/tree/master/tools/echo_server

A UDP echo server using this `go` code, plus a TCP echo server and a secure TCP echo server using the AWS FreeRTOS `go` code and the certificates, are running on a publicly accessible server `ubxlib.it-sgn.u-blox.com`.

# Installation
The [README.md](https://github.com/aws/amazon-freertos/tree/master/tools/echo_server/README.md) at the above link was used to install TCP and secure TCP versions of the echo server.  The certificates generated for the secure TCP echo server can be found in the `certs` directory.  Then `echo-server.go` was copied and adapted to form `echo-server-udp.go`.

The running echo servers can be found at the following addresses:

- UDP:        `ubxlib.it-sgn.u-blox.com:5050`
- TCP:        `ubxlib.it-sgn.u-blox.com:5055`
- Secure TCP: `ubxlib.it-sgn.u-blox.com:5060`