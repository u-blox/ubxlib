# Introduction
This folder contains the source code for the UDP portion of a `go` based echo server and the associated certificates generated for use against a publicly accessible installation at `ciot.it-sgn.u-blox.com`.  It is based up on the AWS FreeRTOS TCP/secure portion which can be found here:

https://github.com/aws/amazon-freertos/tree/master/tools/echo_server

The UDP and non-secure TCP echo servers are run as well as the secure TCP echo server.

# Installation
The [README.md](https://github.com/aws/amazon-freertos/tree/master/tools/echo_server/README.md) at the above link was used to install TCP and secure TCP versions of the echo server.  The certificates generated for the secure TCP echo server can be dound in the `certs` directory.  Then `echo-server.go` was copied and adapted to form `echo-server-udp.go`.

The running echo servers can be found at the following addresses:

UDP:        `ciot.it-sgn.u-blox.com:5050`
TCP:        `ciot.it-sgn.u-blox.com:5055`
Secure TCP: `ciot.it-sgn.u-blox.com:5060`