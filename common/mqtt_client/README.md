# Introduction
This directory contains the API for the MQTT client implementation inside u-blox modules.

# Usage
The [api](api) directory defines the MQTT client API.  The [test](test) directory contains tests for that API that can be run on any platform.

NOTES: For short range modules, uMqttClientConnect() API does not really connect to broker, The real connection to the broker happens only when the user invokes uMqttClientPublish() or uMqttClientSubscribe() after calling uMqttClientConnect()

uMqttClientGetLastErrorCode() API is not implemented for short range modules.

Retrieving the QoS of received message is not supported by uMqttClientMessageRead() API for short range modules.
