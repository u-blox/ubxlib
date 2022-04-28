# Introduction
This folder contains files relevant to installing a test MQTT broker using [mosquitto](https://mosquitto.org/) on a \[Linux\] server.  Also included are instructions for installation of the [Paho MQTT-SN Gateway](https://github.com/eclipse/paho.mqtt-sn.embedded-c/tree/master/MQTTSNGateway) which allows MQTT-SN connections to the same broker.

# MQTT
## Installation
Follow the installation instructions at https://mosquitto.org/ to install `mosquitto` on your server.  With that done, create a directory named something like `mosquitto` off your home directory on the server and copy into it the [mosquitto.conf](mosquitto.conf) file, editing the absolute paths to the certificate/key files as appropriate for your home directory, and also copy the whole [cert](cert) directory.  Edit the file `/lib/systemd/system/mosquitto.service` so that the line:

```
ExecStart=/usr/sbin/mosquitto -c /etc/mosquitto/mosquitto.conf
 ```

...becomes something like:

```
ExecStart=/usr/sbin/mosquitto -c <absolute path to your home directory>/mosquitto/mosquitto.conf
```

...then restart the `mosquitto` service with:

```
sudo systemctl daemon-reload
sudo systemctl restart mosquitto
```

## Test
In order to check that the MQTT broker is working, also download [Mosquito MQTT](https://mosquitto.org/download/) to your local computer (not the server).  In a command window on your local computer, run:

```
mosquitto_sub -h <address of your server> -t test_topic -i client1
```

In another command window on your local computer, run:

```
mosquitto_pub -h <address of your server> -t test_topic -i client2 -m hello!
```

The text `hello!` should appear in the first window.  To check that a secure connection works, repeat the above but this time specifying port 8883 and including the path to [certs/ca_cert.pem](certs\ca_cert.pem), i.e. run:

```
mosquitto_sub -h <address of your server> -p 8883 --cafile <path to certs/ca_cert.pem> -t test_topic -i client1
```

In another command window on your local computer, run:

```
mosquitto_pub -h <address of your server> -p 8883 --cafile <path to certs/ca_cert.pem> -t test_topic -i client2 -m hello!
```

The text `hello!` should appear in the first window.

## Start at Boot
With the testing done, enable the `mosquitto` service to start at boot with:

```
sudo systemctl enable mosquitto
```

# MQTT-SN
The Paho MQTT-SN Gateway is a separate service which behaves like an MQTT-SN broker but in fact is a relay to an MQTT broker.  With the configuration files here it listens on ports 1885/8885 for UDP/DTLS connections, uses the server certificate/key pair in the [cert](cert) sub-directory and relays MQTT traffic to the `mosquitto` MQTT broker, as installed above, on the same \[Linux\] server.

NOTE: with the [most recent version](https://github.com/eclipse/paho.mqtt-sn.embedded-c/commit/59797127e7f3d024de576555cd4232c68e874ac6) of the gateway the connection to the broker fails if a "will" has been set, hence, for non-secure testing, we use [an older version](https://github.com/eclipse/paho.mqtt-sn.embedded-c/commit/c9e807da319bd68bff71cc4817ea5a4f4b25f49d) so that we can test using "will"s, only using the most recent version for DTLS testing (which isn't supported by the older version).  The instructions below are for the most recent version; for the older one follow the instructions at the link as they are different.

NOTE ON THE NOTE: it seems that there's an issue somewhere in the UDP version where it works once but then has to be restarted to work a second time.  Hence we only actually use the [older version](https://github.com/eclipse/paho.mqtt-sn.embedded-c/commit/c9e807da319bd68bff71cc4817ea5a4f4b25f49d) and a non-secure connection in regression testing.

## Installation
Follow the installation instructions at https://github.com/eclipse/paho.mqtt-sn.embedded-c/tree/master/MQTTSNGateway to install and build the Gateway part of Paho MQTT-SN on the same \[Linux\] server as above.

Build it first for `udp`, i.e.:

```
cd paho.mqtt-sn.embedded-c/MQTTSNGateway
./build udp
```

Note: if it complains about being unable to find SSL header files, make sure you have the SSL development libraries installed for your Linux distribution.

Then rename the `bin` directory to `bin.udp` and build it once more for `dtls`:

```
./build dtls
```

...renaming the resulting `bin` directory to `bin.dtls` afterwards.

Copy the files [gateway.conf](gateway.conf), [gateway_dtls.conf](gateway_dtls.conf), [clients.conf](clients.conf) and [predefinedTopic.conf](predefinedTopic.conf) to the `mosquitto` directory off your home directory on the server (as created for MQTT above), modifying the absolute paths of the certificate/key files in [gateway_dtls.conf](gateway_dtls.conf) as appropriate to point at the same [certs/server_cert.pem](certs/server_cert.pem)/[certs/server_key.pem](certs/server_key.pem) files as were already copied onto the server for MQTT above.

## Test
For a manual test, execute the UDP binary, pointing it at the configuration file, e.g.:

```
./bin.udp/MQTT-SNGateway -f <path to gateway.conf>
```

Install https://github.com/njh/mqtt-sn-tools on an\[other\] Linux machine.  In a terminal on that computer, run:

```
./mqtt-sn-sub -h <address of your server> -t test_topic -i client1
```

In another terminal on that computer, run:

```
./mqtt-sn-pub -h <address of your server> -p 1885 -t test_topic -i client2 -m hello!
```

The text `hello!` should appear in the first window.  We couldn't find a PC-based MQTT-SN client that supports DTLS but, if you could find one, you would now stop the UDP binary on the server and instead start the DTLS binary:

```
./bin.dtls/MQTT-SNGateway -f <path to gateway.dtls>
```

We ran our actual DTLS target code at this point and that worked but, as noted above, only once; the Paho MQTT-SN Gateway stopped logging anything and became unresponsive to connection attempts after the first session, requiring it to be restarted.

## Start at Boot
With the testing done, enable both of the MQTT-SN gateway services to run at boot by copying the files [mqttsn_gateway.service](mqttsn_gateway.service) and [mqttsn_gateway_dtls.service](mqttsn_gateway_dtls.service) to the `/etc/systemd/system` directory on your \[Linux\] server, editing the absolute paths in those files as appropriate for the location of the binary files and `.conf` files as above, and then running:

```
sudo systemctl daemon-reload
sudo systemctl start mqttsn_gateway
sudo systemctl start mqttsn_gateway_dtls
sudo systemctl enable mqttsn_gateway
sudo systemctl enable mqttsn_gateway_dtls
```

# Reading Logs
Both `mosquitto` and the Paho MQTT-SN Gateway log copiously, though note that there seems to be a considerable (i.e. up 15 minute) lag in the Paho MQTT-SN Gateway logs reaching the Linux system log so, for that case, don't look at the Linux system log timestamps, look at the timestamps in the Paho MQTT-SN Gateway log messages themselves.  Useful commands for viewing the logs are:

- Follow live logging from the `mosquitto` service: `sudo journalctl -u mosquitto.service -f`
- Display the last 100 lines from the `mqttsn_gateway` service and keep updating them: `sudo journalctl -u mqttsn_gateway.service -f -n 100`