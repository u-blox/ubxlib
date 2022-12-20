# Introduction
This folder contains files relevant to installing a test MQTT broker using [mosquitto](https://mosquitto.org/) on a \[Linux\] server.  Also included are instructions for installation of the [Paho MQTT-SN Gateway](https://github.com/eclipse/paho.mqtt-sn.embedded-c/tree/master/MQTTSNGateway) which allows MQTT-SN connections to the same broker.

# MQTT
## Installation
Follow the installation instructions at https://mosquitto.org/ to install `mosquitto` on your server.  With that done, copy this directory (with the `certs` sub-directory) to your home directory on the server and edit [mosquitto.conf](mosquitto.conf) to set the absolute paths to the certificate/key files appropriate for your home directory.  Edit the file `/lib/systemd/system/mosquitto.service` so that the line:

```
ExecStart=/usr/sbin/mosquitto -c /etc/mosquitto/mosquitto.conf
 ```

...becomes something like:

```
ExecStart=/usr/sbin/mosquitto -c <absolute path to your home directory>/mqtt_broker/mosquitto.conf
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

The text `hello!` should appear in the first window.  If that doesn't work then on the server you can try `sudo netstat -nlp` to see what processes are listening on which ports and, for Ubuntu, `sudo ufw status` to check that ports 1883 and 8883 are open for incoming traffic (both UDP and TCP).  Check what the `mosquitto` application itself is up to in real-time with `sudo journalctl -u mosquitto.service -f -n 100`.


To check that a secure connection works, repeat the above but this time specifying port 8883 and including the path to [certs/ca_cert.pem](certs/ca_cert.pem), i.e. run:

```
mosquitto_sub -h <address of your server> -p 8883 --cafile <path to certs/ca_cert.pem> -t test_topic -i client1
```

In another command window on your local computer, run:

```
mosquitto_pub -h <address of your server> -p 8883 --cafile <path to certs/ca_cert.pem> -t test_topic -i client2 -m hello!
```

The text `hello!` should appear in the first window.  If you get the marvellously helpful `a TLS error occurred`, check the `mosquitto` log on the server to see what it is objecting to.  You can obtain the certificate the server is offering with `openssl s_client -showcerts -connect <address of your server>:8883`.  If you've just changed the certificate and this shows the old ones then you probably forgot to restart `mosquitto`.  If you are just getting `Socket error` out of `mosquitto` then you might `sudo apt install tshark` and grab a log of what's going on with something like `sudo tshark -i eth0 -w /tmp/log.pcap` that you can view in [wireshark](https://www.wireshark.org/).

## Start at Boot
With the testing done, enable the `mosquitto` service to start at boot with:

```
sudo systemctl enable mosquitto
```

# MQTT-SN
The Paho MQTT-SN Gateway is a separate service which behaves like an MQTT-SN broker but in fact is a relay to an MQTT broker.  With the configuration files here it listens on port 1883 for UDP connections and relays MQTT traffic to the `mosquitto` MQTT broker, as installed above, on the same \[Linux\] server.

## Installation
Clone the [Paho MQTT-SN repo](https://github.com/eclipse/paho.mqtt-sn.embedded-c.git).  You will also need CMake installed (e.g. `sudo apt install cmake`).

Build it as below:

```
cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway
./build.sh udp
```

Note: if it complains about being unable to find SSL header files, make sure you have the SSL development libraries installed for your Linux distribution.

## Test
Execute the MQTTSN Gateway, pointing it at the configuration file, e.g.:

```
bin/MQTT-SNGateway -f ~/mqtt_broker/gateway.conf
```

Install https://github.com/njh/mqtt-sn-tools on an\[other\] Linux machine.  In a terminal on that computer, run:

```
./mqtt-sn-sub -h <address of your server> -t test_topic -i client1
```

In another terminal on that computer, run:

```
./mqtt-sn-pub -h <address of your server> -t test_topic -i client2 -m hello!
```

The text `hello!` should appear in the first window.  If it does not, try the debugging pattern as for the MQTT case.  Note that there seems to be a considerable (i.e. up 15 minute) lag in the Paho MQTT-SN Gateway logs reaching the Linux system log so, for that case, don't look at the Linux system log timestamps, look at the timestamps in the Paho MQTT-SN Gateway log messages themselves.

## Start at Boot
With the testing done, enable the MQTT-SN gateway service to run at boot by copying the file [mqttsn_gateway.service](mqttsn_gateway.service) to the `/etc/systemd/system` directory on your \[Linux\] server, editing the absolute paths in those files as appropriate for the location of the binary and the `.conf` files as above (`clients.conf` and `predefinedTopic.conf` don't do anything but have to be present for the application to execute), and then running:

```
sudo systemctl daemon-reload
sudo systemctl start mqttsn_gateway
sudo systemctl enable mqttsn_gateway
```