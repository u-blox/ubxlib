#!/bin/bash

# Run the HTTP servers
./http_server -dir data -port 8080 &
./http_server -dir data -port 8081 -cert_file ../../../mqtt_client/test/mqtt_broker/certs/server_cert.pem -key_file ../../../mqtt_client/test/mqtt_broker/certs/server_key.pem &
