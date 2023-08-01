These files were generated mostly by following these instructions https://mosquitto.org/man/mosquitto-tls-7.html, specifically:

```
openssl req -new -x509 -days 36500 -extensions v3_ca -passout pass:ubxlib -keyout ca_key.pem -out ca_cert.pem -subj "/C=US/ST=WA/L=Thalwil/O=u-blox/OU=ca/CN=ubxlib.com/emailAddress=ubxlib@u-blox.com"
openssl genrsa -out server_key.pem 2048
openssl req -key server_key.pem -out server_csr.pem -subj "/C=US/ST=WA/L=Thalwil/O=u-blox/OU=server/CN=ubxlib.com/emailAddress=ubxlib@u-blox.com" -new
openssl x509 -req -in server_csr.pem -passin pass:ubxlib -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out server_cert.pem
```

\[You can delete the CSR file afterwards\].

And don't forget: [ca_cert.pem](ca_cert.pem) is pasted into [u_wifi_mqtt_test.c](/wifi/test/u_wifi_mqtt_test.c) so if you change it here you need to change what is pasted-in there to match.