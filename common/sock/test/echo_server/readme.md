# Introduction
This folder contains the source code for the UDP portion of a `go` based echo server and the certificates for use against a secure TCP echo server.  The UDP echo server is based upon the AWS FreeRTOS TCP/secure echo server implementation which can be found here:

https://github.com/aws/amazon-freertos/tree/master/tools/echo_server

A UDP echo server using this `go` code, plus a TCP echo server and a secure TCP echo server using the AWS FreeRTOS `go` code and the certificates, are running on a publicly accessible server `ubxlib.it-sgn.u-blox.com`.

The running echo servers can be found at the following addresses:

- UDP:        `ubxlib.it-sgn.u-blox.com:5050`
- TCP:        `ubxlib.it-sgn.u-blox.com:5055`
- Secure TCP: `ubxlib.it-sgn.u-blox.com:5060`

# Installation
The [README.md](https://github.com/aws/amazon-freertos/tree/main/tools/echo_server#readme) at the above link was used to install TCP and secure TCP versions of the echo server.  The certificates generated for the secure TCP echo server can be found in the [certs](certs) directory.  Then [echo-server.go](echo-server.go) was copied and adapted to form [echo-server-udp.go](echo-server-udp.go).

The files in this directory (and sub-directories) were built, installed and run \[using `nohup` on Linux\] as follows:

```
go build echo_server.go
nohup ./echo_server -config config.json >/dev/null 2>&1 &
nohup ./echo_server -config config_secure.json >/dev/null 2>&1 &

go build echo_server_udp.go
nohup ./echo_server_udp -config config_udp.json >/dev/null 2>&1 &
```