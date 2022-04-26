# Introduction
This example is a test of a UART port. It's not a unit test like an application, but it verifies the functionality of the UART port. The application is on top of ubxlib, which uses the uart port to communicate with the modem. This is a complete vertical test to test the ubxlib, uart port, ucpu sdk, and the latest firmware.

# Example
This is a multi-threaded example for testing the functionality of the UART. The application creates two threads, one of which performs MQTT publish/receive and the other do socket send/receive. The application first bring up network and then start the threads.
The MQTT thread creates an MQTT client instance and connects to the AWS IoT broker. Subscribe to a predefined topic. Then perform the MQTT publish and receive data in an infinite loop.
The socket thread creates a TCP socket and connects to the TCP echo server. Then, send and receive data in an infinite loop.
