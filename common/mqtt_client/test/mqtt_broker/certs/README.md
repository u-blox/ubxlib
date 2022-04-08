These files were generated mostly by following these instructions: https://mosquitto.org/man/mosquitto-tls-7.html and using the passphrase `ubxlib`, the country `GB`, company name `u-blox` and CN `ubxlib.it-sgn-u-blox.com`, specifically:

```
openssl req -new -x509 -extensions v3_ca -keyout ca_key.pem -out ca_cert.pem
openssl genrsa -out server_key.pem 2048
openssl req -out server_csr.pem -key server_key.pem -new
openssl x509 -req -in server_csr.pem -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out server_cert.pem
```