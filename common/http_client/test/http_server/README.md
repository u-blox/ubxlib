# Introduction
This folder contains the source code for the `go` based HTTP test server, used when testing `ubxlib` HTTP stuff.

Two HTTP test servers using this `go` code are running on a publicly accessible server `ubxlib.it-sgn.u-blox.com`:

- HTTP:   `ubxlib.it-sgn.u-blox.com:8080`
- HTTPS:  `ubxlib.it-sgn.u-blox.com:8081`

The HTTPS server uses the same certificates as are used for the [MQTT client](/common/mqtt_client/test/mqtt_broker/certs).

# Installation
Make sure that `golang` is installed on your Linux server then copy this directory to a directory on your Linux server.  `cd` to that directory and run:
```
go build http_server.go
```
- To just run both HTTP test servers manually, execute `sudo chmod u+x http_server.sh` and then `sh ./http_server.sh`, modifying the paths to the certificate and key files as appropriate before you do so; see note below if you get strange errors.
- To start the HTTP test servers as a service at boot, kill the processes that started running as a result of the above line (`ps aux` and `kill xxx` where `xxx` is the `PID`), modify the paths in the file [http_server.service](http_server.service) appropriately, copy [http_server.service](http_server.service) to `/etc/systemd/system` and then:
```
sudo systemctl daemon-reload
sudo systemctl start http_server
sudo systemctl enable http_server
```

Note: if you have FTP'ed `http_server.sh` or `http_server.service` across to your echo server from Windows they may well have the wrong line endings and strange things may happen; to give them the correct line endings, open the file in `nano`, press `CTRL-O` to write the file and then, before actually writing it, press `ALT-D` to switch to native Linux format and press \<enter\> to save the file.

- To check that the HTTP test server is working, from a different computer install [curl](https://curl.se/download.html) and run it as follows:
```
curl -d hello_world http://server_url:8080/temp.html
```

...with `server_url` replaced with the URL of your server.  On the server you should see something like:
```
Received HTTP request type "POST", path "/temp.html".
Attempting to write file "data/temp.html".
```

- Repeat the above for the HTTPS case by running [curl](https://curl.se/download.html) as follows (the `-k` to stop certificate validation since, assuming you are using the certificates from the [MQTT client](/common/mqtt_client/test/mqtt_broker/certs), these are deliberately not chained to a trusted certificate authority):
```
curl -k -d hello_world https://server_url:8081/temp.html
```
The server-side should show the same response as it did for the HTTP case.

If the above does not work it is worth running the same `curl` command on the server itself, with `server_url` changed to `localhost`; if that works you know that the issue is with firewall access to your server for an inbound TCP connection on the given ports.

To see how the HTTP test server is behaving once it is installed as a service:

- Follow live logging: `sudo journalctl -u http_server.service -f`
- Display the last 100 lines and keep updating them: `sudo journalctl -u http_server.service -f -n 100`
