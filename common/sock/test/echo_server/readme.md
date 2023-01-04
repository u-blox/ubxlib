# Introduction
This folder contains the source code for the UDP portion of a `go` based echo server and the certificates for use against a secure TCP echo server.  The UDP echo server is based upon the AWS FreeRTOS TCP/secure echo server implementation which can be found here:

https://github.com/aws/amazon-freertos/tree/master/tools/echo_server

A UDP echo server using this `go` code, plus a TCP echo server and a secure TCP echo server using the AWS FreeRTOS `go` code and the certificates, are running on a publicly accessible server `ubxlib.redirectme.net`.

The running echo servers can be found at the following addresses:

- UDP:        `ubxlib.redirectme.net:5050`
- TCP:        `ubxlib.redirectme.net:5055`
- Secure TCP: `ubxlib.redirectme.net:5065`

Note: used to use port 5060 for secure TCP but that port is commonly used by non-secure SIP and hence can be blocked by firewalls which want to exclude SIP, so port 5065 is now used instead.

# Installation
The [README.md](https://github.com/aws/amazon-freertos/tree/main/tools/echo_server#readme) at the above link was used to install TCP and secure TCP versions of the echo server.  The certificates generated for the secure TCP echo server can be found in the [certs](certs) directory.  Then [echo-server.go](echo-server.go) was copied and adapted to form [echo-server-udp.go](echo-server-udp.go).

- Make sure that `golang` is installed on your Linux server.
- Copy this directory to a directory on your Linux server.
- `cd` to that directory and run:
```
go build echo_server.go
go build echo_server_udp.go
```
- To just run all three echo servers manually, execute `sh ./echo_server.sh` (see note below if you get strange errors).
- To start the echo servers as a service at boot, kill the processes that started running as a result of the above line (`ps aux` and `kill xxx` where `xxx` is the `PID`), modify the paths in the file [echo_server.service](echo_server.service) appropriately, copy [echo_server.service](echo_server.service) to `/etc/systemd/system` and then:
```
sudo chmod u+x echo_server.sh
sudo systemctl daemon-reload
sudo systemctl start echo_server
sudo systemctl enable echo_server
```
- To test that it is working (or at least the TCP flavour is), open PuTTY or similar and connect a RAW socket to the server on port 5055 (for the TCP echo server): what you type in the PuTTY terminal should be echo'ed back to you (on a line-buffered basis).

Note: if you have FTP'ed `echo_server.sh` or `echo_server.service` across to your echo server from Windows they may well have the wrong line endings and strange things may happen; to give them the correct line endings, open the file in `nano`, press `CTRL-O` to write the file and then, before actually writing it, press `ALT-D` to switch to native Linux format and press \<enter\> to save the file.