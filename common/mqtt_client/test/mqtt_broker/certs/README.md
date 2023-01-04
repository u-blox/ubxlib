These files were generated mostly by following these instructions: https://mosquitto.org/man/mosquitto-tls-7.html and using the passphrase `ubxlib`, the country `GB`, company name `u-blox`, section name `ca` for the CA certificate and just blank for the server certificate (the two certificates _must_ be different or SSL will fail the connection) and CN `ubxlib.redirectme.net` (all other fields left at defaults by just pressing \<enter\>), specifically:

```
openssl req -new -x509 -extensions v3_ca -keyout ca_key.pem -out ca_cert.pem
openssl genrsa -out server_key.pem 2048
openssl req -out server_csr.pem -key server_key.pem -new
openssl x509 -req -in server_csr.pem -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out server_cert.pem
```

\[You can delete the CSR file afterwards\].

And don't forget: [ca_cert.pem](ca_cert.pem) is pasted into [wifi_mqtt_test.c](/wifi/test/wifi_mqtt_test.c) so if you change it here you need to change what is pasted-in there to match.