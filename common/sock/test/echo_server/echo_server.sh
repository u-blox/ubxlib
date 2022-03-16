#!/bin/bash

# Run the echo servers
./echo_server -config config.json >/dev/null 2>&1 &
./echo_server -config config_secure.json >/dev/null 2>&1 &
./echo_server_udp -config config_udp.json >/dev/null 2>&1 &
