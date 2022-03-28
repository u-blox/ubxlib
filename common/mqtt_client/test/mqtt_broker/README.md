# Introduction
This folder contains files relevant to installing a test MQTT broker: [HMQ](https://github.com/fhmq/hmq), which is `golang` based, in order to be similar to the `golang` echo server used for [sockets testing](../../../sock/test/echo_server), on a \[Linux\] server.

# Installation
Follow the installation instructions at https://github.com/fhmq/hmq to install and build HMQ on your server.

Copy the file [hmq.config](hmq.config) to your server to act as the HMQ configuration file.

The same certificates as are used by the [sockets echo server](../../../sock/test/echo_server) are used by the MQTT broker and [hmq.config](hmq.config) is written assuming that they are installed in an `echo_server/certs` directory off your home directory: you will need to edit the paths in [hmq.config](hmq.config) appropriately.

`CD` to the location of the `hmq` binary file and start the HMQ MQTT broker manually with the following command-line:

```
./hmq --config <path to hmq.config>
```

In order to check that the MQTT broker is working, download [Mosquito MQTT](https://mosquitto.org/download/) to your local computer (not the server).

In a command window on your local computer, run:

```
mosquitto_sub -h <address of your server> -t test_topic -i client1
```

In another command window on your local computer, run:

```
mosquitto_pub -h <address of your server> -t test_topic -i client2 -m hello!
```

The text `hello!` should appear in the first window.

To start the MQTT broker as a service at boot, modify the paths in the file [mqtt_broker.service](mqtt_broker.service) appropriately, copy [mqtt_broker.service](mqtt_broker.service) to `/etc/systemd/system` on the server and then:

```
sudo systemctl daemon-reload
sudo systemctl start mqtt_broker
sudo systemctl enable mqtt_broker
```