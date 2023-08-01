# Introduction
The [badly named] echo server provides not just UDP/TCP echo to the test system but also TCP echo with SSL, an MQTT/MQTTSN test peer and an HTTP/HTTPS test peer.  In addition to this, for the case where the test system is behind a firewall, it provides the gateway into the test system.  The server is visible on the internet as `ubxlib.com`, public IP address `18.133.144.142`.

# Management
The echo server is an EC2 instance under a `ubxlib` account set up inside the u-blox AWS world.  From a machine that has the AWS tools installed, you should be able to log into a console from any command-prompt with:

```
chcp 65001
aws ssm start-session --target <instance ID>
sudo su - <username>
```

The first line is ONLY REQUIRED ON WINDOWS: it switches to the correct code-page (UTF-8) so that line-draw characters etc. are displayed correctly; useful if you are looking at `systemctl status` or some such.

# Test Peers
Install each of the [sockets](/common/sock/test/echo_server), [MQTT/MQTTSN](/common/mqtt_client/test/mqtt_broker) and [HTTP/HTTPS](/common/http_client/test/http_server) test peers according to their instructions, ensuring that they start at boot.

Ensure that each of the required ports are opened in the applicable security group (e.g. `ubxlib-public`) for in-bound traffic of the appropriate type, from any source IP address, and of course also in `ufw` should that be enabled (`sudo ufw status` to find out if it is).

# Gateway
If the test system is behind a firewall, follow the steps in [ACCESS.md Tunneling](ACCESS.md#tunneling) to use this server as your gateway.